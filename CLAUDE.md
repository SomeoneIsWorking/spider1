# spider1 — working rules

A native PC port of the PSX Spider-Man (`SLUS_008.75`, USA) on the psxport framework
(`external/psxport`, a submodule). psxport statically recompiles the game's MIPS code into C — the
"substrate" — and provides the PSX platform layer, hardware backends, differential harness, and
renderer. This repo adds only the game seam plus the recompiled substrate.

This file holds durable **directives**. Findings go to `docs/issues/`, status to `docs/codemap.md`,
progress to `docs/re-frontier.md`, proven results to `docs/info/`.

---

## Start here, every non-trivial task

1. `docs/re-frontier.md` — work the step that is `next`, not a downstream one.
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

## Diagnostics go through the framework's channel logger

`cfg_logi` / `cfg_logw` / `cfg_loge` for what a normal run should print; `cfg_dbg("chan")` +
`cfg_logf("chan", …)` for anything channel-gated (`PSXPORT_DEBUG=chan,chan`). Never scatter raw
`printf`/`fprintf` diagnostics or ad-hoc `getenv` reads through the code. One diagnostic, one line,
one channel.

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
