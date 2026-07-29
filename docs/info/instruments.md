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

## INST-14 — `re_frontier.py check` — **DISTRUSTED 2026-07-29: it validates NOTHING here**

*What it claims:* "re-frontier OK: no unknown deps, no cycles, every re-verified step cites
evidence." It printed exactly that on every run this session, including immediately after edits that
introduced a contradiction.

*What it actually does on THIS repo:* parses **zero entries**. The skill's parser
(`<claude-dir>/skills/re-frontier/re_frontier.py`, `load()`) recognises a step as

    ### <ID> — <title>
    - status: <status>
    - deps: <ids>

but `docs/re-frontier.md` writes its steps as `## RE-05 — <title> — ` + a backticked status. A `##`
heading is parsed as an AREA, so every step in this file is invisible to the tool. Proof, on the
current file: `re_frontier.py tree` prints NOTHING and `next` reports "(none — every unblocked step is
done…)" while the document contains ~15 steps, several of them explicitly `next`.

*Why this is the dangerous kind of broken:* the failure is SILENT and the output is **uniform** — a
pass with no entries is textually identical to a pass over a healthy file. It is the exact tell this
page exists to catch, and it caught me: `check` was cited repeatedly this session as if it had
verified the frontier edits. It had verified nothing. A green check that cannot go red is worse than
no check, because work gets built on it.

*It can be shown to be blind, cheaply:* `tree` on a file with steps must print steps. It prints
nothing. That is the "can it report the OTHER answer" test, failed.

*Do not cite `check` until this is fixed.* The fix is to give each step a machine-readable
`### ID — title` header plus `- status:` / `- deps:` fields (prose can stay underneath), or to teach
the skill this file's heading dialect. Until then the frontier's dependency graph and its `⛔ hack`
debt list are maintained by hand and by reading — which is how the HACK-01 entry was found to be
invisible to `hacks` in the first place.

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
