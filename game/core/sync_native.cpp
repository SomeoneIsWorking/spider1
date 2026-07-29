// sync_native.cpp — Spider-Man's PSX hardware-sync primitives, implemented natively.
//
// WHY THIS FILE EXISTS
// -------------------
// A PSX game's libetc/libcd sync leaves busy-spin on counters that a hardware IRQ advances. A
// PC-native runtime has no vblank IRQ, so those loops never terminate — the guest hangs forever. The
// framework's answer is PlatformHle: a per-Game table mapping a BIOS-library address to a native
// handler that reproduces the primitive's OBSERVABLE RESULT without the spin.
//
// This file holds the primitives that have no generic form — ones this game implements ITSELF, as
// opposed to the framework's generic handlers, which initBuiltins() installs at whatever addresses
// GameConfig::hle declares.
//
// (Historical note worth keeping: until psxport 7c212eb5 the framework's initBuiltins() carried
// Tomba!2's addresses as baked-in literals, which for this game would have missed every primitive it
// actually uses AND hooked unrelated Spider-Man functions sharing those addresses. This port briefly
// worked around that by skipping initBuiltins entirely; the framework now ships no game addresses at
// all, so the workaround is gone and main.cpp calls it normally.)
//
// register_() is a public framework seam, gated on the game's declared BIOS-library window — see
// GameConfig::hle.windowLo/windowHi in game_config.cpp for how that range was established.
#include "core.h"
#include "game.h"
#include "cfg.h"
#include "game_iface.h"
#include "recomp_iface.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// VSync — SLUS_008.75 0x80084BE0. libetc's VSync(int mode).
//
// Identification is ground truth, not inference: the wait helper it calls (0x80084D58, its only
// caller) emits the string at 0x80096020, which reads "VSync: timeout". Reproduce with
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x80084BE0 0x80084D58
//
// The guest state it touches (all read through the disassembly, none guessed):
//   0x800B397C  the VBLANK COUNTER, advanced by the vblank ISR. This is what 0x80084D58 spins on:
//               `while (*0x800B397C < target)` with a deadline of (timeout << 15) iterations. On a
//               runtime with no vblank IRQ this counter never moves — THE hang.
//   0x800B0FA0  pointer to the GPU status register (read for the field/idle bit transition)
//   0x800B0FA4  pointer to the h-counter, read twice until two reads agree (stable read)
//   0x800B0FA8  baseline h-counter value, refreshed at the end of every completed VSync
//   0x800B0FAC  the vblank-counter value at the last completed VSync
//
// Control flow (0x80084BE0):
//   entry:      ret = (stable_read(*0x800B0FA4) - *0x800B0FA8) & 0xFFFF
//   mode <  0:  return *0x800B397C                      (query the vblank counter; no wait)
//   mode == 1:  return ret                              (query the h-delta;      no wait)
//   mode == 0:  target = *0x800B0FAC
//   mode >  1:  target = *0x800B0FAC + mode - 1
//   then:       wait until *0x800B397C >= target, and then for ONE MORE vblank (the second
//               _vsync_wait call at 0x80084CB4 passes counter+1) — so a waiting VSync always
//               advances by at least one vblank.
//   tail:       *0x800B0FAC = *0x800B397C ; *0x800B0FA8 = stable_read(*0x800B0FA4) ; return ret
//
// NATIVE MODEL: on this runtime a vblank IS a presented frame. Waiting for N vblanks means
// presenting N frames and pacing them in real time, then advancing the counter the ISR would have
// advanced. The guest's own arithmetic (the tail stores, the return value) is reproduced exactly, so
// callers observe what they would on hardware.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t kVSync        = 0x80084BE0u;
static constexpr uint32_t kVblankCount  = 0x800B397Cu;

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// VSyncCallback — the registrar, and why the port must invoke what it registers.
//
//   8008B8CC  addiu $sp,$sp,-0x18
//   8008B8D0  lui   $v0,0x800b
//   8008B8D4  lw    $v0,0x390c($v0)     ; libapi's hook-installer table (*0x800B390C)
//   8008B8D8  move  $a1,$a0             ; a1 = the callback the caller passed
//   8008B8E0  lw    $v0,0x14($v0)       ; table[+0x14] = the installer
//   8008B8E8  jalr  $v0
//   8008B8EC  addiu $a0,$zero,4         ; a0 = 4 (selector), in the delay slot
//
//   python3 tools/redump_ram.py
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8008B8CC 0x8008B900
//
// The selector is hardcoded to 4, so this wrapper is specifically VSyncCallback and hooking it
// captures only per-vblank registrations — narrower, and therefore safer, than hooking the shared
// installer the table points at.
//
// WHAT THE CALLBACK ACTUALLY DOES, and why the port must not shortcut it. The game registers
// 0x8005E510. Its first act is `gp+0xC74 += 1`, and with gp = 0x800B47F4 that address is 0x800B5468
// — exactly the counter the boot's title-wait loop in 0x8006BF9C tests against +300. It is very
// tempting to conclude the host "already knows" that value and to bump it natively. That would be a
// bandaid: the same routine also calls 0x800646AC, conditionally patches GPU packets, maintains a
// 5-slot rotation on an 8-vblank period, and conditionally calls 0x8005E234. Fabricating the counter
// would skip all of it and leave the game running on a lie. The correct unit of work is to dispatch
// the callback the guest registered — the port owns WHEN a field happens, the guest still owns what
// happens on one.
//
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8005E510 0x8005E5A0
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t kVSyncCallback = 0x8008B8CCu;

// NTSC field rate in milli-hertz: 60000/1001 Hz. Same hardware fact the counter derivation below
// uses — stated once, in the units rec_host_turn_register wants.
static constexpr unsigned kFieldRateMilliHz = 59940u;

// The callback the guest registered, or 0 if it has not called VSyncCallback yet.
static uint32_t s_vsync_cb = 0;

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// FUN_8005E748(n) — "wait n display fields", the game's own delay primitive (15 call sites).
//
//   8005E748  lw    $v1, 0xc74($gp)      ; v1 = counter
//   8005E74C  lw    $v0, 0xc74($gp)
//   8005E750  addu  $v1, $v1, $a0        ; v1 = TARGET, computed ONCE
//   8005E754  sltu  $v0, $v0, $v1
//   8005E758  beqz  $v0, 0x8005E774      ; already past the target -> return
//   8005E760  lw    $v0, 0xc74($gp)      ;  <-- the spin
//   8005E768  sltu  $v0, $v0, $v1
//   8005E76C  bnez  $v0, 0x8005E760
//
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8005E748 0x8005E780
//
// (Ghidra renders this as `while (c < c + n)` — an apparent infinite loop — because it re-reads the
// counter on both sides. The disassembly shows the target is a snapshot. Read the instructions.)
//
// WHY IT MUST BE NATIVE. The spin at 0x8005E760..0x8005E76C calls NOTHING. The framework's host turn
// is taken at recompiled-FUNCTION ENTRY, so a loop that never enters a function starves it: the
// guest waits on a counter that only a host turn can advance, and no host turn can happen. Measured:
// the counter still advanced 412 times over 20 s (progress between waits) instead of the ~1200 the
// field rate implies, and the run wedged the moment it entered a wait that had not yet expired.
//
// So this is HLE'd at the same level libetc's VSync is, and for the same reason: it is a WAIT on the
// field clock, and the port owns the field clock. The loop below is the identical shape to the
// blocking-VSync path further down — pace, advance, re-check — so both waits share one clock.
//
// KNOWN GENERAL LIMITATION, recorded rather than papered over: any OTHER tight guest spin loop with
// no calls in it would starve the host the same way. The general fix is to emit the gate on loop
// back-edges too, not just function entry. That is a real change to the recompiler with a real cost
// on every loop, and it is not justified by one wait primitive — but if a second such loop turns up,
// take it rather than adding a second special case. See docs/re-frontier.md RE-10.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t kVblankWait      = 0x8005E748u;
static constexpr uint32_t kGameVblankCount = 0x800B5468u;   // gp+0xC74, bumped by the callback
static constexpr uint32_t kGpuStatPtr   = 0x800B0FA0u;
static constexpr uint32_t kHCounterPtr  = 0x800B0FA4u;
static constexpr uint32_t kHBaseline    = 0x800B0FA8u;
static constexpr uint32_t kLastSyncVbl  = 0x800B0FACu;

// The guest reads the h-counter through a pointer. Before libetc's own init has run that pointer can
// still be null; a null-pointer guest read is a real condition here, not an error to swallow, and
// reading 0 is what the hardware path would effectively see with no counter mapped.
static uint32_t h_counter(Core* c) {
  const uint32_t p = c->mem_r32(kHCounterPtr);
  return p ? c->mem_r32(p) : 0u;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The vblank counter is FREE-RUNNING, and that is a measured property of this game, not a choice.
//
// Instrumenting every VSync call over a 60 s boot (PSXPORT_DEBUG=vsync) gives 427,643 calls of
// VSync(-1) — the QUERY form, "return the vblank counter" — against exactly ONE blocking VSync(0),
// from graphics init at ra=0x8008479C. So Spider-Man never asks libetc to wait N fields. It polls the
// counter in a tight loop and does its own timing against it, which is why the game declares no fixed
// frame rate anywhere in the executable.
//
// The consequence for this handler is structural: a counter advanced only inside BLOCKING VSync calls
// never moves for a caller that only ever polls, so the poll loop spins forever. On hardware the
// vblank ISR advances the counter on the field clock no matter what the game is doing. So the native
// model must be the same — derive the counter from elapsed real time at the NTSC field rate — and a
// vblank crossing is where a frame gets presented.
//
// NTSC field rate, exactly: 60000/1001 Hz ≈ 59.94. This is a US release (SLUS_008.75).
// ─────────────────────────────────────────────────────────────────────────────────────────────────
#include <chrono>

// A vblank crossing is one presented frame. If the process is descheduled (or a loader blocks the
// guest for a long stretch) the elapsed time can cover many fields; presenting all of them would
// stall in a catch-up burst. Cap the catch-up and let the counter jump — a dropped FRAME is correct
// under load; a dropped COUNT would corrupt the guest's own timing arithmetic.
static constexpr uint32_t kMaxPresentCatchup = 4;

static void vblank_advance(Core* c) {
  using clock = std::chrono::steady_clock;
  static const clock::time_point t0 = clock::now();
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
  // fields = elapsed_ns * (60000/1001) / 1e9, in integer arithmetic.
  const uint32_t fields = (uint32_t)((unsigned long long)ns * 60000ull / 1001ull / 1000000000ull);

  const uint32_t cur = c->mem_r32(kVblankCount);
  if (fields <= cur) return;
  uint32_t present = fields - cur;
  if (present > kMaxPresentCatchup) present = kMaxPresentCatchup;


  // `PSXPORT_DEBUG=presentwatch` — does presenting write GUEST RAM? It must not: psxport's porting
  // guide makes the render overlay read-only with respect to guest memory, because a guest write
  // from render corrupts live state (STALL-04: it destroyed a saved callee-saved register on the
  // guest stack). This snapshots a window around the CURRENT guest stack pointer across each
  // present and names every word that changed, so the answer is addresses rather than suspicion.
  const bool watch = cfg_dbg("presentwatch");
  const uint32_t win_lo = (c->r[29] & ~3u) - 0x100u;   // a little below sp (callee frames)
  const uint32_t kWords = 0x180 / 4;                   // and well above it (live caller frames)
  uint32_t snap[kWords];
  for (uint32_t i = 0; watch && i < kWords; ++i) snap[i] = c->mem_r32(win_lo + i * 4);

  if (watch) cfg_logf("presentwatch", "arm: presenting %u frame(s), window [%08X..%08X) sp=%08X",
                      present, win_lo, win_lo + kWords * 4, c->r[29]);
  for (uint32_t i = 0; i < present; ++i) {
    gpu_present(c);
    if (!watch) continue;
    for (uint32_t w = 0; w < kWords; ++w) {
      const uint32_t now = c->mem_r32(win_lo + w * 4);
      if (now != snap[w]) {
        cfg_logf("presentwatch", "present WROTE guest RAM [%08X] %08X -> %08X (sp=%08X)",
                 win_lo + w * 4, snap[w], now, c->r[29]);
        snap[w] = now;
      }
    }
  }
  c->mem_w32(kVblankCount, fields);

  // Announce each elapsed field to the guest by invoking the callback it registered. This is the
  // ISR's OTHER job, and the one that was missing: advancing the counter told the guest that time
  // had passed, but the game's own per-vblank work (and its own counter, 0x800B5468) only happens
  // inside the callback.
  //
  // Once per OWED field, not once per turn. Hardware would coalesce — the VBlank I_STAT bit is a
  // latch, so a guest with interrupts masked across N fields gets one ISR pass — but this game
  // derives its timing FROM the callback's own counter, so coalescing would make that counter report
  // less time than really elapsed and every animation paced by it would run slow under load. The
  // callback is cheap; dispatching each field keeps the guest's clock honest. Recorded as a
  // deliberate policy choice, not an oversight, in case a heavyweight callback later argues for the
  // hardware behaviour instead.
  if (!s_vsync_cb) return;

  // Re-entrancy: the callback runs guest code, and guest code can call VSync, which lands back here.
  static bool in_cb = false;
  if (in_cb) return;

  uint32_t owed = fields - cur;
  // A very large backlog means the process was descheduled or a native step blocked for a long time.
  // Dispatching thousands of callbacks would stall far worse than the gap itself. Cap it, and SAY
  // so — a silent cap here would look exactly like the game's timing quietly drifting.
  static constexpr uint32_t kMaxCallbackCatchup = 16;
  if (owed > kMaxCallbackCatchup) {
    cfg_logw("vsyncsb", "%u field(s) owed at once — dispatching %u and dropping the rest. The "
                        "guest's per-vblank counter will lag real time by the difference.",
             owed, kMaxCallbackCatchup);
    owed = kMaxCallbackCatchup;
  }

  in_cb = true;
  for (uint32_t i = 0; i < owed; ++i) {
    const R3000 saved = *static_cast<R3000*>(c);
    rec_dispatch(c, s_vsync_cb);
    *static_cast<R3000*>(c) = saved;
  }
  in_cb = false;
}

// 0x8008B8CC VSyncCallback(fn) — capture the pointer; the port invokes it on the field clock.
static void spiderman_vsync_callback(Core* c) {
  const uint32_t fn = c->r[4];
  if (fn != s_vsync_cb)
    cfg_logi("sync", "VSyncCallback: guest registered 0x%08X (was 0x%08X)", fn, s_vsync_cb);
  s_vsync_cb = fn;
  c->r[2] = 0;   // the guest wrapper's caller discards the return; 0 is the benign value
}

// FUN_8005E748(n) — wait n fields, natively. Same shape as the blocking-VSync path: pace, advance
// the field clock (which dispatches the guest's per-vblank callback, which is what actually bumps
// the counter being waited on), re-check.
static void spiderman_vblank_wait(Core* c) {
  const uint32_t n = c->r[4];
  const uint32_t target = c->mem_r32(kGameVblankCount) + n;

  // Nothing advances kGameVblankCount except the guest's registered per-vblank callback. If the game
  // has not registered one yet, this wait can never complete and spinning here would look exactly
  // like a hang with no cause. Say so instead — a wait that cannot finish is a real condition.
  if (!s_vsync_cb) {
    cfg_loge("sync", "FUN_8005E748(%u) called before the guest registered a VSyncCallback — nothing "
                     "can advance 0x%08X, so this wait could never complete. This is an RE gap, not "
                     "a stall: see docs/re-frontier.md RE-05.", n, kGameVblankCount);
    c->r[2] = 0;
    return;
  }

  // Signed comparison on the difference so the wait still terminates correctly across the counter's
  // 32-bit wrap, which the guest's own `sltu` against a snapshot does not survive either — but the
  // guest would take ~2.3 years of fields to get there, so matching its exact wrap behaviour is not
  // worth reproducing a bug for.
  while ((int32_t)(c->mem_r32(kGameVblankCount) - target) < 0) {
    gpu_pace_frame(c);
    vblank_advance(c);
  }
  c->r[2] = 0;
}

// One host turn: advance the field clock (which presents, and announces each field to the guest),
// then service input. Both were previously reachable ONLY from the native frame loop, which never
// runs while the guest executes a straight-line boot — the single cause behind both the frozen
// vblank counter and the never-filled pad buffer (docs/re-frontier.md RE-05).
static void spiderman_host_turn(Core* c) {
  vblank_advance(c);
  c->game->pad.serviceFrame();
}

static void spiderman_vsync(Core* c) {
  // `PSXPORT_DEBUG=vsyncregs` — does this handler preserve the guest's callee-saved registers?
  // It is an HLE standing in for a guest leaf, so its caller expects the guest ABI to hold across
  // it. But it presents frames, and presenting can run framework and guest code against the SAME
  // c->r[] array — so "native handler" does not imply "register-transparent".
  const uint32_t _s1_in = c->r[17], _s0_in = c->r[16], _s2_in = c->r[18];
  // Coverage-proven stack watch: the s1 probe publishes the exact slot it cares about, so this
  // brackets the ENTIRE VSync call (including everything presenting runs) around that one word.
  // Unlike a window around sp, it cannot silently miss the window that matters.
  extern uint32_t g_diag_stack_watch;
  extern unsigned g_diag_vsync_while_armed;
  const uint32_t _wa = g_diag_stack_watch;
  const uint32_t _wv = _wa ? c->mem_r32(_wa) : 0u;
  if (_wa) {
    ++g_diag_vsync_while_armed;   // proves coverage, so a silent watch is not mistaken for innocence
    // The callee saves s1 to this slot and THEN calls VSync, so the value visible here is the
    // saved one. This distinguishes "the save never landed" from "the save landed and was later
    // destroyed" — the two remaining explanations, which every earlier probe conflated.
    // sp here IS the callee's frame pointer: the emitted code saves s1 and then calls VSync with no
    // sp change between, so the effective save address is OBSERVED as sp+28 rather than derived from
    // the caller's sp and an assumed frame size. That derivation is what went wrong before.
    cfg_logf("stackwatch", "armed VSync #%u: callee sp=%08X -> save addr %08X = %08X | assumed %08X = %08X | s1 live=%08X",
             g_diag_vsync_while_armed, c->r[29], c->r[29] + 28u, c->mem_r32(c->r[29] + 28u),
             _wa, _wv, c->r[17]);
  }
  vblank_advance(c);   // the ISR's job: the counter tracks real time on EVERY call, query or wait

  const int32_t mode = (int32_t)c->r[4];
  const uint32_t ret = (h_counter(c) - c->mem_r32(kHBaseline)) & 0xFFFFu;

  // `PSXPORT_DEBUG=vsync` — THE instrument that answers "what frame rate does this game target?".
  // The question is not a matter of opinion or of what the port would like to run at: a blocking
  // VSync(mode) waits `mode` vblanks (mode 0/1 degenerate to one), so the mode the GAME passes at its
  // frame boundary IS its declared frame pacing on a 60 Hz field clock — 1 vblank = 60fps, 2 = 30fps,
  // 3 = 20fps. Logged with the caller's `ra` so a per-call-site histogram distinguishes the real
  // frame-boundary VSync from incidental waits inside loaders. See docs/info/instruments.md.
  if (cfg_dbg("vsync"))
    cfg_logf("vsync", "VSync(%d) ra=0x%08X vbl=%u", mode, c->r[31], c->mem_r32(kVblankCount));

  if (mode < 0)  { c->r[2] = c->mem_r32(kVblankCount); return; }   // query the vblank counter
  if (mode == 1) { c->r[2] = ret; return; }                        // query the h-delta

  const uint32_t last  = c->mem_r32(kLastSyncVbl);
  const uint32_t target = (mode > 1) ? last + (uint32_t)mode - 1u : last;

  // Blocking wait: reach `target`, and always at least one field past where we are (the guest's
  // second _vsync_wait(counter + 1) at 0x80084CB4). The counter is real-time-driven, so "waiting"
  // means pacing until real time has produced those fields — not incrementing a number ourselves.
  // gpu_pace_frame is the framework's frame-rate limiter; vblank_advance does the present + store.
  const uint32_t want = (target > c->mem_r32(kVblankCount) + 1u) ? target : c->mem_r32(kVblankCount) + 1u;
  while (c->mem_r32(kVblankCount) < want) {
    gpu_pace_frame(c);
    vblank_advance(c);
  }

  // The tail stores of 0x80084D04..0x80084D3C, reproduced verbatim.
  c->mem_w32(kLastSyncVbl, c->mem_r32(kVblankCount));
  c->mem_w32(kHBaseline, h_counter(c));
  (void)c->mem_r32(kGpuStatPtr);   // the guest's status read has no side effect on our GPU

  if (_wa) { const uint32_t now = c->mem_r32(_wa);
    if (now != _wv) cfg_logf("stackwatch", "VSync CORRUPTED guest stack [%08X] %08X -> %08X", _wa, _wv, now); }
  c->r[2] = ret;
  if (cfg_dbg("vsyncregs") && (c->r[17] != _s1_in || c->r[16] != _s0_in || c->r[18] != _s2_in))
    cfg_logf("vsyncregs", "CLOBBERED across VSync: s0 %08X->%08X  s1 %08X->%08X  s2 %08X->%08X",
             _s0_in, c->r[16], _s1_in, c->r[17], _s2_in, c->r[18]);
}

// Install this game's sync primitives. Called from main() INSTEAD of PlatformHle::initBuiltins().
void spiderman_install_sync_natives(Game* g) {
  g->platform_hle.register_(kVSync, spiderman_vsync);
  cfg_logi("sync", "native VSync installed at 0x%08X (libetc, RE-verified)", kVSync);
  g->platform_hle.register_(kVSyncCallback, spiderman_vsync_callback);
  // NOT platform_hle: that seam is gated to the declared BIOS-library window and REFUSES a game
  // address, deliberately — engine logic is owned through the recomp override table instead. The
  // refusal is a correct gate and was not worked around by widening the window.
  psxport_recomp()->shard_set_override(kVblankWait, spiderman_vblank_wait);
  cfg_logi("sync", "native field-wait installed at 0x%08X (game delay primitive, RE-verified)",
           kVblankWait);
  cfg_logi("sync", "native VSyncCallback installed at 0x%08X (libapi, RE-verified)", kVSyncCallback);
  // Give the host a turn at recompiled-function entry on the field clock. Without this the guest's
  // straight-line boot never yields, so neither the registered vblank callback nor the pad fill can
  // run — see host_turn.cpp for why the pacing here is a hardware fact and not a tunable.
  rec_host_turn_register(&g->core, spiderman_host_turn, kFieldRateMilliHz);
  // Not yet RE'd for this game, and so deliberately NOT registered: CdReadSync / CdDataSync / the
  // low-level CdInit handshake / the libgpu DMA-timeout pair / the cooperative task-switch funnel.
  // Each is an open step in docs/re-frontier.md. A caller that spins in one of them will hang, and
  // that hang is the honest signal that its RE has not been done — not something to paper over.
}
