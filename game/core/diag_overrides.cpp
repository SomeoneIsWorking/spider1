// diag_overrides.cpp — DIAGNOSTIC overrides: native handlers installed only to observe, never to
// change behaviour. Each logs, then runs the original recompiled body, so a run with the channel on
// executes exactly what a run with it off does.
//
// WHY THIS EXISTS RATHER THAN A BACKTRACE OR A REGISTER DUMP
// ---------------------------------------------------------
// Identifying "who called this guest function, and with what argument" is harder under a static
// recompiler than it looks, and BOTH of the obvious instruments mislead here (see
// docs/info/instruments.md INST-07):
//
//   * Guest `pc`/`ra` are not refreshed on static gen-to-gen calls, so they can name a function
//     nowhere near the site. Observed: a three-instruction getter appearing to issue a CD command,
//     with ra = 0.
//   * A HOST backtrace is confounded too, because the generated code is compiled with
//     -foptimize-sibling-calls (required — guest tail-jump loops would otherwise grow the stack
//     without bound). A tail call REPLACES the caller's frame, so the backtrace can name a function
//     that merely tail-called into the chain, with the intermediate frames gone.
//
// An override at the callee's own entry sidesteps both: it runs with the guest ABI registers as the
// caller actually set them, before the body touches anything. It cannot tell you WHO called, but it
// tells you exactly WHAT was passed — which is the question that matters when a caller appears to
// pass a value no static call site contains.
#include "core.h"
#include "game.h"
#include "cfg.h"
#include "override_registry.h"

// The recompiled bodies this file wraps. Declared with the signature the recompiler emits.
extern void gen_func_8008CE8C(Core*);   // libcd command-send: a0 = command byte
extern void gen_func_8008C944(Core*);   // called by the command-send routine before its store
extern void gen_func_8008D4E4(Core*);   // CdInit low-level init A — must return 0 for success
extern void gen_func_8008D3F4(Core*);   // CdInit low-level init B — must return 0 for success
extern void gen_func_8008C3E0(Core*);   // libcd's CD service routine (the "interrupt handler")
extern void gen_func_8009152C(Core*);   // installed into the libcd descriptor's +4 slot by CdInit
extern void gen_func_800913AC(Core*);   // installed as libcd callback #3 by the same routine
extern void gen_func_800651C8(Core*);   // the game's allocator: (size, arena, flag) -> block ptr
extern void gen_func_8002A338(Core*);   // the resumable MDEC bit-stream decoder (RE-16)
extern void gen_func_8002A478(Core*);   // its inner emit/shift block — contains the 0x8002A460 `jr $ra`
extern void gen_func_8002A5F4(Core*);   // the other spurious entry inside the same guest body

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// libcd command-send (0x8008CE8C) — `PSXPORT_DEBUG=cdarg`.
//
// RE-03's open question: the CD model observes command 0x00, but all 13 static call sites pass
// 0x01/0x02/0x0A/0x0C, and the translation of both the callee entry (`c->r[17] = c->r[4]`) and the
// store (`c->mem_w8(c->r[2], c->r[17])`) is faithful. So either a caller genuinely passes 0, or the
// value is lost before entry. This says which, with no dependence on frames or on guest pc/ra.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static void diag_cd_command(Core* c) {
  // Entry/exit markers bracket the super-call so an interleaved register-write log shows whether a
  // given store happened INSIDE this call or merely near it in time. Without the exit marker,
  // sequence is not containment — and guest pc is stale here, so it cannot answer that either.
  cfg_logf("cdarg", "CD cmd-send ENTER: a0=%02X a1=%08X a2=%08X a3=%08X sp=%08X",
           (unsigned)(c->r[4] & 0xFF), c->r[5], c->r[6], c->r[7], c->r[29]);
  gen_func_8008CE8C(c);   // super-call: the original body, unmodified
  cfg_logf("cdarg", "CD cmd-send LEAVE: v0=%08X a0=%02X s1=%08X",
           c->r[2], (unsigned)(c->r[4] & 0xFF), c->r[17]);
}


// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The game's allocator (0x800651C8) — `PSXPORT_DEBUG=alloc`.
//
// RE-09 needs ONE number: the address the allocator returns for the FIRST request after InitHeap.
// The module slot is taken from there, and every runtime-loaded module is recompiled at that base —
// so it has to be a fact, not a derivation. Deriving it as "heapBase + an assumed 8-byte header"
// would be a guess: FUN_800651C8 has per-arena free lists AND a separate small-block path (the
// `size <= 0xA0 && arena == -1` branch that recycles from a cache), and which one serves a given
// request is not obvious from a skim.
//
// Logs the first few calls with their arguments and result, then super-calls, so behaviour with the
// channel on is identical to behaviour with it off.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static void diag_alloc(Core* c) {
  static unsigned n = 0;
  const uint32_t size = c->r[4], arena = c->r[5], flag = c->r[6];
  gen_func_800651C8(c);                       // super-call: the original body, unmodified
  if (n < 12) {
    ++n;
    cfg_logf("alloc", "#%u  alloc(size=%u, arena=0x%X, flag=%u) -> 0x%08X", n, size, arena, flag,
             c->r[2]);
  }
}

// Installed from the registerOverrides hook. Diagnostic overrides are gated on their channel so a
// normal run installs nothing at all — an always-installed wrapper would put a native frame in the
// middle of every call chain and change the very tail-call behaviour being investigated.
// `PSXPORT_DEBUG=s1trace` — bisecting WHERE the command byte (held in s1) is lost between the
// command-send routine's entry and its register store. In the recomp every function shares one
// c->r[] array, so a callee that fails to restore a callee-saved register corrupts its caller's live
// state. This reports only when the value actually changes across the call, so a silent run is the
// negative result rather than an absence of output.
// Published for the VSync handler to watch. The earlier window-around-sp probe could not prove it
// ever sampled while execution was inside this call, and "zero hits" is indistinguishable from
// "never looked" — so the address is handed over explicitly instead of guessed from sp.
uint32_t g_diag_stack_watch = 0;
unsigned g_diag_vsync_while_armed = 0;

static void diag_s1_across_C944(Core* c) {
  // The callee saves s1 to its own frame at sp-64+28 and restores from the same slot (verified in
  // both the disassembly and the emitted C). So if s1 comes back wrong there are exactly two
  // possibilities, and reading the slot afterwards tells them apart:
  //   slot still holds the saved value -> the RESTORE did not run (control flow)
  //   slot holds something else        -> the guest STACK was corrupted during the call
  const uint32_t before = c->r[17];
  const uint32_t slot   = (c->r[29] - 64u) + 28u;
  g_diag_stack_watch = slot;      // armed only for the duration of this call
  g_diag_vsync_while_armed = 0;
  const uint32_t slot_before = c->mem_r32(slot);
  gen_func_8008C944(c);
  g_diag_stack_watch = 0;
  if (c->r[17] != before)
    cfg_logf("s1trace", "0x8008C944 did NOT preserve s1: %08X -> %08X | slot[%08X] %08X->%08X | vsyncs-covered=%u",
             before, c->r[17], slot, slot_before, c->mem_r32(slot), g_diag_vsync_while_armed);
}

// `PSXPORT_DEBUG=cdinit` — WHICH half of CdInit's success test fails. 0x8008A1FC returns 1 (success)
// only when BOTH of these return 0; CdInit retries it four times and only then installs the CD
// event callbacks. Reporting each return value separately turns "CdInit fails" into a named leaf.
// Both probes super-call, so behaviour is unchanged.
static void diag_cdinit_A(Core* c) {
  gen_func_8008D4E4(c);
  cfg_logf("cdinit", "0x8008D4E4 (init A) returned %08X  %s", c->r[2], c->r[2] ? "<-- FAILS" : "ok");
}
static void diag_cdinit_B(Core* c) {
  gen_func_8008D3F4(c);
  cfg_logf("cdinit", "0x8008D3F4 (init B) returned %08X  %s", c->r[2], c->r[2] ? "<-- FAILS" : "ok");
}

// `PSXPORT_DEBUG=cdisr` — does libcd's CD service routine 0x8008C3E0 ever RUN, and what does it
// return? This is the routine that writes the completion byte 0x800B3DF0, and the whole of RE-03
// turns on whether it executes. Three of its four call sites are UNGATED (0x8008CAAC, 0x8008CD2C,
// 0x8008DA58 — each reads the CD status register and calls straight in); only the wait loop's site
// at 0x8008D188 sits behind the polling gate. So "the ISR never runs" is a claim that needs
// measuring, not assuming — an earlier store-watch showed it never writing 0x800B3DF0, but that
// traces one byte and cannot distinguish "did not run" from "ran and took a path that stores
// nothing". This counts entries and reports the return value, which is a bitmask of what it serviced.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
// RE-16 attempt 11 — is $ra ALREADY garbage when 0x8002A338 is entered? `PSXPORT_DEBUG=coroentry`.
//
// The RE-16 fix makes `jr $ra` at 0x8002A460 a computed jump, and the port then dispatches to
// 0x03FF03FF — bit-stream data, not an address. Two theories have already been killed by
// measurement: the value does NOT come from a restored continuation (nothing ever writes the
// continuation slot 0x80097DB8 during boot — INST-I002, validated against a positive control), and
// it does NOT come from an incomplete MDEC decode (RE-03c is fixed and the fault is unchanged).
//
// The remaining question is upstream/downstream: does this routine CLOBBER $ra, or does it merely
// INHERIT a $ra that is already garbage? Nothing on the fresh-start path in the disassembly writes
// $ra (the decode loop works in r1..r10), so entry state decides it. If $ra is already 0x03FF03FF
// here, RE-16 has been chasing a symptom and the defect belongs to whatever calls this.
//
// Runs on the HEALTHY build: $ra holds what it holds regardless of how the emitter renders the jump.
// Super-calls, so an armed run executes exactly what an unarmed one does.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static unsigned g_coroentry_calls = 0;
static void diag_coro_entry(Core* c) {
  const unsigned n = ++g_coroentry_calls;
  const uint32_t ra = c->r[31], a0 = c->r[4], sp = c->r[29];
  // Report the FIRST few in full, then only when $ra changes — a decimated-by-count log would show
  // the boring steady state and miss the one call where $ra goes bad, which is the entire question.
  static uint32_t s_last_ra = 1;   // 1 is not a legal $ra, so call #1 always prints
  const bool changed = (ra != s_last_ra);
  s_last_ra = ra;
  if (n <= 8 || changed) {
    const bool ra_is_code = (ra >= 0x80010000u && ra < 0x800C6800u) && ((ra & 3) == 0);
    cfg_logf("coroentry", "0x8002A338 entry #%u  ra=%08X (%s)  a0=%08X (%s)  sp=%08X",
             n, ra, ra_is_code ? "a code address" : "NOT A CODE ADDRESS -- garbage on entry",
             a0, a0 ? "fresh start" : "RESUME", sp);
  }
  gen_func_8002A338(c);
}

// BRACKETING the clobber. Entry to 0x8002A338 has a good $ra (measured, CLAIM-C005), and the
// dispatch at 0x8002A460 sees 0x03FF03FF. 0x8002A460 lives inside 0x8002A478, which is itself an
// (spurious) function entry, so probing it splits the interval in two: a good $ra here means the
// clobber is inside THIS block's body and can be disassembled precisely; a bad one means it happened
// back in 0x8002A338 between entry and the jump.
//
// Reporting rule is the same as above and matters more here: print the first few, then only on
// CHANGE. What we are hunting is one transition from a code address to bit-stream data, and a
// count-decimated log is exactly the shape that would miss it.
static unsigned g_coro478_calls = 0, g_coro5F4_calls = 0;
static void coro_report(const char* who, unsigned n, uint32_t* last, Core* c) {
  const uint32_t ra = c->r[31];
  const bool changed = (ra != *last);
  *last = ra;
  if (n <= 4 || changed) {
    const bool code = (ra >= 0x80010000u && ra < 0x800C6800u) && ((ra & 3) == 0);
    cfg_logf("coroentry", "%s entry #%u  ra=%08X (%s)  sp=%08X",
             who, n, ra, code ? "code" : "NOT CODE -- clobbered before here", c->r[29]);
  }
}
static void diag_coro_478(Core* c) {
  static uint32_t last = 1;
  coro_report("0x8002A478", ++g_coro478_calls, &last, c);
  gen_func_8002A478(c);
}
static void diag_coro_5F4(Core* c) {
  static uint32_t last = 1;
  coro_report("0x8002A5F4", ++g_coro5F4_calls, &last, c);
  gen_func_8002A5F4(c);
}

static unsigned g_cdisr_calls = 0;
static void diag_cd_isr(Core* c) {
  const unsigned n = ++g_cdisr_calls;
  gen_func_8008C3E0(c);
  // Every call for the first few, then decimated — the wait loop can call this thousands of times
  // and an unbounded log would bury the answer it exists to give.
  if (n <= 8 || (n % 500) == 0)
    cfg_logf("cdisr", "0x8008C3E0 call #%u -> v0=%08X", n, c->r[2]);
}

// `PSXPORT_DEBUG=cdcb` — do libcd's two INSTALLED callbacks ever run? Neither has a static call
// site: 0x8009152C is stored into the descriptor's +4 slot by CdInit and reached only through the
// thunk at 0x8008B89C, and 0x800913AC is handed to the registrar as callback #3. A jal-only call
// graph cannot answer this — libcd dispatches indirectly throughout, so "not statically reachable"
// is a weak negative. Entry probes are immune to that: they fire wherever the call came from.
static unsigned g_cb_152C = 0, g_cb_13AC = 0;
static void diag_cb_152C(Core* c) {
  const unsigned n = ++g_cb_152C;
  gen_func_8009152C(c);
  if (n <= 4 || (n % 500) == 0) cfg_logf("cdcb", "0x8009152C (desc[+4]) call #%u -> v0=%08X", n, c->r[2]);
}
static void diag_cb_13AC(Core* c) {
  const unsigned n = ++g_cb_13AC;
  gen_func_800913AC(c);
  if (n <= 4 || (n % 500) == 0) cfg_logf("cdcb", "0x800913AC (callback #3) call #%u -> v0=%08X", n, c->r[2]);
}

void spiderman_install_diag_overrides(Game* g) {
  if (cfg_dbg("cdcb")) {
    engine_set_override_main(0x8009152Cu, diag_cb_152C, gen_func_8009152C);
    engine_set_override_main(0x800913ACu, diag_cb_13AC, gen_func_800913AC);
    cfg_logi("cdcb", "libcd installed-callback probes ARMED on 0x8009152C and 0x800913AC — "
                     "no call lines means neither ever ran");
  }
  if (cfg_dbg("coroentry")) {
    engine_set_override_main(0x8002A338u, diag_coro_entry, gen_func_8002A338);
    engine_set_override_main(0x8002A478u, diag_coro_478,   gen_func_8002A478);
    engine_set_override_main(0x8002A5F4u, diag_coro_5F4,   gen_func_8002A5F4);
    // Unconditional ARM line, per this file's own rule: a channel-gated probe's SILENCE only means
    // something if the run proves the probe was installed.
    cfg_logi("coroentry", "MDEC coroutine entry probe ARMED on 0x8002A338 — no entry lines below "
                          "means the routine genuinely never ran");
  }
  if (cfg_dbg("cdisr")) {
    engine_set_override_main(0x8008C3E0u, diag_cd_isr, gen_func_8008C3E0);
    // Unconditional arm line: this file's own lesson (docs/info/instruments.md) is that a
    // channel-gated probe's SILENCE is worthless unless the run proves the probe was installed.
    cfg_logi("cdisr", "CD service-routine probe ARMED on 0x8008C3E0 — if no call lines follow, the "
                      "routine genuinely never ran");
  }
  if (cfg_dbg("cdinit")) {
    engine_set_override_main(0x8008D4E4u, diag_cdinit_A, gen_func_8008D4E4);
    engine_set_override_main(0x8008D3F4u, diag_cdinit_B, gen_func_8008D3F4);
    cfg_logi("cdinit", "CdInit success-path probes installed on 0x8008D4E4 / 0x8008D3F4");
  }
  if (cfg_dbg("s1trace")) {
    engine_set_override_main(0x8008C944u, diag_s1_across_C944, gen_func_8008C944);
    cfg_logi("s1trace", "s1-preservation probe installed on 0x8008C944");
  }
  if (cfg_dbg("cdarg")) {
    engine_set_override_main(0x8008CE8Cu, diag_cd_command, gen_func_8008CE8C);
    cfg_logi("cdarg", "diagnostic override installed on 0x8008CE8C (logs a0, then super-calls)");
  }
  if (cfg_dbg("alloc")) {
    engine_set_override_main(0x800651C8u, diag_alloc, gen_func_800651C8);
    cfg_logi("alloc", "allocator probe ARMED on 0x800651C8 — logs the first 12 allocations "
                      "(RE-09 needs the FIRST block's address)");
  }
  (void)g;
}
