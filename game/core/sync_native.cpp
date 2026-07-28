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
  for (uint32_t i = 0; i < present; ++i) gpu_present(c);
  c->mem_w32(kVblankCount, fields);
}

static void spiderman_vsync(Core* c) {
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

  c->r[2] = ret;
}

// Install this game's sync primitives. Called from main() INSTEAD of PlatformHle::initBuiltins().
void spiderman_install_sync_natives(Game* g) {
  g->platform_hle.register_(kVSync, spiderman_vsync);
  cfg_logi("sync", "native VSync installed at 0x%08X (libetc, RE-verified)", kVSync);
  // Not yet RE'd for this game, and so deliberately NOT registered: CdReadSync / CdDataSync / the
  // low-level CdInit handshake / the libgpu DMA-timeout pair / the cooperative task-switch funnel.
  // Each is an open step in docs/re-frontier.md. A caller that spins in one of them will hang, and
  // that hang is the honest signal that its RE has not been done — not something to paper over.
}
