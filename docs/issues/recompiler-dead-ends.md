# Recompiler dead ends — ruled out, do not re-derive

Negative results about the MIPS→C translation. Each entry exists so a future session does not spend a
tick re-discovering that something is *not* a bug.

---

## DE-01 — "The recompiler ignores branch-likely (beql/bnel/blezl/bgtzl)" — **NOT A DEFECT**

*The worry:* branch-likely instructions NULLIFY their delay slot when the branch is not taken, so
emitting the delay slot unconditionally would be wrong. `decode.py` has no case for opcodes
`0x14`–`0x17`, and a raw scan of the executable's text range found **161** words whose top 6 bits
match them (beql 80, bnel 56, blezl 14, bgtzl 11). That looks damning.

*Why it is not:* **the PSX CPU is an R3000A, which is MIPS-I. Branch-likely is MIPS-II.** The
instructions do not exist on this hardware, so those words cannot be code.

*Confirmed, rather than argued:* none of the candidate addresses is emitted as code. Checking the
generated substrate for a `L_<addr>` label or a `func_<addr>` entry at each of the first six
(`0x80091AF4`, `0x80091B48`, `0x80091C24`, `0x80091C9C`, `0x80091CBC`, `0x80091CEC`) finds nothing,
and they cluster in `0x8009xxxx` — the string/table data region. They are data being read as opcodes
by a scan that swept the whole text range.

*The reusable lesson, which is already a project rule:* **a grep count is text, not code.** Counting
raw word patterns across a text range counts constants, strings and jump tables alongside
instructions. Before treating a count as a defect surface, confirm the addresses are REACHED — an
emitted label, a call site, or a fire counter on a real run. This one was one step from being
reported as a recompiler bug.

*What would reopen this:* a candidate address that IS emitted as code, or a port of a MIPS-II console.

---

## DE-02 — RE-16's fix: three attempts at "is this `jal` target a real function?", all wrong

The RE-16 diagnosis is solid (see the frontier): an unconditional `bgez $zero` to a fall-through
`jal` target emits `call_or_dispatch(...) + return;`, so the enclosing function skips its shared
epilogue and leaks 4 bytes of guest stack per call. The **fix** is a discovery-pass question — when is
a `jal` target an internal subroutine rather than a function? — and three criteria have now failed.
Recorded so the next attempt starts from attempt 4.

The emitter side produced the RIGHT-LOOKING code first time — with the target demoted, the leak site
became `goto L_8002A478`, a `jal` to an in-body label became `link + goto`, and `jr $ra` in a body
that loads `$ra` as data became a switch over that body's labels with `default: return;`.
**Correction (attempt 4): "right-looking" was not "right". The emitter side is where the remaining
bug is.**

**Attempt 1 — "no prologue AND fall-through reachable".** Demoted **78** targets in MAIN and broke the
link: it took `StGetNext` (`0x80086B10`) with it, a real library function the port itself super-calls
(`game/core/cd_stream.cpp`). Being preceded by a non-terminator is far too common to mean anything on
its own. *Note the asymmetry that made this loud rather than silent:* a guest `jal` to a demoted
function degrades to a runtime dispatch miss, but the port's own C code fails at LINK — which is why
only 3 undefined references appeared for what was a much broader breakage.

**Attempt 2 — add "all `jal` callers are in the same enclosing body".** Demoted **nothing**. The
enclosing-function lookup was `bisect` over the current function list, which still CONTAINS the
candidate — so any caller sitting *after* the candidate resolved to the candidate itself rather than
to the real host, and the same-body test could never pass for exactly the shape it was meant to find.

**Attempt 3 — same, with the candidate excluded from the lookup.** Demoted **1**, kept `StGetNext`,
and the build was clean — but it demoted a *different* function than the one RE-16 needs
(`gen_func_8002A478` still emitted), and it **regressed a previously working run**: the `DOWN` path
started fail-fasting where it had rendered the menu. Reverted.

**What the next attempt should carry:**
- The three `jal` sites targeting `0x8002A478` are `0x8002A41C`, `0x8002A700`, `0x8002A76C` — all
  inside the host body `0x8002A338`, so the same-body criterion is *correct in principle*; attempt 3
  demoted something else, meaning the predicate is matching cases it should not.
- Verify the criterion by asserting the demoted SET before regenerating — print it and check
  `0x8002A478 ∈ set` and `0x80086B10 ∉ set`. All three attempts were validated only by build/run,
  which is far too coarse and slow a signal for a set-membership question.
- The regression gate is cheap and it is what caught attempt 3: menu renders ~99%
  (`PSXPORT_SHOT_AT=2301` + `PSXPORT_FORCE_BUTTONS=0040`), no `FATAL`, no `recomp-MISS`.

*Meta-lesson, and it is the expensive one:* this was attempted on a pass where the risk had already
been assessed as too high, and it played out exactly as predicted — a regression, then a revert. The
diagnosis being certain does not make the fix low-risk when the fix is a heuristic over function
discovery.

### Attempt 4 — the discovery criterion is SOLVED; the emitter side is not

Ran the criterion as a standalone script against `generated/rec_decls.h` + the RAM image, asserting
set membership **before** regenerating — the method DE-02 said to adopt. It cost seconds per iteration
instead of a build, and it found the answer immediately.

**Why attempt 3 demoted the wrong function:** `0x8002A478`'s three `jal` callers resolve to hosts
`0x8002A338`, `0x8002A5F4`, `0x8002A5F4` — and `0x8002A5F4` is ITSELF a spurious entry inside the
same real body. Spurious entries cluster, and each one hides the next.

**The criterion, now correct:** no prologue **AND** fall-through reachable **AND** every `jal` to it
resolves to the same host — with the candidate excluded from the host lookup, iterated to a
**FIXPOINT**. On this binary it converges in 2 iterations and yields exactly
`{0x8002A5F4, 0x8002A478}`: the target RE-16 needs is in, and `StGetNext` (`0x80086B10`) is out.
Both membership assertions pass.

**And the port still regressed.** With that exact set demoted, the build is clean but the
`PSXPORT_FORCE_BUTTONS=0040` run fail-fasts twice and renders no frame. Reverted.

So the remaining defect is in **one of the three emitter changes**, not in discovery:
1. branch/`j` to an in-body label → `goto`
2. `jal` to an in-body label → `link + goto`
3. `jr $ra` → switch over the body's labels when the body writes `$ra` as data

**Attempt 5 should isolate which**, by landing them ONE AT A TIME against the regression gate rather
than together. Suspicion, untested: (3) is the most likely — the switch is emitted over *every* label
in the body and `default: return;`, so a `$ra` that legitimately holds a caller address whose low
bits happen to collide with an in-body label would jump into the middle of this function instead of
returning. A tighter version would switch only over labels that are actually reachable as resume
points (the instruction after each in-body `jal`), which for this body is a set of 3, not 35.

### Attempt 5 — the `jr $ra` breadth was NOT the cause either

DE-02 recorded an untested suspicion: the `jr $ra` switch covered every label in the body with
`default: return;`, so a `$ra` legitimately holding a caller address whose low bits collided with an
in-body label would jump into the middle of the function. Tested by restricting the switch to genuine
RESUME POINTS only — the instruction after each in-body `jal`, which for `0x8002A338` is 3 addresses
rather than 35.

**Still regresses.** Clean build, correct demotion set (`0x8002A478`, `0x8002A5F4`), and the
`FORCE_BUTTONS=0040` run still fail-fasts and renders no frame. Suspicion falsified — do not retry it.

So of the three emitter changes, the remaining defect is in (1) branch/`j` → `goto` or (2) `jal` →
`link + goto`, or in an interaction none of the three isolates. Attempt 6 should land them
**individually**, which requires a way to demote WITHOUT the other two — e.g. demote and emit only the
branch change, accepting that the `jal` sites will fail-fast at runtime, and check whether the
*earlier* part of the boot still renders. A fail-fast at a known later point is a usable signal; a
regression that renders nothing is not.

*Method note that keeps paying off:* the standalone set-assertion (attempt 4) reduced a build-cycle
question to seconds. The equivalent for the emitter side would be to diff the generated C for
`gen_func_8002A338` between builds and read what actually changed, rather than inferring from a
run — that diff has never been examined, and it is the obvious next cheap instrument.

### Attempt 6 — read the generated DIFF; parts 1+2 alone are not enough (and could not be)

Used the instrument attempt 5 recorded: extract `gen_func_8002A338` from `generated/shard_7.c` before
and after, and READ the diff instead of inferring from a run. It cost one regenerate and was worth it.

**The emission is correct.** The demoted region inlines into the enclosing body with proper labels:

    -  c->r[9] = c->r[0] | 10u; func_8002A478(c);
    +  c->r[9] = c->r[0] | 10u; goto L_8002A478;
    -  { int _t = ...; if (_t) { func_8002A478(c); return; } }
    +  { int _t = ...; if (_t) goto L_8002A478; }
    +L_8002A478:;   ... 24 lines of inlined body, branching back to L_8002A444 / L_8002A4C0 ...

and the inlined code reads as a coherent bit-unpacking loop (shift counts in `r9`/`r10`, an input
cursor in `r4`, an output cursor in `r5`) — not garbage, not misplaced.

**Parts 1+2 without part 3 still fault**, at `0x8183D114` — the SAME address the full three-part
change produced. That is worth stating carefully: it is NOT a clean isolation. With part 3 absent,
`jr $ra` inside the demoted region still emits `return;`, so the resume path is broken by
construction and this variant could never have worked. What it does show is that part 3 is not the
*sole* cause, and that the fault address is stable across variants — which is itself a lead.

**Where attempt 7 should start.** The three parts are mutually dependent, so "land them one at a
time" (attempt 5's plan) is not actually possible — that plan is now known to be unachievable and
should not be re-attempted as stated. Instead:
- The fault address `0x8183D114` is CONSTANT across the failing variants. Chase THAT: capture the
  guest context at it (the unmapped fail-fast prints registers) and identify which pointer it is. It
  is a different address from RE-16's original `0x80800004`, so the three-part change moves the
  failure rather than merely failing to fix it — that is progress information, not noise.
- The inlined body branches to `L_8002A444` and `L_8002A4C0`. Verify those labels exist in the SAME
  emitted body and are not duplicated tails belonging to another function; a `goto` into a duplicated
  tail would be a plausible mechanism for a stable wrong address.

*Standing methodological win:* both cheap instruments this saga produced — the standalone demoted-set
assertion (attempt 4) and the generated-C diff (attempt 6) — each answered in seconds what a
build-and-run answered in minutes and ambiguously. Reach for them first.

### Attempt 7 — guest context at the constant fault address

Captured the registers at `0x8183D114` with the full three-part change applied:

    guest: last-fn-entered=0x80074C98 (NOT the faulting pc) ra=0x800692CC sp=0x807FFDEC
           gp=0x800B47F4 fp=0x00000004
    args : a0=0x8005E720 a1=0x00001FFF a2=0x8011BBDC a3=0x00000000 v0=0x8183D104 v1=0x0013530C
    temps: t0=0x80119BDD t1=0x00000001 t2=0x00000001 t3=0x00000041 s0=0x00000000 s1=0x00000001

**`v0 = 0x8183D104` is the corrupt pointer** — the fault reads `v0 + 0x10`. So this is a structure
field access through a bad base, not a runaway scan (contrast RE-16's original `0x80800004`, which was
a byte-at-a-time walk off the stack top).

**The most suggestive value is `a0 = 0x8005E720`.** That is a CODE address, sitting just below the
field-wait primitive `0x8005E748`, being passed as an argument — the shape of a function-pointer table
walk with a corrupted base, or of a code pointer used where a data pointer was expected.

`fp = 0x00000004` is also wrong (the game holds `$fp` at `0x800B0000` as a global base — see RE-11),
so a callee-saved register is again being lost. That points back at a frame-balance problem rather
than at the emission of the demoted region itself, which attempt 6 verified reads correctly.

**Attempt 8 starting points, in order:**
1. Disassemble around `ra=0x800692CC` and `last-fn-entered=0x80074C98` to find the structure access
   that dereferences `v0 + 0x10`, and what is supposed to produce `v0`.
2. `fp` being clobbered again is the strongest thread: the three-part change was supposed to STOP a
   frame leak, and a different callee-saved register is now wrong. Check whether the demoted region's
   inlined code contains a path that reaches the shared epilogue TWICE (releasing 4 bytes twice), which
   would be the mirror image of the original bug and would explain a stable-but-different corruption.
3. Confirm `0x8002A5F4` genuinely belongs in the demoted set. It was demoted first and its own callers
   were never audited the way `0x8002A478`'s three were.

### Attempt 8 — what `0x8002A338` actually IS: a resumable coroutine with a saved continuation

Attempts 1–7 all treated `0x8002A338` as an ordinary function containing a stray internal subroutine.
It is not. Disassembling the whole body settles it, and this is the fact every prior attempt lacked.

**It saves and restores its own register context, including its continuation, in a global block at
`0x80097D88`.**

    ; entry, $a0 == 0 -> RESUME path
    8002A340  lui/addiu $t6, 0x80097D84
    8002A358  lui/addiu $t6, 0x80097D88
    8002A360..8002A38C  lw  $v0,$v1,$t0,$a0,$a1,$a2,$t2,$t3,$t5,$t7,$t8,$t9  <- 0x00..0x2C($t6)
    8002A390  lw   $ra, 0x30($t6)      <- THE SAVED CONTINUATION
    8002A398  bnez $ra, 0x8002a43c     <- a continuation exists: re-enter the decode loop
    8002A3A0  bgez $zero, 0x8002a7a0   <- none: go straight to the tail

    ; exit, suspend path
    8002A7E8  bgez $zero, 0x8002a7f4
    8002A7EC  or   $at, $zero, $ra     <- delay slot: $at = the LIVE continuation
    8002A7F0  move $at, $zero          <- other entry: no continuation
    8002A7F4  lw $ra,($sp) / addi $sp,$sp,4        <- real epilogue
    8002A804..8002A830  sw  $v0..$t9   -> 0x00..0x2C($t6)
    8002A834  sw   $at, 0x30($t6)      <- THE CONTINUATION IS STORED BACK
    8002A838  jr   $ra                 <- real return; 8002A83C ori $v0,$zero,1 (returns 1 = suspended)

So `$a0 != 0` means "start on a new input buffer", `$a0 == 0` means "resume where you left off".
This is hand-written assembly (GTE `mfc2`, bit-unpacking, shared frame) using `jal`/`jr $ra` as an
internal coroutine mechanism. **`jr $ra` in this body does not mean "return".**

**There are three `jr $ra` in the body and they are NOT the same kind:**

| addr | kind | where its `$ra` came from |
|---|---|---|
| `0x8002A460` | coroutine jump | an in-body `jal` link, or the restored `0x30($t6)` |
| `0x8002A7E0` | real return | `0x8002A7C0  lw $ra, ($sp)` — the epilogue |
| `0x8002A838` | real return | `0x8002A7F4  lw $ra, ($sp)` — the epilogue |

Every prior attempt used a **whole-body** gate ("this body writes `$ra` as data, so convert *all* its
`jr $ra` to dispatch"). The discriminator is not a body property — it is **reaching-definitions on
`$ra` at each `jr`**: a real return iff the reaching def is the frame reload; a computed jump iff it
is a `jal` link or a data load. That is structural, not heuristic, and no attempt has used it.

**The resume-point set is SEVEN, not three.** Enumerated from the disassembly, the in-body `jal`s are:

    8002A41C jal 8002A478 -> 8002A424      8002A774 jal 8002A5F4 -> 8002A77C
    8002A700 jal 8002A478 -> 8002A708      8002A77C jal 8002A5F4 -> 8002A784
    8002A76C jal 8002A478 -> 8002A774      8002A784 jal 8002A5F4 -> 8002A78C
                                           8002A78C jal 8002A5F4 -> 8002A794

Attempt 5 restricted the switch to "a set of 3" — the three `jal`s targeting `0x8002A478` only. The
four resume points behind the `jal`s to `0x8002A5F4` would have hit `default: return;` and silently
truncated the decoder. **That is a concrete defect in attempt 5, and it means attempt 5's falsification
of the `jr $ra` breadth suspicion was measured on a broken variant — its negative result does not
stand.** (The set is also closed under the continuation: the value stored at `0x2A834` is whatever
`$ra` held, which is always one of these seven or zero.)

**The emitter also duplicates overlapping tails, which is why this was so confusing to read.** The
same guest instructions are emitted THREE times today:

    gen_func_8002A338  (generated/shard_7.c:3111)  spans 8002A338..8002A83C
    gen_func_8002A478  (generated/shard_0.c:3004)  spans 8002A478..8002A5F0 + a copy of 8002A444..
    gen_func_8002A5F4  (generated/shard_1.c:4479)  spans 8002A5F4..8002A79x

`gen_func_8002A478` contains **no `r[29]` operation at all** — confirming RE-16's leak: the frame is
allocated in `gen_func_8002A338` and the callee copy never releases it.

**RE-16's diagnosis is understated, not wrong.** At the `bgez $zero` sites emitted as
`func_8002A478(c); return;` there are TWO defects, not one: the frame leak, *and* control-flow
truncation — the guest's `jr $ra` at `0x8002A460` is emitted as `return;`, so instead of resuming at
`0x8002A424`/`0x8002A708`/… the decoder unwinds out of `gen_func_8002A338` entirely, never reaching
the tail that saves context and returns its status word. The `jal` at `0x8002A41C` only works today by
*accident* — C call/return happens to coincide with the guest's link/resume there.

**Attempt 9 therefore starts from a different place than attempt 8 did:**
1. Part 3's gate must be per-`jr`, by reaching-definitions on `$ra`, not per-body.
2. The resume set must be enumerated over ALL in-body `jal`s after the demotion fixpoint, not per
   demoted target. Assert `|set| == 7` for this body before regenerating — the attempt-4 method.
3. Attempt 5's negative is void; the breadth question must be re-measured once (2) is right.

### Attempt 9 — the coroutine `jr $ra` SHIPS; the fault it exposes is upstream of it

Abandoned the demotion heuristic entirely. Attempts 1,2,3 plus two more in this session all tried to
answer "is this `jal` target a real function?" and produced demotion sets of 1, 78, 325 and 50 — the
question is not reliably answerable by a peephole over the function set, and every wrong answer breaks
a real library function or misses the target.

**The recompiler already had the mechanism.** `main_reentry` seeds mid-function re-entry points and
makes them router-dispatchable. So the shape of the fix is not "merge the subroutine into its host"
but "let `jr $ra` be a computed jump, and seed the addresses it can compute":

1. `emit.py ra_computed_jumps` — per-`jr` reaching-definitions on `$ra`. **1 site out of 1722** in the
   whole substrate: `0x8002A460`. Everything else keeps `return;`.
2. `game/recomp_seeds.json` — the seven resume points, in **both** `main` and `main_reentry`.

*Two false-positive classes, both found by auditing the sites the analysis reported rather than by a
build, and both worth knowing:*
- `move $ra, $sN` restore never cleared the preceding `jal` state (10 sites → 6).
- A **frameless** function saves `$ra` to a GLOBAL and reloads it — `0x8008BE5C: sw $ra, 0x392C($at)`
  … `lw $ra, 0x392C($ra)` (6 → 1). **"Not the stack" is not the same as "not a return address."**
  The discriminator is whether that function ever stored `$ra` to that offset.

*A trap that cost real time:* **`main_reentry` is a MODIFIER, not a seed source.** `emit.py` builds its
seed set from `main` alone; `main_reentry` only makes the preceding fragment fall through instead of
returning. An address listed only under `main_reentry` is silently not a function. The tell was the
regeneration reporting 1672 functions — *exactly* the pre-change count. Listing them in both gives 1679.

**Result: the port gets FURTHER than any previous attempt.** Clean build, and the
`PSXPORT_FORCE_BUTTONS=0040` run RENDERS A FRAME (`scratch/screenshots/shot_2301.ppm`), where attempts
4–7 rendered none. One FATAL remains, and it is a different one:

    [hle] [miss 0] addr 0x03FF03FF   (caller ra=0x03FF03FF  c->pc=0x8002A478  a0=0x801C9F4C)
    [mdec:error] DMA0 in: decoder still cannot accept after 4096 step(s); 4 of 1824 word(s) written
    [mem:error] FATAL: UNMAPPED RAM read8 @ 0x080252D4

The dispatch fires at `0x8002A460` with `$ra = 0x03FF03FF`, which is **not** a resume point — it is
bit-stream data. `s0=0x03FF07FF`, `s1=0x47FF03FF`, `fp=0x03FF0FFF` are the same shape, and `0x3FF` is
the decoder's own 10-bit mask (`8002A474  ori $t0, $zero, 0x3ff`). So the routine is decoding garbage
and its saved continuation is garbage with it.

**Two lines above it is the likely reason, and it is nothing to do with the recompiler:**

    [cd:error] CdSearchFile: '/CINEMAS/TTSLOGO.STR;1' not found on the disc image

The guest asked for the logo FMV, did not get it, and ran the MDEC bit-stream decoder over memory that
was never filled. Before this change `jr $ra` was `return;`, which SWALLOWED the garbage continuation
silently; it now fail-fasts and names it. **That is the intended direction** — the guest would itself
have jumped to `0x03FF03FF` — but it does convert a silent survival into a hard stop, so it must not be
called a clean pass until the A/B below is read.

*Open, and the next thing to do:* an A/B against the pre-change emitter (`git -C external/psxport
checkout HEAD~1 -- tools/recomp/emit.py`, regenerate, rebuild, same command) to establish whether the
`TTSLOGO` miss and the MDEC error are present there too. If they are, this fault is pre-existing and
attempt 9 merely made it audible; if they are not, the change is implicated after all. **Do not record
attempt 9 as verified until that A/B is read** — "it got further" is not "it is correct".

*Bounded risk worth stating even if the A/B is clean:* the coroutine resume maps a guest `jal`/`jr $ra`
pair (O(1) guest stack) onto a C call that only unwinds when the whole routine finishes, so a decoder
that resumes N times costs N C frames. Depth was shallow in this run (`native_boot_run` →
`gen_func_8002C354` → `gen_func_8006BF9C`), but a long decode has not been measured.
