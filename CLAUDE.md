# spider1 — working rules

A native PC port of the PSX Spider-Man (`SLUS_008.75`, USA) on the psxport framework
(`external/psxport`, a submodule). psxport statically recompiles the game's MIPS code into C — the
"substrate" — and provides the PSX platform layer, hardware backends, differential harness, and
renderer. This repo adds only the game seam plus the recompiled substrate.

This file holds durable **directives**. Findings go to `docs/issues/`, status to `docs/codemap.md`,
progress to `docs/re-frontier.md`, proven results to `docs/info/`.

---

## Start here, every non-trivial task

1. `docs/re-frontier.md` — work the step that is `next`, not a downstream one. Query it with

       python3 tools/re_frontier.py next

   **The env var is NO LONGER required — verified 2026-08-11 by running it, not by reading the code.**
   A bare invocation resolves `<repo>/docs/re-frontier.md` correctly and parses 30 entries. `ROADMAP`
   defaults to a path derived from the script's own location's PARENT, so the in-repo copy finds the
   in-repo roadmap. `RE_FRONTIER_ROADMAP=` still works and is the right override when running a copy
   from outside the repo. Use the in-repo `tools/re_frontier.py`, not a `$CLAUDE_SKILLS`-relative path:
   that variable is unset in a plain shell, so such a command collapses to `/re-frontier/re_frontier.py`
   and fails.

   `docs/info/instruments.md` INST-14 is the history, and **half of it was still live until
   2026-08-11**: the MISSING-FILE case refused correctly, but a roadmap that existed and yielded ZERO
   entries printed "re-frontier OK" and exited 0 — vacuously true over an empty set. `check` now fails
   on a zero-entry parse and `next` distinguishes "nothing is ready" from "the roadmap was never read"
   (it used to claim every unblocked step was done). Fixed in all three drifted copies.
2. `docs/codemap.md` — where the subsystem lives and its honest status.
3. `docs/issues/` — has this symptom been hit before? Has this cause been ruled out?
4. `docs/info/claims.md` + `instruments.md` — is the thing you are about to rely on actually proven,
   and can the tool you are about to measure with be trusted?

Believe these over instinct about what is already known. Write back at the END of the task — what you
proved, what you disproved, and any tool you caught lying. Read-then-write, never write-only.

## No bandaids — fix the actual cause

Name the root cause before fixing. A change that makes a symptom disappear without explaining *why it
occurred* is a bandaid.

Bandaid smells, all banned here: a magic constant or offset that makes output line up; special-casing
the one input that fails; swallowing an error; retry-until-it-passes; sleep-to-fix-a-race; a fake
completion so a wait returns; hardcoding an expected value; commenting out a failing check.

**If the RE for something is not done, the code must say so and stop** — hang, or abort with a
diagnostic naming the frontier step. It must never fabricate behaviour to look like it works. A
stalled boot is diagnosable; a boot fed garbage by a stub is not. If a real fix is genuinely too big
right now, say so plainly, name the proper fix, and let the user decide — and mark any approved
stopgap in-code as `// STOPGAP: <proper fix> because <why>` and add it to the `⛔ hack` list.

The `⛔ hack` list in `docs/re-frontier.md` is currently **empty**. Keep it that way, or shrink it.

## The picture comes from GAME STATE, never from what the GTE produced

Two checkable rules; the binding statement is `external/psxport/docs/workspace/PROTOCOL.md`. The word "tap" is retired — it
needed case-by-case adjudication every time, which is the signature of an underspecified rule.

1. **The shipping picture path runs no `gen_func_*` body.** Reads are not the problem: a producer
   reads the node's own fields, and diagnostics are exempt because they ANSWER QUESTIONS rather than
   produce the picture. The gate: a producer that runs a gen body cannot interpolate, since re-running
   it under a lerped camera would write guest RAM.
2. **Resolve from what SUBMITS to the GTE, never from what it produced.** Find the
   `SetRotMatrix`/`SetTransMatrix`/RTPS site and take its INPUTS — the game's own pre-quantisation
   values. Never invert `gte_read_ctrl()`/the OT/a composed GP0 packet to recover a transform: those
   are s16-quantised and factoring the camera back out leaves a residue that is *a function of the
   camera* (0.13 px still, 1.53 px panning, 12/12 sign alternations — a layer that "vibrated" with
   nothing in the game moving it).

Intercepting the guest's own store as it writes (the framework's `gte_store_xy` hook) is observation
at the submission boundary, not inversion — that is RE-08 and it is allowed.

**Dusklight lerps recorded matrices and we may not.** Theirs are FLOAT values from a decomp, before
any hardware; ours would be s16 GTE output. Same technique, different source — resolve from the
submitter and we are in their position. Separately, PSX has no Z-buffer (`OTZ` is a bucket index, not
a distance); that argues for native per-vertex depth and says nothing about interpolation.

## Reverse-engineer first, and cite it

Every guest address that enters this repo carries, at its definition: the instruction it was read
from, and the command that reproduces the disassembly
(`tools/redump_ram.py`, then `external/psxport/tools/disasm.py <ram.bin> <lo> <hi>`).

An address with no provenance is a guess, and guesses are how a port acquires magic constants.
`GameConfig` fields that have not been RE'd stay **zero**, with the frontier step named — zero means
"not yet known", never "not needed".

Prefer ground truth over inference. The single most useful identification in this port so far came
from a diagnostic string the binary itself emits, not from pattern-matching control flow.

## The generated substrate is sacrosanct

`generated/` is regenerated output. Never hand-edit it, never commit it. A mistranslation is fixed in
the recompiler (`external/psxport/tools/recomp/`), not in its output. Provisioning goes through
`tools/ensure_recomp.py` — one hash-gated step, so every machine builds an identical substrate.

## Where the framework source comes from — NEVER edit `external/psxport`

`external/psxport` is a **read-only pinned consumer**. Framework edits happen in the workspace's
framework DEV CLONE (`$PSX/psxport`, i.e. `../psxport` from here) and nowhere else — `run.sh` re-syncs
this submodule to the RECORDED gitlink on every run, so an edit made here is liable to be silently
reverted mid-gate, and the build or measurement that follows describes a different framework than you
think.

Build this game against in-progress framework work WITHOUT touching the submodule:

```sh
PSXPORT_DIR=$PSX/psxport ./run.sh          # or: cmake -S . -B build -DPSXPORT_DIR=$PSX/psxport
```

`PSXPORT_DIR` defaults to the submodule, so a bare clone of this repo still builds standalone — keep it
that way. `run.sh` announces which framework checkout a run was built from and whether it was dirty;
read that line before trusting any measurement. The full protocol (area claims, how a framework change
lands, the standing USER rules) is `external/psxport/docs/workspace/PROTOCOL.md`; the workspace map is
`external/psxport/docs/workspace/WORKSPACE.md`.

## Keep the framework game-agnostic

psxport carries no game code and must keep compiling standalone (`psxport_smoke`). When the framework
needs a game-specific value, route it through `GameConfig` — do not `#include` anything from
`generated/` into framework code, and do not bake an address into `runtime/`.

Where the framework already bakes one in, that is a **wart**: record it in
`docs/issues/framework-agnosticism-warts.md`, work around it through a public seam, and fix it
upstream rather than patching the submodule from here.

**Never edit, build, commit to, or bump the pin of a SIBLING consumer** (Tomba2Engine) from this
repo — not even to keep a positional `GameConfig` initialiser compiling. Each consumer pins its own
psxport commit, so a framework change cannot break a sibling until that sibling chooses to move; the
lockstep feels obligatory and is not. Add the field, update this port, push psxport, and say in the
commit message that other consumers must add the field when they bump. Their schedule, their call.
(USER directive, 2026-07-28.)

## The PC owns subsystems — it does not emulate interrupts

When a guest subsystem blocks on PSX hardware, the answer is for the PORT to own that subsystem
natively, not to reproduce the hardware faithfully enough that the guest's own ISR runs. Emulating an
interrupt controller so a guest ISR can set a completion byte is the long way round to a value the
host already knows.

Concretely: HLE the LIBRARY at a `GameConfig` chokepoint, do the real work natively (serve the read
from the disc image), and drive whatever completion the guest is waiting on. Hardware modelling is
still worth having where it is genuinely simpler or serves other consumers — but it is not the
default answer to "the guest is waiting". (USER directive, 2026-07-28.)

## Diagnostics go through `lucent::` — one line, no `if` around it

`lucent::info` / `warn` / `error` for what a normal run should print; `lucent::debug("chan", …)` for
anything channel-gated (`PSXPORT_DEBUG=chan,chan`). Include `<lucent/log.h>`; it is vendored at
`external/psxport/vendor/lucent` and already linked. Pick by AUDIENCE, not by wrapping.

**Never wrap a log call in a condition.** `lucent::debug` is channel-gated internally and does not
evaluate its arguments when the channel is off, so `if (cfg_dbg(c)) cfg_logf(c, …)` is pure noise
re-creating the `if (dbg) fprintf(…)` idiom the logger exists to abolish. The ONE legitimate guard is
around expensive NON-LOGGING work (walking a structure, building a dump), and it guards a BLOCK.

`cfg_logi/logw/loge/logf/dbg` are a printf-style shim that forwards 1:1 to lucent and is **being
retired** — add no new call sites, and convert any you touch. Format strings become `std::format`:
`%08X`→`{:08X}`, `%u`/`%zu`→`{}`, `%.2f`→`{:.2f}`, `%%`→`%`. **Trap:** `printf("%s", p)` on a null
`const char*` prints `(null)`; `std::format` on one is undefined behaviour — check every `%s`.

Never scatter raw `printf`/`fprintf` diagnostics or ad-hoc `getenv` reads through the code.

An instrument is only worth citing if it can show the OTHER answer — validate it against a case that
must differ, and record it in `docs/info/instruments.md`.

## Verify before declaring done

No "works" / "matches" / "fixed" without a real check on real data, cited. A green result means what
it exercised and nothing more — and the port currently exercises very little, so be especially
careful about generalising from a boot-only measurement to gameplay.

When the user contradicts you about the running system, they are observing and you are inferring.
Treat their correction as ground truth and investigate from "it's broken".

## Scratch and hygiene

All run artifacts go to the gitignored `scratch/`, structured by kind (`scratch/bin/`,
`scratch/logs/`, `scratch/screenshots/`) — **never `/tmp`**, which is a small RAM-backed tmpfs on the
dev machine and fills in a run or two.

## Never commit

Disc images, extracted executables, or any game asset. Machine-specific paths, home directories, or
usernames in tracked files — repo-relative paths, placeholders, or a gitignored `.env` instead.
Python bytecode (`.pyc` embeds absolute source paths, which a text-only audit cannot see).

Before publishing, run the go-public audit over the FULL history, not just the working tree.

## Reference material

The PC release has a community decompilation, `krystalgamer/spidey-decomp` — same game, same
developer, different architecture. Useful as a Rosetta stone for structure and naming when RE'ing the
PSX build. Not yet used here; if you start using it, record what it actually established versus what
was confirmed against the PSX binary, because the two builds are not identical.
