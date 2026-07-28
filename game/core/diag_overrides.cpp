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

// Installed from the registerOverrides hook. Diagnostic overrides are gated on their channel so a
// normal run installs nothing at all — an always-installed wrapper would put a native frame in the
// middle of every call chain and change the very tail-call behaviour being investigated.
// `PSXPORT_DEBUG=s1trace` — bisecting WHERE the command byte (held in s1) is lost between the
// command-send routine's entry and its register store. In the recomp every function shares one
// c->r[] array, so a callee that fails to restore a callee-saved register corrupts its caller's live
// state. This reports only when the value actually changes across the call, so a silent run is the
// negative result rather than an absence of output.
static void diag_s1_across_C944(Core* c) {
  // The callee saves s1 to its own frame at sp-64+28 and restores from the same slot (verified in
  // both the disassembly and the emitted C). So if s1 comes back wrong there are exactly two
  // possibilities, and reading the slot afterwards tells them apart:
  //   slot still holds the saved value -> the RESTORE did not run (control flow)
  //   slot holds something else        -> the guest STACK was corrupted during the call
  const uint32_t before = c->r[17];
  const uint32_t slot   = (c->r[29] - 64u) + 28u;
  gen_func_8008C944(c);
  if (c->r[17] != before)
    cfg_logf("s1trace", "0x8008C944 did NOT preserve s1: %08X -> %08X | frame slot [%08X]=%08X",
             before, c->r[17], slot, c->mem_r32(slot));
}

void spiderman_install_diag_overrides(Game* g) {
  if (cfg_dbg("s1trace")) {
    engine_set_override_main(0x8008C944u, diag_s1_across_C944, gen_func_8008C944);
    cfg_logi("s1trace", "s1-preservation probe installed on 0x8008C944");
  }
  if (cfg_dbg("cdarg")) {
    engine_set_override_main(0x8008CE8Cu, diag_cd_command, gen_func_8008CE8C);
    cfg_logi("cdarg", "diagnostic override installed on 0x8008CE8C (logs a0, then super-calls)");
  }
  (void)g;
}
