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
