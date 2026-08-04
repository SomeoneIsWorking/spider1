# Instruments ledger — can the tool be trusted to show the OTHER answer?

> **RULE, learned the expensive way (2026-07-28): a probe must prove it CAN fire before its silence
> means anything.** `cfg_logf(chan, ...)` is channel-gated. A check written as
> `if (bad) cfg_logf("mychan", ...)` prints nothing when `mychan` is not in `PSXPORT_DEBUG` — and
> "condition never true" and "channel not enabled" are indistinguishable on screen. This produced a
> false negative that stood as a recorded conclusion for a full session ("VSync does not corrupt the
> slot"), and it is the THIRD distinct false-zero in this file (INST-04, INST-06, this).
> Every probe here now emits an unconditional arm/coverage line, so a silent run is evidence.

> **SECOND RULE, same day: `cmake --build … | grep error` is not a build check.** Run from the wrong
> directory, cmake printed `No rule to make target 'spiderman_port'` and exited — and the grep, which
> only looked for `error:`, swallowed it and printed a cheerful "built". The next 40-second run
> measured a STALE binary and reported zero hits for a probe that was never compiled in. Check the
> build's exit status, or look at the last lines (`Linking … / Built target …`), and confirm the
> artifact changed — `strings` for a format string the new code introduces is a two-second check that
> would have caught this immediately.

A broken instrument fails **silently**: "no signal" and "instrument returning nothing" are identical
on screen. Uniform output — all-black frames, all-zero dumps, "no diff", an empty log — is the tell.

Validate an instrument by feeding it a case that **must** differ and watching it say so. When one is
caught lying, mark it distrusted and re-check every result that used it.

> **THIRD RULE, 2026-08-05, and it is the most expensive one so far: an instrument that measures a
> DIFFERENT STAGE of the pipeline than the one you are asking about is not a weak instrument, it is
> a lying one — and it lies with true numbers.** `PSXPORT_SHOT_AT` reported 99.95% non-black on the
> intro logo while the USER, watching the window, saw a black screen. Every number was correct: it
> reads back guest VRAM, which did contain the logo, in the headless leg it was run in. Nothing in
> its output said "this never touched the swapchain". Two instruments (INST-18, INST-19) certified
> that false negative and one of them, the watchdog, is structurally incapable of ever contradicting
> it. **Before quoting an instrument, name the stage it samples and check that it is the stage in
> question.** See issue 0005.

---

## INST-18 — `PSXPORT_SHOT_AT` / `GpuVkState::shot()` PPM histogram — **DISTRUSTED 2026-08-05 for "what the player sees"; trusted for guest VRAM content**

*What it ACTUALLY shows:* `GpuVkState::shot()` (`external/psxport/runtime/recomp/gpu_vk.cpp:1202`)
calls `dump_to()` (`:1171`), which reads back the **guest VRAM texture `s_vram_tex`** and decodes
the display region itself. It never samples the swapchain. It is a measurement of VRAM CONTENT at a
present index, and of nothing downstream of that.

*What it was read as showing, for a whole session:* what is on screen.

*How it lied, with true numbers (MEASURED 2026-08-05, spider1 `3381fcc` / psxport `3f6a1e14`):* it
reported f120 = 99.95% non-black / 11395 colours for the Activision logo, and RE-07 was closed on
that. The USER, watching a window on the same build, saw a black screen. Windowed, the same
instrument reads 0.00% / 1 colour at every present index to f2400 — also true, and also not a
statement about the window. Root cause was upstream of both readings (C021, guest starvation) and
the instrument is blind to the whole segment where it lived.

*Validated where it IS good:* it does show the other answer within its own stage — 0.00% / 1 colour
on every intro shot before the RE-07 fixes, 99.95% / 11395 after (C020).

*Trust it for:* guest VRAM content at a present index, **with the headless/windowed leg stated**.
*Do NOT trust it for:* whether anything reached the display; whether present, composite, letterbox
or fade work; whether the window is alive; or distinguishing "the guest drew nothing" from "the
window shows nothing" — it gives the identical answer to both.

*The missing instrument, named so it stops being a surprise:* **nothing in this port samples the
SWAPCHAIN.** Until something does, every "the picture is correct" result in this repo is a claim
about VRAM. Building one is the cheapest way to stop repeating this failure.

*Structured entry:* `I013` (distrusted). *See:* issue 0005, C019 (falsified), C020 (scoped re-issue).

---

## INST-19 — The frame-progress watchdog (`PSXPORT_WATCHDOG=<sec>`) used as a GUEST-progress gate — **DISTRUSTED 2026-08-05; see INST-03 for what it is genuinely good at**

*The defect, and it is structural rather than a bug:* `watchdog_pet()` is called from
`gpu_present_ex` (`external/psxport/runtime/recomp/gpu_native.cpp:1399`). Presents are driven by the
host-turn timer whether or not the GUEST advances, so the pet has **no guest-side denominator at
all**. A run can be completely wedged in guest terms and the watchdog will never fire.

*MEASURED, not argued (2026-08-05, `PSXPORT_DEBUG=presentskip`):* a windowed run produced

    presents=4027  reuse_last=4027  rebuild_geom=0  rebuild_vram=0  vram_writes=0

— the guest wrote zero bytes to VRAM across 4027 presents — and the watchdog reported nothing. The
headless leg of the same build, same duration: `presents=4106 reuse_last=2165 rebuild_geom=1511
rebuild_vram=430 vram_writes=12812`.

*What this invalidates:* **every gate result of the form "0 abort, ran N frames".** That phrasing
appears throughout this repo's logs and reports; it means "presents kept happening" and never "the
game is running". Re-read any conclusion that leaned on it.

*Trust it for:* a hard hang where the present loop itself stops, and for its backtrace when it does
fire — with INST-03's caveats (single sample, corroborate against the disassembly).
*Do NOT trust it for:* guest liveness, boot progress, or any part of a pass/fail gate.

*The missing instrument:* a watchdog fed by a **guest-side** counter — guest vblank count, VRAM
writes, or recompiled-function dispatches — printing its denominator, so a negative reads "guest
advanced 0 of N" instead of silence. Until that exists, quote the `presentskip` counters
(`vram_writes`, `rebuild_geom`) rather than the frame count.

*Structured entry:* `I014` (distrusted). *See:* issue 0005, C021, and INST-03.

---

## INST-01 — `PSXPORT_DEBUG=vsync` (VSync call log) — **trusted**

*What it shows:* every `VSync(mode)` the guest makes, with the caller's `ra` and the current vblank
counter. Implemented in `game/core/sync_native.cpp`.

*Validated:* it distinguishes the two answers it exists to distinguish. It reported a **427,643 to 1**
split between query and blocking calls — not a uniform stream — and reported *different* `mode`
values and *different* `ra` values within the same run. An instrument that could only ever print one
mode would have been indistinguishable from the truth here, so this mattered: the split is what
established CLAIM-02.

*Known limit — read this before citing it:* it can only observe calls the port actually reaches, and
the port currently stops in the disc-init retry loop during boot. So it measures the **boot** call
pattern, not the gameplay loop. Do not cite it as evidence about gameplay pacing.

---

## INST-02 — `tools/redump_ram.py` + the framework's `disasm.py` — **trusted**

*What it shows:* the guest instructions at any address, from the retail executable.

*Validated:* it produced *different, structurally coherent* disassembly at every address queried —
a recognisable Sony crt0 at the entry point, a stable-read counter loop at `0x80084BE0`, a countdown
loop at `0x80084D58` — rather than uniform garbage, which is what a wrong load-address mapping would
produce. Cross-checked independently: the string pointer the disassembly showed being loaded
(`0x80096020`) resolves to the diagnostic text `VSync: timeout`, confirming both the byte mapping and
the function identification at once.

*Known limit:* it lays down only the executable's `.text` at load time. Anything the game pages in
from `CD.WAD` at runtime is **not** in the image and will disassemble as zeros. Zeros mean "not
present in this image", not "no code there".

*Second known limit — SILENCE IS NOT "EMPTY" (found 2026-07-28):* `disasm.py` is a thin capstone
wrapper, and capstone's `disasm()` **stops at the first word it cannot decode and yields nothing
further**. Ask for a range that *begins* on data — e.g. `0x8008C3B0`, which holds a string pointer —
and the tool prints **nothing at all** and exits 0. That is indistinguishable from "this range is
empty", and it is exactly the read-silence-as-a-negative trap that has already cost this project
three false conclusions. If a range comes back empty, dump the raw words before believing it; the
code is usually there, just preceded by a literal pool or a BIOS-call stub. Nudging the start
forward past the data resumes normal output.

---

## INST-09 — `PSXPORT_DEBUG=bios` (every BIOS call, handled or not) — **trusted**

*What it shows:* one line per A0/B0/C0 dispatch — table, function number, the four argument
registers, and `$ra`. The BIOS stubs are tail jumps (`li $t2,0xA0; jr $t2`), so `$ra` survives as the
real call site. For `SysEnqIntRP`/`SysDeqIntRP` it also dumps the guest `InterruptElement`'s four
words.

*Why it exists:* the pre-existing `UNIMPL` log only fires for calls that fall THROUGH the dispatcher,
which answers "what is missing" but not "what does this game use" — and the second question is the
one that decides which BIOS subsystem a port must model next. It was also the fastest way to settle
a structure layout that no header could be trusted for.

*Validated:* it distinguishes — 21 distinct function numbers over one boot, with counts that match
independent expectations (`A0:0x39 InitHeap` exactly once from crt0; `B0:0x08 OpenEvent` eight times
against eight `B0:0x0C EnableEvent`; `C0:0x03` immediately before `C0:0x02` on the same element, the
standard deq-then-enq idiom). Cross-checked against the binary: the element words it printed
(`0x80087660`, `0x800875F8`) disassemble as a handler/verifier pair, and the register base the
verifier loads (`*0x800B12C4`) reads `0x1F801070` in the load image — the address `I_STAT` should be.
Uniform or empty output would have been the tell; it produced neither.

*Known limit:* it sees only calls that reach `Hle::dispatchBios`. A BIOS routine the guest reaches by
some other route — an address baked into a table, a driver vtable it filled itself — does not appear.
Absence here is not proof the guest never used a facility.

---

## INST-10 — `PSXPORT_DEBUG=irq` (interrupt-controller traffic) — **trusted**

*What it shows:* every `I_STAT`/`I_MASK` read and write with `$ra`, the before/after value on an
acknowledge, and an explicit line each time the CD controller raises IRQ2.

*Validated — and the first version FAILED this, which is why the entry exists.* A run with only the
read/write logging produced **35 lines and not one CD-raise**, because the raise was latched lazily
on the next `I_STAT` read and nothing in this boot reads `I_STAT`. That is the file's own rule biting:
the probe could not fire, so its silence meant nothing. Moving the latch to where the controller
actually raises made the same boot report **152 raises**. It also distinguishes values rather than
printing a constant — `I_MASK` is observed as `0x000`, `0x001` and `0x00D` at different points, and
acknowledges print a real before→after transition.

*What it settled immediately:* the guest's `I_MASK` reaches **`0x00D`** — VBlank, CDROM and DMA all
enabled — so this game does want the CD interrupt, and the remaining gap is delivery, not masking.

*Known limit:* `I_STAT` bit 2 is the only bit any source asserts, because it is the only interrupt
source the framework models. A zero in any other bit means "no source implemented", not "the hardware
was quiet". Do not read this log as evidence that VBlank or DMA interrupts did not occur.

---

## INST-11 — `PSXPORT_DEBUG=cdisr` (CD service-routine entry probe) — **trusted**

*What it shows:* every entry to libcd's CD service routine `0x8008C3E0` and its return value, via an
observe-only override that super-calls the original body (`game/core/diag_overrides.cpp`).

*Why it exists:* RE-03 turns entirely on whether that routine executes, and the two cheaper
instruments cannot answer it. A store watch on `0x800B3DF0` traces ONE byte, so it cannot separate
"did not run" from "ran and took a path that stores nothing". Host backtraces are confounded by
`-foptimize-sibling-calls`, and guest `pc`/`ra` are stale under static gen-to-gen calls (INST-07).
An override at the callee's own entry is immune to all three.

*Validated — it emits an unconditional ARM line.* This file's standing rule is that a channel-gated
probe's silence is worthless unless the run proves the probe was installed. The arm line prints, then
zero call lines follow, so **the routine genuinely never runs** — a real negative rather than an
absent instrument. Counting is decimated after the first 8 entries because the wait loop could
otherwise call it thousands of times and bury the answer.

*Extended (`PSXPORT_DEBUG=cdcb`):* the same shape now covers libcd's two INSTALLED callbacks,
`0x8009152C` (descriptor slot `+4`) and `0x800913AC` (callback #3). Neither has a static call site, so
an entry probe is the only instrument that can see them — and both report zero entries with their arm
line present.

*Known limit:* it proves the routine was never ENTERED. It says nothing about why its three ungated
call sites (`0x8008CAAC`, `0x8008CD2C`, `0x8008DA58`) are not reached — that is a separate question.

---

## INST-12 — Ghidra headless (`tools/ghidra_query.py`) — **trusted, after failing its first control**

*What it shows:* real cross-references (calls AND data), the function containing an address with its
extent, the call set of a function, annotated data dumps — and **decompiled C**.

*Why it replaces the old approach:* INST-02 (capstone + hand-rolled address scans) is what produced
this project's run of confident wrong answers — "no callers" for a routine installed into a table, a
`.data` pointer read as zero, a store blamed on the wrong `jal` because it sat in a branch delay
slot, function boundaries guessed from `jal` targets. An address scan finds only the reference FORMS
you thought to look for. Prefer this for anything about references, boundaries, or control flow.

*It FAILED its first control, and that is the entry.* A raw-binary import gives Ghidra no entry
point, so it disassembled nothing and **every** reference query returned zero — indistinguishable
from "genuinely unreferenced", the exact failure this tool exists to remove. Worse, the first
PyGhidra binding silently opened a NEW EMPTY program rather than the imported one, so memory reads
threw and the seeder reported "created 0 functions" while seeding nothing.

*Validated only after both were fixed* (`open_project` + `program_context` binds the saved program;
`tools/ghidra_seed.py` feeds it the recompiler's own 1561 function entries): a control query on
`0x8008C3E0` now returns **exactly the four `UNCONDITIONAL_CALL` sites** an independent hand scan
found — `0x8008CAAC`, `0x8008CD2C`, `0x8008D188`, `0x8008DA58` — each with its owning function.
Memory reads agree with a raw dump (`0x800B38EC = 0x80096450`).

*Run a control after any re-import.* An empty answer from this tool has now been wrong twice for
reasons that had nothing to do with the question asked.

*Known limit:* it sees STATIC references. A pointer written into a table at runtime still shows zero
call refs — for those, an entry probe on the callee (INST-11) is the instrument, not this.

---

## INST-13 — `PSXPORT_DEBUG=guest` (the game's OWN diagnostics) — **trusted**

*What it shows:* every string the executable prints through BIOS `A(3Fh) printf` / `B(3Fh) puts`.

*Why it matters more than it sounds:* these were being discarded as `UNIMPL A0:0x3F` — hundreds per
boot — which threw away the most direct evidence a port can get: the binary saying, in English, what
IT thinks is wrong. This project's single most useful identification (`VSync: timeout`) already came
from a string the executable emits; every other one was going in the bin.

*Validated, and it corrected a live conclusion within one run:* the sector headers being served
looked correct (`LBA 8850 -> 02:00:00 mode 02`, matching its Setloc exactly), from which I had just
concluded the guest's drive-position check was passing. The channel immediately reported
`CdRead: sector error` x34 and `CdRead: retry...` x68 — so the check is being REJECTED. It also
distinguishes rather than echoing one string: five distinct messages in one boot, including
`CD_init:` and `ResetGraph:jtb=...,env=...` with correctly formatted arguments.

*Known limit:* it sees only what the game routes through the BIOS calls. A title with its own serial
or screen logger prints nothing here, and silence is not evidence that the game is content.

---

## INST-04 — `tools/go_public.py scan` — **trusted, but it CAN report a false green**

*What it shows:* pre-publication scan of the git HISTORY for copyrighted/binary assets (section A),
foreign or absolute paths (section B), and docs referencing sensitive gitignored items (section C).

*Caught lying once, and this is the important entry in this file:* run against a repo whose `HEAD`
had been deleted, it reported **`RESULT: clean ✓ — ready to publish`**. There was no history to walk,
so it found nothing — and "found nothing" and "nothing is there" print identically. Re-running the
identical command after a real commit reported **0 blocking + 58 to review**. Same repo, same
content, opposite-looking verdicts.

*FIXED IN THE TOOL 2026-07-28 — this no longer relies on the reader remembering.* `report()` now
receives the commit count, refuses to issue any verdict when it is <= 0 ("NOT A VERDICT — this
repository has no commits"), and prints `scanned N commit(s) of history` on every run so the reach is
never implicit. Fixed in all three copies (this repo, Tomba2Engine, the global skill).

*The fix itself needed two attempts, which is worth recording:* the first version recomputed the
count inside `report()`, where `cwd` is not in scope, and hid the resulting `NameError` behind
`except Exception` — so it reported "could not be queried" for a healthy 16-commit repo and would
have failed closed on everything. Caught by validating BOTH directions (empty repo must refuse, real
repo must scan) rather than only the case being fixed.

*How to use it so it cannot lie:* **only scan a repo that has commits, and confirm the verdict is
non-empty.** A `clean ✓` with zero items listed in any section is the tell — a real scan of a real
repo essentially always has section-C items to review. Treat a silent all-clean as an unrun scan
until proven otherwise.

*Validated positively:* it distinguishes answers when it has history. It found a genuine blocking
item (a tilde home path in a vendored tool's comment) and, once that was fixed, correctly moved it
off the blocking list while keeping the 58 unrelated review items — rather than flipping everything
to clean at once.

*Interpreting section C:* the review items on this repo (100 at last count) are filename PATTERNS appearing as text
(`SLUS_`, `.chd`, `.bin`, `.exe`) in documentation and provisioning code that tells a user to supply
their own disc. That is intentional and portable. Sections A and B are the ones that block, and both
are empty here.

---

## INST-08 — `PSXPORT_DEBUG=cdarg` (entry-argument override) — **trusted; the one that works here**

*What it shows:* the guest ABI arguments at the ENTRY of a chosen recompiled function, logged by a
native override that then super-calls the original body (`game/core/diag_overrides.cpp`).

*Why it is the right tool:* it depends on neither of the two things that mislead in this codebase —
it does not read guest `pc`/`ra` (stale on gen-to-gen calls) and it does not walk host frames
(collapsed by `-foptimize-sibling-calls`). It runs with the registers as the caller actually set
them.

*Validated:* it reported a NON-uniform distribution (17x `a0=0x01`, 17x `a0=0x0A`) rather than one
repeated value, and that result then disproved a previously-recorded backtrace attribution — an
instrument that can overturn a standing belief is doing its job.

*Cost when off:* nothing. The override is installed only when its channel is set, deliberately: an
always-installed wrapper would insert a native frame into every call chain and perturb the very
tail-call behaviour under investigation.

---

## INST-17 — "do two resident modules share guest bytes?" — the `overlay_place()` abort — **trusted; a runtime invariant, not a check tool**

*The question this answers:* how do I know two live CD.WAD modules are not overlapping in guest RAM
— the failure that silently makes one module's code run as another's, and that killed the port at
recomp-MISS `0x800C6684`?

*The answer is that you cannot reach a state where they do.* The check is not a tool you remember to
run against a dump; it is enforced on **every** placement, in
`external/psxport/runtime/recomp/overlay_router.cpp:132-151`:

```cpp
    // The invariant the base-relative design rests on: no two resident modules overlap. Checked on
    // every placement, because the alternative is one module's code silently running as another's.
    const uint32_t lo = base & 0x1FFFFFFFu, hi = lo + size;
    for (int j = 0; j < R->overlay_count && j < Core::kRecMaxOverlays; j++) {
      if (j == i || !c->ovBase[j]) continue;
      const uint32_t jlo = c->ovBase[j] & 0x1FFFFFFFu;
      const uint32_t jhi = jlo + (R->overlays[j].end - R->overlays[j].base);
      if (lo < jhi && jlo < hi) {
        lucent::error("ovload", …);
        fflush(stderr);
        abort();
      }
    }
```

The diagnostic it aborts with names both modules and both live ranges, and states the only two
things that can produce it: *"The game's own allocator cannot produce this — it means the loader
intercept named the wrong allocation as the module body, or a body was freed without evicting it
from this registry."*

*Why this is a better instrument than any dump scanner.* A scanner samples ONE instant and its clean
verdict has to carry "cannot rule out an overlap before or after the dump". This has no sampling
window at all: `overlay_place()` is the single writer of `Core::ovBase`/`ovDelta` (see
`runtime/recomp/core.h:114`), so a violating state is unreachable, not merely unobserved. And a
scanner's silence is ambiguous — the abort's silence is not, because the *absence* of the abort over
a run is the same evidence as a green check at every load in that run.

*Note the invariant CHANGED, and is not the one a slot-era check would test.* Co-residency itself is
now **normal and correct** — L5A5LSC, LIZMAN and VENOM are simultaneously live at
`0x8014A6D0` / `0x801BDA30` / `0x801C6238` on a real boot. Only *overlap* is a fault. A check that
flags "two modules live at once" would today report a violation on every healthy run.

*Corroborating instrument for the live picture:* `PSXPORT_DEBUG=ovload` logs each placement with its
name, live range and delta from the link base, and each eviction — so the residency set over time is
observable without a dump.

*Its blind spot, stated plainly:* the abort branch itself is **not covered by a test**.
`external/psxport/tests/test_overlay_reloc.cpp` exercises 6 cases (co-resident routing, half-open
ranges, eviction, reload, per-`Core` isolation, fixed-base/unknown-name refusal) and none of them
places an overlapping pair, because tripping it calls `abort()` and the harness has no death-test
facility. So the *routing* around the invariant is proven; the abort's own firing is proven only by
reading it. Adding a death test is the outstanding work — until then, treat "it would have caught
it" as reasoning, not measurement.

---

## INST-07 — `PSXPORT_DEBUG=cdcw,cdcbt` (CD register writer) — **trusted, with one hard caveat**

*What it shows:* every write to a CD controller register with the register, value, current bank, and
guest `pc`/`ra` (`cdcw`); plus a host backtrace on the command-register write (`cdcbt`).

*Validated:* it distinguishes answers — different registers, values and banks per line, and it
resolved a caller chain that a blind instrument (INST-06) could not, then that chain was corroborated
against the disassembly (`0x8008D4E4` really does call `0x8008CE8C`, from four sites).

*THE CAVEAT — the `pc`/`ra` fields lie under recompiled execution.* The recomp does not refresh guest
`pc`/`ra` on static gen-to-gen calls. This instrument's very first output attributed a CD command
write to `0x8008B900`, which disassembles as a three-instruction getter (load halfword, return) that
touches no CD register. `ra=0` is the tell. Treat `pc`/`ra` as a hint only — a plausible non-zero `ra` is weak evidence, a zero `ra` is none.

*SECOND CAVEAT, found later: `cdcbt`'s host backtrace is ALSO confounded.* Generated code is compiled
with `-foptimize-sibling-calls` (required, or guest tail-jump loops grow the stack without bound), so
a tail call REPLACES the caller's frame. The backtrace can name a function that merely tail-called
into the chain, with intermediate frames gone. So this instrument can localise a write to a region
but **cannot be trusted to name the immediate caller** — this was later confirmed the hard way, when
an entry-argument override (INST-08) disproved a caller this backtrace had named. For a definitive answer, log the argument on
entry via a framework override + super-call rather than reading frames.

---

## INST-15 — `PSXPORT_SELFTEST=mdecpump` (MDEC pending-DMA / voffs placement A/B) — **trusted, negative-validated**

*What it shows:* whether the guest-visible MDEC DMA path (pending-channel latch in `mem.cpp
mdec_dma_pump`, per-word `voffs` scatter into guest RAM) produces BIT-IDENTICAL output to the
independently verified native_fmv drain path, on a synthetic 16-macroblock 16bpp frame. Also asserts
the mechanism itself engaged: DMA0's busy bit must still be SET right after its start (deferral
really happened) and both busy bits CLEAR after DMA1's start completes the ping-pong.

*Validated both ways (2026-07-30):* passes on the real code; deliberately zeroing `offs` in the pump
made it FAIL with `964 of 2048 words` differing. It also guards its own blind spot: an all-alike
reference frame (which would compare equal under any placement bug) is itself a FAIL — the test
requires >= 16 distinct output words before the comparison counts.

*Known limits:* single whole-frame DMA1 at 16bpp with block size 0x20 — it does not exercise 24bpp
(WWS=6), multi-transfer-per-frame DMA1 (per-strip `DecDCTout`), or a DMA1-first start order. Those
paths are reasoned + traced, not A/B-proven.

*Run:* `PSXPORT_SELFTEST=mdecpump ./scratch/bin/spiderman_port scratch/bin/SLUS_008.75` (exit 0/1).

---

## INST-14 — `re_frontier.py check` — **FIXED 2026-07-29; my first diagnosis was WRONG**

*What it claimed:* "re-frontier OK: no unknown deps, no cycles, every re-verified step cites
evidence." It printed that on every run for a whole session, including right after edits that
introduced a contradiction, because it was parsing **zero entries**.

*The cause I recorded first, and it was not the main one.* I wrote that the skill's parser wants
`### <ID> — <title>` with `- status:` fields while this file used `## RE-05 — … — <status>`, so every
step was being read as an AREA. That IS a real defect and the file did need converting — but it was
the SECOND problem, and fixing it alone changed nothing.

**The primary cause was that the tool was reading a file that does not exist.** `ROADMAP` defaults to
`<parent-of-script>/docs/re-frontier.md`, which is correct only when the tool lives at
`<repo>/tools/`. Running from the global skill dir it resolved to
`<skills-dir>/docs/re-frontier.md` — a path that never exists. And `load()` opened with

    if not os.path.exists(ROADMAP):
        return {}, []

so a missing roadmap was **indistinguishable from a healthy one**: zero entries, then a green check
over nothing. That silent early-return is the worst failure a validator can have, and it is what made
the output uniform.

*Fixed, in both places:*
- the global `re-frontier` skill now **exits non-zero** with the path it
  looked for and the `RE_FRONTIER_ROADMAP` override to set — the early-return is deleted;
- this project's `docs/re-frontier.md` is converted to the machine-readable schema, with duplicate
  IDs resolved (`RE-03` appeared three times, `RE-04`/`RE-05` twice each on *different* topics —
  the OT/packet-pool and scheduler steps are now `RE-12`/`RE-13`, and the two superseded `CdInit`
  investigations are `HIST-03a`/`HIST-03b`, `skip-by-design`).

**Invoke it with the path — the bare command is still wrong for this repo:**

    RE_FRONTIER_ROADMAP=docs/re-frontier.md python3 "$CLAUDE_SKILLS/re-frontier/re_frontier.py" check

*Validated, and it can now show the OTHER answer — which is the whole bar:* with no roadmap it exits
1 and says so; with the roadmap it rendered the full dependency tree, listed 5 RE-ready steps, and
**immediately failed with 11 real problems** (re-verified steps whose evidence lived only in prose,
with no machine-readable `- evidence:` field). Those are now filled and it passes. A tool that could
only ever print OK produced none of that.

*The reusable lesson, and it is the same one this page keeps teaching:* I diagnosed a uniform-output
instrument from its OUTPUT FORMAT and stopped there. The cheap check I skipped was "does the input
file it reads actually exist" — one `os.path.isfile` away.

*Invocation correction (2026-08-05):* use the IN-REPO path, `python3 tools/re_frontier.py`, from the
repo root. `$CLAUDE_SKILLS` is unset in a plain shell, so the form above collapses to
`/re-frontier/re_frontier.py` and fails. The `RE_FRONTIER_ROADMAP=docs/re-frontier.md` prefix is
still required.

*SECOND DEFECT, found 2026-08-05 and it DESTROYS DATA: `re_frontier.py set` silently deletes any
field not in its schema.* `FIELDS = ["status", "area", "deps", "evidence", "where", "gap", "notes"]`
(`tools/re_frontier.py:61`); a step is re-serialised from that list on every `set`, so anything else
is dropped with no warning and no diff the tool shows you. **Measured:** a `set RE-07 status=... gap=...`
silently removed RE-07's hand-written `- where-2:` line (the per-channel DMA-completion `where`),
caught only by reading `git diff` afterwards. It has been merged into `where:` and is not lost.
**So: never add a non-schema field to this file, and always read `git diff` after a `set`.** The
proper fix is for `set` to preserve unknown fields (or refuse), and it is NOT done.

---

## INST-06 — `PSXPORT_WWATCH` (guest store watch) — **TRUST RESTORED 2026-07-28 — my distrust was wrong**

*What it should show:* the guest `pc`/`ra` of any store landing in an address range
(`PSXPORT_WWATCH=lo,hi`), which is exactly the tool for "who writes this register?".

**RETRACTION.** This entry marked the tool distrusted. Re-tested directly: it arms correctly
(`cfg_str` returns the range, `sscanf` parses `lo=800B397C hi=800B3980`) and fires — **892 hits** over
a boot on the validation address. The tool works. Leaving it marked distrusted was the more damaging
error, because it steers the next session away from a working instrument toward hand-rolled probes.

Why it reported zero earlier is **not established**. The likeliest candidates are an intervening
framework change to the per-guest-write path, or a flaw in my original test. Recorded as unresolved
rather than guessed. Validate before relying on it — but expect it to work.

*Discrimination re-validated 2026-07-29 — it can show the OTHER answer.* The strongest check yet, and
it came free: within the same binary and the same boot, the watch reports **62,114 hits** on
`0x800A5130` (the game's per-frame pad mirror) and **4 hits** on `0x800A50EC` (the libpad buffer,
written only by init). A blind instrument returning a uniform answer cannot produce those two numbers
from the same run. It also attributes them to *different* writers (`0x8006B3C8` vs `0x80091330`),
which a stuck instrument could not do either. This is the "must be able to report the other answer"
bar, met on real data rather than on a synthetic probe — treat the tool as trusted for both
directions, including the negative ("nothing writes this").

*The original (incorrect) reasoning, kept because the discipline was right even though the verdict
was wrong:* it reported **zero hits on an address that is written continuously**. Armed
over the vblank counter `0x800B397C..0x800B3980` — which this port's own native VSync writes on every
call, thousands of times a run — it logged nothing at all. A tool that cannot see a guaranteed write
cannot be used to prove a write does not happen.

*What it nearly cost:* it was armed over the CD command register `0x1F801801` to answer RE-03's open
question, returned zero hits twice, and the obvious reading — "the guest never writes the command
register" — is a substantive claim about the game that would have gone into the frontier notes as
fact. It is not supported by anything. **Both zero-hit results are void.**

*Two traps found while testing it, worth knowing if it is ever repaired:*
  * `wwatch_check` ORs `0x80000000` into the store address before comparing, but the env-arm path
    (unlike the programmatic `wwatch_arm`) does NOT normalise the configured bounds. So an I/O watch
    must be armed in KSEG1 form (`9f801801`), not `1f801801`, or it silently never matches. This is
    documented in a comment in `mem.cpp` and is easy to walk straight into.
  * The env parse shadows the width parameter `w` with the config string `w` inside `wwatch_check`.
    Harmless as written, but it is the kind of thing to check first.

*Root cause not established.* Only SBS calls `wwatch_arm`, and SBS was not running, so the
"pre-armed, env never read" theory does not explain it. Ordering is not the problem either —
`wwatch_check` runs before `io_write` in `mem_w8`, so I/O stores do pass through it.

*Before using it again:* re-validate against a known-written address and confirm it fires. Treat a
zero result as "unproven", never as "does not happen".

---

## INST-05 — `PSXPORT_DEBUG=cdc` (framework CD-controller channel) — **trusted**

*What it shows:* every command the guest issues to the modelled CD controller
(`external/psxport/runtime/recomp/cdc_native.c`), with its parameters, and whether the model handled it.

*Validated:* it distinguishes answers — it names a specific command byte and reports handled versus
`UNHANDLED`, and it is the reason RE-03 has a mechanism rather than a guess. It turned "CdInit returns
0 for some reason" into "the guest issues command 0x00 77 times and the model acks each one".

*Known limit:* it reports what the MODEL received, not what the guest meant. `cmd 0x00` means a zero
reached the command register with index 0 — it does not establish that the guest intended a command.
That distinction is exactly the open question in RE-03, so do not read this channel as intent.

---

## INST-03 — The watchdog backtrace (`PSXPORT_WATCHDOG=<sec>`) — **trusted, with a caveat**

*What it shows:* where the process was when no frame got presented in time — the framework's stall
diagnostic.

*Validated:* it distinguished states rather than always saying the same thing. Across the port's
successive stalls it named *different* locations — the libetc VSync deadline loop, then the disc-init
retry loop — and each pointed at a cause that, once addressed, moved the stall somewhere new. That is
the behaviour of a working instrument.

*Second caveat, which DID mislead here:* a single sample lands wherever the process happens to be,
not necessarily at the cause. On this port it repeatedly sampled inside the 100-vblank wait of a
retry loop, which says only "it is retrying" — the actual failure was one call further on. Worse, a
backtrace taken against the seed-contaminated substrate showed a call chain (`0x8008CE8C`) that does
not exist in the clean disassembly, and it was written into the frontier notes before being caught.
**Corroborate any backtrace against the disassembly before recording it as a call chain.**

*Caveat that cost time here:* it is a **single sample**, so it cannot by itself distinguish "spinning
in a tight loop" from "making slow progress". Confirm which by sampling more than once and comparing
— an identical backtrace across independent runs is the spin signal. Attaching an external sampler
(`gdb -p`, `eu-stack -p`) did **not** work in this environment; repeated runs are the practical
substitute.
