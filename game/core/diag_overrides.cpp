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
#include <execinfo.h>   // TEMP-PROBE

// The recompiled bodies this file wraps. Declared with the signature the recompiler emits.
extern void gen_func_8008CE8C(Core*);   // libcd command-send: a0 = command byte
extern void gen_func_8008C944(Core*);   // called by the command-send routine before its store
extern void gen_func_8008D4E4(Core*);   // CdInit low-level init A — must return 0 for success
extern void gen_func_8008D3F4(Core*);   // CdInit low-level init B — must return 0 for success
extern void gen_func_8008C3E0(Core*);   // libcd's CD service routine (the "interrupt handler")
extern void gen_func_8009152C(Core*);   // installed into the libcd descriptor's +4 slot by CdInit
extern void gen_func_800913AC(Core*);   // installed as libcd callback #3 by the same routine
extern void gen_func_800651C8(Core*);   // the game's allocator: (size, arena, flag) -> block ptr
extern void gen_func_80069A60(Core*);   // TEMP-PROBE: script-VM "load resource by inline name"
extern void gen_func_8005C7EC(Core*);   // TEMP-PROBE: the script bytecode VM (a0 = script pointer)

// TEMP-PROBE (remove): the script VM and its resource-load opcode.
static unsigned g_vm_calls = 0;
static void diag_vm(Core* c) {
  char b[64]; unsigned n = 0; const uint32_t p = c->r[4];
  for (; n < 32; n++) { unsigned v = c->mem_r8(p + n); b[n*2] = "0123456789ABCDEF"[v>>4]; b[n*2+1] = "0123456789ABCDEF"[v&15]; }
  b[64-1] = 0;
  cfg_logf("vm", "VM#%u enter script=%08X a1=%08X bytes=%.62s", ++g_vm_calls, p, c->r[5], b);
  gen_func_8005C7EC(c);
  cfg_logf("vm", "VM#%u leave", g_vm_calls);
}
extern void gen_func_8001B990(Core*);   // TEMP-PROBE: module loader (natively overridden by the port)
extern void gen_func_80010008(Core*);   // TEMP-PROBE: overlay-module entry dispatcher
static void diag_s1_loader(Core* c) {
  const uint32_t b = c->r[17];
  gen_func_8001B990(c);
  cfg_logf("vm", "    LOADER 0x8001B990 a0=%08X s1 %08X -> %08X%s", c->r[4], b, c->r[17], b==c->r[17]?"":"  <<< CLOBBERED");
}
static void diag_ovl_store(Core*, uint32_t a, uint32_t v, uint32_t w) {
  cfg_logf("vm", "      SLOT WRITE [%08X]=%08X w%u", a, v, w);
  void* bt[24]; int n = backtrace(bt, 24); backtrace_symbols_fd(bt, n, 2);
}
static void diag_s1_ovl(Core* c) {
  const uint32_t b = c->r[17], fn = c->r[4];
  const uint32_t slot = (c->r[29] - 0x20u) + 0x14u;
  const uint32_t sb = c->mem_r32(slot);
  c->storeWatchCb = diag_ovl_store;
  c->wwatch_arm(slot, slot + 4u);
  gen_func_80010008(c);
  c->wwatch_arm(0, 0);
  c->storeWatchCb = nullptr;
  cfg_logf("vm", "    OVLCALL 0x80010008 fn=%08X sp=%08X slot=%08X s1 %08X -> %08X | slot %08X -> %08X%s",
           fn, c->r[29], slot, b, c->r[17], sb, c->mem_r32(slot), b==c->r[17]?"":"  <<< CLOBBERED");
}
// TEMP-PROBE: generic callee-saved / sp preservation checker.
#define DIAG_PRES(A) \
  extern void gen_func_##A(Core*); \
  static void diag_pres_##A(Core* c) { \
    const uint32_t s1 = c->r[17], sp = c->r[29]; \
    gen_func_##A(c); \
    if (c->r[17] != s1 || c->r[29] != sp) \
      cfg_logf("vm", "    !! 0x%08X did NOT preserve: s1 %08X->%08X  sp %08X->%08X  ra_out=%08X v0=%08X", \
               (unsigned)0x##A, s1, c->r[17], sp, c->r[29], c->r[31], c->r[2]); \
  }
DIAG_PRES(80010080)
DIAG_PRES(8002AA0C)
DIAG_PRES(800101CC)
DIAG_PRES(80017A84)
DIAG_PRES(80017920) DIAG_PRES(8002A2EC) DIAG_PRES(8002A914) DIAG_PRES(8002B0F4)
DIAG_PRES(8002B18C) DIAG_PRES(8002B1FC) DIAG_PRES(8002B430) DIAG_PRES(80048464)
DIAG_PRES(8006AFEC) DIAG_PRES(8006B048) DIAG_PRES(8006B1B0)
DIAG_PRES(8006B514) DIAG_PRES(80082000) DIAG_PRES(80085B24) DIAG_PRES(80085BA0)
DIAG_PRES(80085BC0) DIAG_PRES(80085FB0) DIAG_PRES(80086CA8) DIAG_PRES(80086F18)
DIAG_PRES(8002B3CC) DIAG_PRES(8002A338) DIAG_PRES(800872AC)
DIAG_PRES(80087064) DIAG_PRES(8008710C) DIAG_PRES(8008735C) DIAG_PRES(80084BE0)
#define DIAG_ARM(A) engine_set_override_main(0x##A##u, diag_pres_##A, gen_func_##A);

extern void gen_func_8005C2C8(Core*);   // TEMP-PROBE: VM cursor advance (skip inline string, 2-align)
static void diag_adv(Core* c) {
  const uint32_t in = c->r[4];
  gen_func_8005C2C8(c);
  cfg_logf("vm", "    adv %08X -> %08X  nextop=%04X", in, c->r[2], (unsigned)c->mem_r16(c->r[2]));
}
static void diag_res(Core* c) {
  char b[80]; unsigned n = 0; const uint32_t p = c->r[4];
  for (; n < 24; n++) { unsigned v = c->mem_r8(p + n); b[n*3] = "0123456789ABCDEF"[v>>4]; b[n*3+1] = "0123456789ABCDEF"[v&15]; b[n*3+2] = ' '; }
  b[24*3-1] = 0;
  cfg_logf("vm", "  RES load namep=%08X a1=%08X raw=%s", p, c->r[5], b);
  gen_func_80069A60(c);
}

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
  if (cfg_dbg("vm")) {   // TEMP-PROBE (remove)
    engine_set_override_main(0x8005C7ECu, diag_vm, gen_func_8005C7EC);
    engine_set_override_main(0x80069A60u, diag_res, gen_func_80069A60);
    engine_set_override_main(0x8005C2C8u, diag_adv, gen_func_8005C2C8);
    engine_set_override_main(0x80010008u, diag_s1_ovl, gen_func_80010008);
    (void)diag_s1_loader;
    engine_set_override_main(0x80010080u, diag_pres_80010080, gen_func_80010080);
    engine_set_override_main(0x8002AA0Cu, diag_pres_8002AA0C, gen_func_8002AA0C);
    engine_set_override_main(0x800101CCu, diag_pres_800101CC, gen_func_800101CC);
    engine_set_override_main(0x80017A84u, diag_pres_80017A84, gen_func_80017A84);
    DIAG_ARM(80017920) DIAG_ARM(8002A2EC) DIAG_ARM(8002A914) DIAG_ARM(8002B0F4)
    DIAG_ARM(8002B18C) DIAG_ARM(8002B1FC) DIAG_ARM(8002B430) DIAG_ARM(80048464)
    DIAG_ARM(8006AFEC) DIAG_ARM(8006B048) DIAG_ARM(8006B1B0)
    DIAG_ARM(8006B514) DIAG_ARM(80082000) DIAG_ARM(80085B24) DIAG_ARM(80085BA0)
    DIAG_ARM(80085BC0) DIAG_ARM(80085FB0) DIAG_ARM(80086CA8) DIAG_ARM(80086F18)
    DIAG_ARM(8002B3CC) DIAG_ARM(8002A338) DIAG_ARM(800872AC)
    DIAG_ARM(80087064) DIAG_ARM(8008710C) DIAG_ARM(8008735C) DIAG_ARM(80084BE0)
    cfg_logi("vm", "TEMP script-VM probes armed on 0x8005C7EC / 0x80069A60");
  }
  (void)g;
}
