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

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// libcd command-send (0x8008CE8C) — `PSXPORT_DEBUG=cdarg`.
//
// RE-03's open question: the CD model observes command 0x00, but all 13 static call sites pass
// 0x01/0x02/0x0A/0x0C, and the translation of both the callee entry (`c->r[17] = c->r[4]`) and the
// store (`c->mem_w8(c->r[2], c->r[17])`) is faithful. So either a caller genuinely passes 0, or the
// value is lost before entry. This says which, with no dependence on frames or on guest pc/ra.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static void diag_cd_command(Core* c) {
  cfg_logf("cdarg", "CD cmd-send entry: a0=%02X a1=%08X a2=%08X a3=%08X sp=%08X",
           (unsigned)(c->r[4] & 0xFF), c->r[5], c->r[6], c->r[7], c->r[29]);
  gen_func_8008CE8C(c);   // super-call: the original body, unmodified
}

// Installed from the registerOverrides hook. Diagnostic overrides are gated on their channel so a
// normal run installs nothing at all — an always-installed wrapper would put a native frame in the
// middle of every call chain and change the very tail-call behaviour being investigated.
void spiderman_install_diag_overrides(Game* g) {
  if (cfg_dbg("cdarg")) {
    engine_set_override_main(0x8008CE8Cu, diag_cd_command, gen_func_8008CE8C);
    cfg_logi("cdarg", "diagnostic override installed on 0x8008CE8C (logs a0, then super-calls)");
  }
  (void)g;
}
