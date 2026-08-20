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
#include "cfg.h"
#include "core.h"
#include "game.h"
#include "mesh_probe.h"
#include "override_registry.h"
#include "proj_params.h" // the geomwatch probe reads the recorded projection off c->rsub
#include "str_skip_oracle.h"
#include <lucent/log.h>

// The recompiled bodies this file wraps. Declared with the signature the recompiler emits.
extern void gen_func_8008CE8C(Core *); // libcd command-send: a0 = command byte
extern void gen_func_8008C944(Core *); // called by the command-send routine before its store
extern void gen_func_8008D4E4(Core *); // CdInit low-level init A — must return 0 for success
extern void gen_func_8008D3F4(Core *); // CdInit low-level init B — must return 0 for success
extern void gen_func_8008C3E0(Core *); // libcd's CD service routine (the "interrupt handler")
extern void gen_func_8009152C(Core *); // installed into the libcd descriptor's +4 slot by CdInit
extern void gen_func_800913AC(Core *); // installed as libcd callback #3 by the same routine
extern void gen_func_800651C8(Core *); // the game's allocator: (size, arena, flag) -> block ptr
extern void gen_func_8002A338(Core *); // the resumable MDEC bit-stream decoder (RE-16)
extern void gen_func_80064B3C(Core *); // the CD.HED name lookup — a0 = filename pointer

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// libcd command-send (0x8008CE8C) — `PSXPORT_DEBUG=cdarg`.
//
// RE-03's open question: the CD model observes command 0x00, but all 13 static call sites pass
// 0x01/0x02/0x0A/0x0C, and the translation of both the callee entry (`c->r[17] = c->r[4]`) and the
// store (`c->mem_w8(c->r[2], c->r[17])`) is faithful. So either a caller genuinely passes 0, or the
// value is lost before entry. This says which, with no dependence on frames or on guest pc/ra.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static void diag_cd_command(Core *c) {
  // Entry/exit markers bracket the super-call so an interleaved register-write log shows whether a
  // given store happened INSIDE this call or merely near it in time. Without the exit marker,
  // sequence is not containment — and guest pc is stale here, so it cannot answer that either.
  cfg_logf("cdarg",
           "CD cmd-send ENTER: a0=%02X a1=%08X a2=%08X a3=%08X sp=%08X",
           (unsigned)(c->r[4] & 0xFF),
           c->r[5],
           c->r[6],
           c->r[7],
           c->r[29]);
  gen_func_8008CE8C(c); // super-call: the original body, unmodified
  cfg_logf("cdarg",
           "CD cmd-send LEAVE: v0=%08X a0=%02X s1=%08X",
           c->r[2],
           (unsigned)(c->r[4] & 0xFF),
           c->r[17]);
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
static void diag_alloc(Core *c) {
  static unsigned n = 0;
  const uint32_t size = c->r[4], arena = c->r[5], flag = c->r[6];
  gen_func_800651C8(c); // super-call: the original body, unmodified
  if (n < 12) {
    ++n;
    cfg_logf("alloc",
             "#%u  alloc(size=%u, arena=0x%X, flag=%u) -> 0x%08X",
             n,
             size,
             arena,
             flag,
             c->r[2]);
  }
}

// Installed from the registerOverrides hook. Diagnostic overrides are gated on their channel so a
// normal run installs nothing at all — an always-installed wrapper would put a native frame in the
// middle of every call chain and change the very tail-call behaviour being investigated.
// `PSXPORT_DEBUG=s1trace` — bisecting WHERE the command byte (held in s1) is lost between the
// command-send routine's entry and its register store. In the recomp every function shares one
// c->r[] array, so a callee that fails to restore a callee-saved register corrupts its caller's
// live state. This reports only when the value actually changes across the call, so a silent run is
// the negative result rather than an absence of output. Published for the VSync handler to watch.
// The earlier window-around-sp probe could not prove it ever sampled while execution was inside
// this call, and "zero hits" is indistinguishable from "never looked" — so the address is handed
// over explicitly instead of guessed from sp.
uint32_t g_diag_stack_watch = 0;
unsigned g_diag_vsync_while_armed = 0;

static void diag_s1_across_C944(Core *c) {
  // The callee saves s1 to its own frame at sp-64+28 and restores from the same slot (verified in
  // both the disassembly and the emitted C). So if s1 comes back wrong there are exactly two
  // possibilities, and reading the slot afterwards tells them apart:
  //   slot still holds the saved value -> the RESTORE did not run (control flow)
  //   slot holds something else        -> the guest STACK was corrupted during the call
  const uint32_t before = c->r[17];
  const uint32_t slot = (c->r[29] - 64u) + 28u;
  g_diag_stack_watch = slot; // armed only for the duration of this call
  g_diag_vsync_while_armed = 0;
  const uint32_t slot_before = c->mem_r32(slot);
  gen_func_8008C944(c);
  g_diag_stack_watch = 0;
  if (c->r[17] != before) {
    cfg_logf(
        "s1trace",
        "0x8008C944 did NOT preserve s1: %08X -> %08X | slot[%08X] %08X->%08X | vsyncs-covered=%u",
        before,
        c->r[17],
        slot,
        slot_before,
        c->mem_r32(slot),
        g_diag_vsync_while_armed);
  }
}

// `PSXPORT_DEBUG=cdinit` — WHICH half of CdInit's success test fails. 0x8008A1FC returns 1
// (success) only when BOTH of these return 0; CdInit retries it four times and only then installs
// the CD event callbacks. Reporting each return value separately turns "CdInit fails" into a named
// leaf. Both probes super-call, so behaviour is unchanged.
static void diag_cdinit_A(Core *c) {
  gen_func_8008D4E4(c);
  cfg_logf(
      "cdinit", "0x8008D4E4 (init A) returned %08X  %s", c->r[2], c->r[2] ? "<-- FAILS" : "ok");
}
static void diag_cdinit_B(Core *c) {
  gen_func_8008D3F4(c);
  cfg_logf(
      "cdinit", "0x8008D3F4 (init B) returned %08X  %s", c->r[2], c->r[2] ? "<-- FAILS" : "ok");
}

// `PSXPORT_DEBUG=cdisr` — does libcd's CD service routine 0x8008C3E0 ever RUN, and what does it
// return? This is the routine that writes the completion byte 0x800B3DF0, and the whole of RE-03
// turns on whether it executes. Three of its four call sites are UNGATED (0x8008CAAC, 0x8008CD2C,
// 0x8008DA58 — each reads the CD status register and calls straight in); only the wait loop's site
// at 0x8008D188 sits behind the polling gate. So "the ISR never runs" is a claim that needs
// measuring, not assuming — an earlier store-watch showed it never writing 0x800B3DF0, but that
// traces one byte and cannot distinguish "did not run" from "ran and took a path that stores
// nothing". This counts entries and reports the return value, which is a bitmask of what it
// serviced.
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
// Runs on the HEALTHY build: $ra holds what it holds regardless of how the emitter renders the
// jump. Super-calls, so an armed run executes exactly what an unarmed one does.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static unsigned g_coroentry_calls = 0;
static void diag_coro_entry(Core *c) {
  const unsigned n = ++g_coroentry_calls;
  const uint32_t ra = c->r[31], a0 = c->r[4], sp = c->r[29];
  // Report the FIRST few in full, then only when $ra changes — a decimated-by-count log would show
  // the boring steady state and miss the one call where $ra goes bad, which is the entire question.
  static uint32_t s_last_ra = 1; // 1 is not a legal $ra, so call #1 always prints
  const bool changed = (ra != s_last_ra);
  s_last_ra = ra;
  if (n <= 8 || changed) {
    const bool ra_is_code = (ra >= 0x80010000u && ra < 0x800C6800u) && ((ra & 3) == 0);
    cfg_logf("coroentry",
             "0x8002A338 entry #%u  ra=%08X (%s)  a0=%08X (%s)  sp=%08X",
             n,
             ra,
             ra_is_code ? "a code address" : "NOT A CODE ADDRESS -- garbage on entry",
             a0,
             a0 ? "fresh start" : "RESUME",
             sp);
  }
  // CALLEE CONTRACT CHECK across the whole decoder — the pre-registered falsifier for CLAIM-C011.
  // Six different emission designs produce the 0x080252D4 fault byte-identically, which points away
  // from how the control flow is RENDERED and toward what the decoder DOES. This decides it: MIPS
  // requires sp and s0-s7 to come back unchanged, so if they do, the corruption is not inside this
  // call and the emission is exonerated; if they do not, the rendering is still guilty after all.
  //
  // Reported on the FIRST violation and on every change of violation shape, never decimated by
  // count — the interesting call is the one that differs, not the hundredth identical one. The
  // no-violation case is reported too (once, at the end of the first clean call), because a probe
  // that only speaks when it finds something is indistinguishable from one that never ran.
  const uint32_t s_in[8] = {
      c->r[16], c->r[17], c->r[18], c->r[19], c->r[20], c->r[21], c->r[22], c->r[23]};
  gen_func_8002A338(c);
  uint32_t bad = 0;
  for (int i = 0; i < 8; i++) {
    if (c->r[16 + i] != s_in[i]) {
      bad |= (1u << i);
    }
  }
  const bool sp_bad = (c->r[29] != sp);
  static uint32_t s_last_shape = 0xFFFFFFFFu;
  const uint32_t shape = bad | (sp_bad ? 0x100u : 0u);
  if (shape != s_last_shape) {
    s_last_shape = shape;
    if (!shape) {
      cfg_logf("coroentry",
               "0x8002A338 call #%u: CONTRACT OK — sp and s0-s7 all preserved "
               "(so this call did not corrupt them)",
               n);
    } else {
      cfg_logf("coroentry",
               "0x8002A338 call #%u: CONTRACT VIOLATED — sp %08X->%08X (%s), "
               "s-reg mask 0x%02X",
               n,
               sp,
               c->r[29],
               sp_bad ? "CHANGED" : "ok",
               bad);
      for (int i = 0; i < 8; i++) {
        if (bad & (1u << i)) {
          cfg_logf("coroentry", "    s%d: %08X -> %08X", i, s_in[i], c->r[16 + i]);
        }
      }
    }
  }
}

// BRACKETING the clobber. Entry to 0x8002A338 has a good $ra (measured, CLAIM-C005), and the
// dispatch at 0x8002A460 sees 0x03FF03FF. 0x8002A460 lives inside 0x8002A478, which is itself an
// (spurious) function entry, so probing it splits the interval in two: a good $ra here means the
// clobber is inside THIS block's body and can be disassembled precisely; a bad one means it
// happened back in 0x8002A338 between entry and the jump.
//
// Reporting rule is the same as above and matters more here: print the first few, then only on
// CHANGE. What we are hunting is one transition from a code address to bit-stream data, and a
// count-decimated log is exactly the shape that would miss it.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The CD.HED name lookup (0x80064B3C) — `PSXPORT_DEBUG=hedname`.
//
// This is where the port dies when the menu is advanced: an unmapped read at 0x80800004 with
// ra=0x80064CA4, i.e. inside this routine. It walks a NUL-separated name table and has NO
// end-of-table exit — the only way out is a full match — so a name it cannot match walks upward
// until it leaves the mapped window. That makes it a perfect DETECTOR: it faults loudly on a bad
// argument instead of failing quietly, and the argument is the evidence.
//
// The question this answers is which half is broken: is the routine being handed a REAL filename
// (so the table walk itself is at fault), or garbage (so the caller is, and this routine is just
// where the damage surfaces)? RE-16 predicts garbage — an instruction word passed as a filename
// after the script VM's cursor is destroyed — but that has never been observed, only inferred.
//
// Prints the first bytes as both hex and text, because a filename is obvious as text and an
// instruction word is obvious as hex.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static unsigned g_hedname_calls = 0;
static void diag_hed_name(Core *c) {
  const unsigned n = ++g_hedname_calls;
  const uint32_t p = c->r[4];
  char txt[24];
  unsigned k = 0;
  bool printable = true;
  for (; k < sizeof txt - 1; k++) {
    const uint8_t ch = (uint8_t)c->mem_r8(p + k);
    if (!ch) {
      break;
    }
    if (ch < 0x20 || ch > 0x7E) {
      printable = false;
    }
    txt[k] = (char)ch;
  }
  txt[k] = 0;
  // Every call for the first few, then only the ones that look WRONG — a decimated log would show
  // the healthy steady state and miss the single bad call, which is the entire question.
  if (n <= 6 || !printable || k == 0) {
    cfg_logf("hedname",
             "#%u name@%08X = %02X%02X%02X%02X \"%s\" (%s, len %u)  fp=%08X ra=%08X",
             n,
             p,
             (unsigned)c->mem_r8(p),
             (unsigned)c->mem_r8(p + 1),
             (unsigned)c->mem_r8(p + 2),
             (unsigned)c->mem_r8(p + 3),
             txt,
             (printable && k) ? "looks like a filename" : "NOT TEXT — caller passed garbage",
             k,
             c->r[30],
             c->r[31]);
  }
  gen_func_80064B3C(c);
}

static void coro_report(const char *who, unsigned n, uint32_t *last, Core *c) {
  const uint32_t ra = c->r[31];
  const bool changed = (ra != *last);
  *last = ra;
  if (n <= 4 || changed) {
    const bool code = (ra >= 0x80010000u && ra < 0x800C6800u) && ((ra & 3) == 0);
    cfg_logf("coroentry",
             "%s entry #%u  ra=%08X (%s)  sp=%08X",
             who,
             n,
             ra,
             code ? "code" : "NOT CODE -- clobbered before here",
             c->r[29]);
  }
}
// (The 0x8002A478 AND 0x8002A5F4 probes are GONE, and their absence is the point:
// demote_internal_labels put both blocks back inside 0x8002A338, so there is no gen_func_8002A478
// or gen_func_8002A5F4 to override any more. The link error that removing them fixes was the
// DESIRED signal that demotion took effect — unlike attempt 1's link break, which killed real
// library functions the port super-calls.
//
// The 0x8002A5F4 probe is also the one whose result became CLAIM C008, "0x8002A5F4 is never
// called". That measurement was real but its scope was not: it ran on the pre-fix build, where the
// RE-16 leak makes 0x8002A338 bail after ~one block so those call sites are never REACHED.
// Reinstating this probe to re-check that claim is not possible and not needed — on the fixed build
// the address is not a function at all. See docs/info/claims.md C010.)

static unsigned g_cdisr_calls = 0;
static void diag_cd_isr(Core *c) {
  const unsigned n = ++g_cdisr_calls;
  gen_func_8008C3E0(c);
  // Every call for the first few, then decimated — the wait loop can call this thousands of times
  // and an unbounded log would bury the answer it exists to give.
  if (n <= 8 || (n % 500) == 0) {
    cfg_logf("cdisr", "0x8008C3E0 call #%u -> v0=%08X", n, c->r[2]);
  }
}

// `PSXPORT_DEBUG=cdcb` — do libcd's two INSTALLED callbacks ever run? Neither has a static call
// site: 0x8009152C is stored into the descriptor's +4 slot by CdInit and reached only through the
// thunk at 0x8008B89C, and 0x800913AC is handed to the registrar as callback #3. A jal-only call
// graph cannot answer this — libcd dispatches indirectly throughout, so "not statically reachable"
// is a weak negative. Entry probes are immune to that: they fire wherever the call came from.
static unsigned g_cb_152C = 0, g_cb_13AC = 0;
static void diag_cb_152C(Core *c) {
  const unsigned n = ++g_cb_152C;
  gen_func_8009152C(c);
  if (n <= 4 || (n % 500) == 0) {
    cfg_logf("cdcb", "0x8009152C (desc[+4]) call #%u -> v0=%08X", n, c->r[2]);
  }
}
static void diag_cb_13AC(Core *c) {
  const unsigned n = ++g_cb_13AC;
  gen_func_800913AC(c);
  if (n <= 4 || (n % 500) == 0) {
    cfg_logf("cdcb", "0x800913AC (callback #3) call #%u -> v0=%08X", n, c->r[2]);
  }
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// PALETTE PATH — `PSXPORT_DEBUG=palwatch`.  Issue 0007: every CLUT in the 3D scene reads 0x3333.
//
// The four probes below observe the whole guest palette path, entry to VRAM:
//   0x80062BB4  the CLUT register+upload routine  (a0=id, a1=palette data, a2=type; type&1 -> 16
//               entries, else 256) — it ends in LoadImage(rect, a1). THIS is what should put real
//               palette data into the CLUT strip.
//   0x80081CB0  StoreImage(rect*, dest) — VRAM -> RAM readback.
//   0x80069D44  the palette DESATURATE routine: allocates two save buffers (0x8800 + 0x1800),
//               StoreImage's the whole CLUT strip into them, then greyscales the strip row by row.
//   0x8006A154  its inverse: LoadImage's those two save buffers back over the whole strip.
//
// NEGATIVE DESIGN.  Each probe prints an ARM line, so silence means "never ran" rather than "never
// installed".  Every log line carries its DENOMINATOR: the palette/readback probes scan the whole
// buffer and print `poison=<k>/<n>` (halfwords equal to 0x3333, the guest allocator's fill — see
// FUN_800651C8, which memsets every block it hands out to 0x33333333).  So "the source was already
// poison" and "the source held real colours" are distinguishable, and a probe that scanned nothing
// says so.  The StoreImage probe samples the destination BEFORE and AFTER the super-call and prints
// both — a readback that writes nothing is then visible as before==after, and it is capped by
// NOVELTY (first 8 calls plus every call where the buffer actually changed), never by "first N".
// ─────────────────────────────────────────────────────────────────────────────────────────────────
extern void gen_func_80062BB4(Core *);
extern void gen_func_80081CB0(Core *);
extern void gen_func_80069D44(Core *);
extern void gen_func_8006A154(Core *);

// Count halfwords equal to 0x3333 in [addr, addr+2*n). Returns the count; *first holds hw[0].
static unsigned pal_poison_count(Core *c, uint32_t addr, unsigned n, uint32_t *first) {
  unsigned k = 0;
  for (unsigned i = 0; i < n; i++) {
    const uint32_t hw = (uint32_t)(c->mem_r16(addr + 2u * i) & 0xFFFFu);
    if (i == 0 && first) {
      *first = hw;
    }
    if (hw == 0x3333u) {
      k++;
    }
  }
  return k;
}

static void diag_pal_upload(Core *c) {
  static unsigned n = 0;
  const uint32_t id = c->r[4], src = c->r[5], type = c->r[6];
  const unsigned entries = (type & 1u) ? 16u : 256u;
  uint32_t first = 0;
  const unsigned poison = pal_poison_count(c, src, entries, &first);
  gen_func_80062BB4(c); // super-call: the original body, unmodified
  ++n;
  // Cap by NOVELTY, not by count: always log a call whose source held REAL data (that is the
  // interesting case and the one a "first N" cap would hide), plus the first 24 of any kind.
  if (n <= 24 || poison != entries) {
    cfg_logf("palwatch",
             "#%u CLUTload(0x80062BB4) id=%08X src=%08X type=%u entries=%u "
             "poison=%u/%u first=%04X -> clut_attr=%04X %s",
             n,
             id,
             src,
             type,
             entries,
             poison,
             entries,
             first,
             (unsigned)(c->mem_r16(c->r[2]) & 0xFFFFu),
             poison == entries ? "ALL-POISON" : "has real data");
  }
}

static void diag_pal_storeimage(Core *c) {
  static unsigned n = 0, changed_n = 0;
  const uint32_t rect = c->r[4], dst = c->r[5];
  const uint32_t x = c->mem_r16(rect), y = c->mem_r16(rect + 2);
  const uint32_t w = c->mem_r16(rect + 4), h = c->mem_r16(rect + 6);
  const unsigned words = (w * h) / 2u; // 2 pixels per 32-bit word
  const unsigned probe = words < 8u ? words : 8u;
  uint32_t before[8];
  for (unsigned i = 0; i < probe; i++) {
    before[i] = c->mem_r32(dst + 4u * i);
  }
  gen_func_80081CB0(c); // super-call
  unsigned diff = 0;
  for (unsigned i = 0; i < probe; i++) {
    if (c->mem_r32(dst + 4u * i) != before[i]) {
      diff++;
    }
  }
  ++n;
  if (diff) {
    ++changed_n;
  }
  if (n <= 8 || diff) {
    cfg_logf("palwatch",
             "#%u StoreImage(0x80081CB0) rect=(%u,%u %ux%u) dst=%08X words=%u "
             "sampled=%u changed=%u  before[0]=%08X after[0]=%08X  %s "
             "(calls-that-changed-anything=%u/%u)",
             n,
             x,
             y,
             w,
             h,
             dst,
             words,
             probe,
             diff,
             probe ? before[0] : 0u,
             probe ? c->mem_r32(dst) : 0u,
             diff ? "READBACK WROTE" : "READBACK WROTE NOTHING",
             changed_n,
             n);
  }
}

static void diag_pal_desat(Core *c) {
  static unsigned n = 0;
  ++n;
  cfg_logf("palwatch", "#%u ENTER desaturate-palettes 0x80069D44", n);
  gen_func_80069D44(c);
}

static void diag_pal_restore(Core *c) {
  static unsigned n = 0;
  ++n;
  // gp is r[28]; the routine reads its two save-buffer pointers from gp+0xEC4 (0x8800 = 256x68
  // halfwords) and gp+0xEC0 (0x1800 = 256x12). Read them BEFORE the super-call frees them.
  const uint32_t armed = c->mem_r32(c->r[28] + 0xEBCu);
  const uint32_t big = c->mem_r32(c->r[28] + 0xEC4u), small = c->mem_r32(c->r[28] + 0xEC0u);
  uint32_t fb = 0, fs = 0;
  const unsigned pb = big ? pal_poison_count(c, big, 256u * 68u, &fb) : 0u;
  const unsigned ps = small ? pal_poison_count(c, small, 256u * 12u, &fs) : 0u;
  cfg_logf("palwatch",
           "#%u ENTER restore-palettes 0x8006A154 armed=%u big=%08X poison=%u/%u "
           "first=%04X | small=%08X poison=%u/%u first=%04X",
           n,
           armed,
           big,
           pb,
           256u * 68u,
           fb,
           small,
           ps,
           256u * 12u,
           fs);
  gen_func_8006A154(c);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// CAMERA PROJECTION — `PSXPORT_DEBUG=geomwatch`.
//
// 0x80075D0C is the SOLE caller of libgte SetGeomScreen (jal 0x80076180) and SetGeomOffset (jal
// 0x80076190) in this executable — `tools/ghidra_query.py xrefs 0x8008BF14` / `0x8008BF24` each
// report exactly one CALL reference, and a raw word scan of all 524,288 words of ram.bin for those
// two `jal` encodings agrees. So this probe sees EVERY projection this game ever states.
//
// It exists because ProjParams is otherwise silent: `geomValid()` false and `geomValid()` true look
// identical from outside until a native producer calls requireGeom() and aborts. This turns that
// into a line per projection change, which is what makes
// GameConfig::hle.setGeomOffset/.setGeomScreen verifiable at all — with those two addresses ZERO
// the recompiled leaves still write the GTE, so the picture is unchanged and only this probe can
// tell you nothing was RECORDED.
//
// NEGATIVE DESIGN. An unconditional ARM line, so silence means "0x80075D0C never ran" rather than
// "the probe was never installed" — and the ARM line prints the state BEFORE any call, which for a
// correctly-unset port is `valid=0`. Capped by NOVELTY, not by count: the first 8 calls plus EVERY
// call that changes (OFX, OFY, H). Spider-Man recomputes the projection from the active viewport
// and FOV rather than passing constants, so "it changed" is the interesting event and a first-N cap
// would hide exactly the evidence that it is not a constant.
// ─────────────────────────────────────────────────────────────────────────────────────────────────
extern void gen_func_80075D0C(Core *); // the viewport/projection setup: calls SetGeomScreen+Offset

static void diag_geom_setup(Core *c) {
  static unsigned n = 0;
  static float last_ofx = 0.0f, last_ofy = 0.0f, last_h = 0.0f;
  static bool last_valid = false;
  gen_func_80075D0C(c); // super-call: the original body, unmodified
  const ProjParams &pp = c->rsub.projParams;
  const bool changed = pp.geomValid() != last_valid || pp.geomOfx() != last_ofx ||
                       pp.geomOfy() != last_ofy || pp.geomH() != last_h;
  ++n;
  if (n <= 8 || changed) {
    lucent::debug("geomwatch",
                  "#{} after 0x80075D0C: ProjParams valid={} OFX={} OFY={} H={} ({})",
                  n,
                  pp.geomValid() ? 1 : 0,
                  pp.geomOfx(),
                  pp.geomOfy(),
                  pp.geomH(),
                  changed ? "CHANGED" : "same");
  }
  // THE GATE, executed rather than reasoned about. Every native producer added from here on reads
  // the projection through requireGeom(), which ABORTS with a backtrace when the game never stated
  // one. This port has no native producer yet, so nothing else would call it — and "it would not
  // have aborted" is not a measurement. Calling it here on the first call makes the gate a fact
  // about a real run: with the two GameConfig addresses wired this returns the game's triple, and
  // with them zeroed it aborts, which is the same instrument producing the other answer.
  if (n == 1) {
    float ofx = 0.0f, ofy = 0.0f, H = 0.0f;
    pp.requireGeom("geomwatch (G7 gate: does requireGeom abort?)", ofx, ofy, H);
    lucent::info("geomwatch",
                 "GATE PASSED: requireGeom() returned OFX={} OFY={} H={} without "
                 "aborting on call #1 of the game's projection setup.",
                 ofx,
                 ofy,
                 H);
  }
  last_valid = pp.geomValid();
  last_ofx = pp.geomOfx();
  last_ofy = pp.geomOfy();
  last_h = pp.geomH();
}

void spiderman_install_diag_overrides(Game *g) {
  spiderman_install_str_skip_oracle(g);
  spiderman_install_mesh_probe(g);
  // Interned channel: the guard is around the INSTALL (real work), not around a log call.
  static const lucent::Channel geomwatch{"geomwatch"};
  if (geomwatch) {
    engine_set_override_main(0x80075D0Cu, diag_geom_setup, gen_func_80075D0C);
    lucent::info(
        "geomwatch",
        "projection probe ARMED on 0x80075D0C (the only caller of SetGeomScreen 0x8008BF14 "
        "and SetGeomOffset 0x8008BF24). ProjParams at arm time: valid={} OFX={} OFY={} H={}."
        " NO LINES BELOW MEANS 0x80075D0C NEVER RAN — which is itself the finding, not a "
        "clean bill of health for the projection.",
        g->core.rsub.projParams.geomValid() ? 1 : 0,
        g->core.rsub.projParams.geomOfx(),
        g->core.rsub.projParams.geomOfy(),
        g->core.rsub.projParams.geomH());
  }
  if (cfg_dbg("palwatch")) {
    engine_set_override_main(0x80062BB4u, diag_pal_upload, gen_func_80062BB4);
    engine_set_override_main(0x80081CB0u, diag_pal_storeimage, gen_func_80081CB0);
    engine_set_override_main(0x80069D44u, diag_pal_desat, gen_func_80069D44);
    engine_set_override_main(0x8006A154u, diag_pal_restore, gen_func_8006A154);
    cfg_logi("palwatch",
             "palette-path probes ARMED on 0x80062BB4 (CLUT upload), 0x80081CB0 "
             "(StoreImage), 0x80069D44 (desaturate), 0x8006A154 (restore). No lines "
             "for one of these means THAT routine never ran.");
  }
  if (cfg_dbg("cdcb")) {
    engine_set_override_main(0x8009152Cu, diag_cb_152C, gen_func_8009152C);
    engine_set_override_main(0x800913ACu, diag_cb_13AC, gen_func_800913AC);
    cfg_logi("cdcb",
             "libcd installed-callback probes ARMED on 0x8009152C and 0x800913AC — "
             "no call lines means neither ever ran");
  }
  if (cfg_dbg("hedname")) {
    engine_set_override_main(0x80064B3Cu, diag_hed_name, gen_func_80064B3C);
    cfg_logi("hedname",
             "CD.HED name-lookup probe ARMED on 0x80064B3C — no lines below means the "
             "lookup never ran, which would itself be the finding");
  }
  if (cfg_dbg("coroentry")) {
    engine_set_override_main(0x8002A338u, diag_coro_entry, gen_func_8002A338);
    // Unconditional ARM line, per this file's own rule: a channel-gated probe's SILENCE only means
    // something if the run proves the probe was installed.
    cfg_logi("coroentry",
             "MDEC coroutine entry probe ARMED on 0x8002A338 — no entry lines below "
             "means the routine genuinely never ran");
  }
  if (cfg_dbg("cdisr")) {
    engine_set_override_main(0x8008C3E0u, diag_cd_isr, gen_func_8008C3E0);
    // Unconditional arm line: this file's own lesson (docs/info/instruments.md) is that a
    // channel-gated probe's SILENCE is worthless unless the run proves the probe was installed.
    cfg_logi("cdisr",
             "CD service-routine probe ARMED on 0x8008C3E0 — if no call lines follow, the "
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
    cfg_logi("alloc",
             "allocator probe ARMED on 0x800651C8 — logs the first 12 allocations "
             "(RE-09 needs the FIRST block's address)");
  }
  (void)g;
}
