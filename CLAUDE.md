# spider — working rules

A native PC port of both Neversoft PSX Spider-Man games on the psxport framework:

- Spider-Man (`SLUS_008.75`, USA)
- Spider-Man 2: Enter Electro (`SLUS_013.78`, USA)

`external/psxport` is a symlink to the shared framework clone, pinned via `psxport.pin`. psxport
statically recompiles each game's MIPS code into C — the "substrate" — and provides the PSX platform
layer, hardware backends, differential harness, and renderer. This repo adds only the game seams plus
the recompiled substrates.

## Two-title product contract

The disc boot executable/serial is the canonical title key: `SLUS_008.75` means Spider-Man and
`SLUS_013.78` means Enter Electro. Human-facing names are labels, never selectors; do not infer a
title from a generic "Spider-Man" string, asset shape, or whichever substrate happens to exist.
A product target binds to exactly one recognized serial and must refuse an unknown or mismatched
disc instead of silently running the other title's configuration. Serial is necessary but not
sufficient: provisioning and direct target boot must also match the executable's measured file size
and SHA-256 before `Game` construction, so a renamed or stale cache cannot supply identity.

`SLUS_008.75` is the playable implementation. `SLUS_013.78` now has its own manifest, generated
substrate namespace, direct `EnterElectroRuntime`, and executable-derived crt0 facts; it deliberately
aborts at its first unported game-owned call (`0x80031F54`, EE-02). This is a verified identity/boot
boundary, not gameplay or rendering support. Shared `game/` code must remain address-free lineage
mechanism; title addresses and generated thunks belong below `titles/<id>/`. Do not claim Enter
Electro rendering, widescreen, or gameplay before its own frontier establishes those layers.

This file holds durable **directives**. Epic intent goes to `docs/project-goals.md`; factual
capability state and current focus to `docs/project-state.md`; atomic work and findings to
`docs/issues/`; placement to `docs/codemap.md`; ordered RE dependencies to `docs/re-frontier.md`;
proven results to `docs/info/`.

---

## Start here, every non-trivial task

1. `python tools/info.py brief <terms>` — one query across goals, project state, issues, ownership,
   the RE frontier, claims, and instruments. Then read `docs/project-state.md` for capability
   coverage/current focus and `docs/project-goals.md` only when product intent is relevant.
2. `docs/re-frontier.md` — work the step that is `next`, not a downstream one. Query it with

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
3. `docs/codemap.md` — which subsystem owns the responsibility and where related work belongs.
4. `docs/issues/` — has this symptom been hit before? Has this cause been ruled out?
5. `docs/info/claims.md` + `instruments.md` — is the thing you are about to rely on actually proven,
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

The `⛔ hack` list in `docs/re-frontier.md` currently contains HACK-03, the user-authorized whole
guest-frame fallback for graphics whose native producer remains unported. Burn it down as native
producers land; do not let it silently become architecture.

## Native producers come from GAME STATE; the guest-output fallback is explicit debt

Two checkable rules; the binding statement is `external/psxport/docs/workspace/PROTOCOL.md`. The word "tap" is retired — it
needed case-by-case adjudication every time, which is the signature of an underspecified rule.

1. **A native producer runs no `gen_func_*` body.** Reads are not the problem: a producer
   reads the node's own fields, and diagnostics are exempt because they ANSWER QUESTIONS rather than
   produce the picture. The gate: a producer that runs a gen body cannot interpolate, since re-running
   it under a lerped camera would write guest RAM.
2. **Resolve a native producer from what SUBMITS to the GTE, never from what it produced.** Find the
   `SetRotMatrix`/`SetTransMatrix`/RTPS site and take its INPUTS — the game's own pre-quantisation
   values. Never invert `gte_read_ctrl()`/the OT/a composed GP0 packet to recover a transform: those
   are s16-quantised and factoring the camera back out leaves a residue that is *a function of the
   camera* (0.13 px still, 1.53 px panning, 12/12 sign alternations — a layer that "vibrated" with
   nothing in the game moving it).

USER-AUTHORIZED EXCEPTION, 2026-08-21: graphics whose native producer remains unported may be drawn
from the ACTUAL guest-time GTE result / guest packets, but MUST NOT be interpolated. Spider-Man's
HACK-03 implements only a mutually-exclusive WHOLE guest frame through the retail submit body under
`RenderPath::Gte`; it skips all native producers for that frame and refuses FPS60 or prior native
submission. This does not make guest output a valid native-producer input and does not advance
RE-21. A future mixed frame needs proven packet ownership and mechanical no-double-draw control;
never add a second packet parser, guess missing state, or interpolate packet output.

Intercepting the guest's own store as it writes (the framework's `gte_store_xy` hook) is observation
at the submission boundary, not inversion — that is RE-08 and it is allowed.

**Dusklight lerps recorded matrices and we may not.** Theirs are FLOAT values from a decomp, before
any hardware; ours would be s16 GTE output. Same technique, different source — resolve from the
submitter and we are in their position. Separately, PSX has no Z-buffer (`OTZ` is a bucket index, not
a distance); that argues for native per-vertex depth and says nothing about interpolation.

The host ownership mapping follows Dusklight's current `dusk/frame_interpolation.*`,
`dusk/game_clock.*`, and camera boundary: simulation records stable game-state identities; a separate
temporal owner builds presentation samples; the render loop consumes those samples; the title camera
owner publishes aspect/projection. For this port, `mesh_pose_contract.*` is only the source decoder,
`mesh_pose_history.*` is the separate temporal owner, the native mesh producer consumes it, and
`spider_projection.*` owns title projection. Do not collapse these into `render_seam.cpp`.

Spider-Man's matching record-and-replace boundary starts inside `FUN_80077198`, before
`FUN_8007FB1C/FUN_8007FD1C` submit their three transform records to the GTE. Object transforms are
identified by the display-object address plus the stable authored pose-record address; camera pose
and title projection remain separate owners. `game/render/mesh_pose_contract.*` owns the pure input
decode. A future temporal store belongs in its own `game/render/` module and the render seam only
composes it; neither HACK-03 nor a post-GTE matrix may seed that store.

## Reverse-engineer first, and cite it

Every guest address that enters this repo carries, at its definition: the instruction it was read
from, and the command that reproduces the disassembly
(`tools/redump_ram.py`, then `external/psxport/tools/disasm.py <ram.bin> <lo> <hi>`).

An address with no provenance is a guess, and guesses are how a port acquires magic constants. The
remaining legacy configuration facts that have not been RE'd stay **zero**, with the frontier step
named — zero means "not yet known", never "not needed". New behavior and policy belong on the
appropriate title-derived runtime; do not grow `GameConfig` or `GameHooks`.

Prefer ground truth over inference. The single most useful identification in this port so far came
from a diagnostic string the binary itself emits, not from pattern-matching control flow.

## The generated substrate is sacrosanct

`generated/` is regenerated output. Never hand-edit it, never commit it. A mistranslation is fixed in
the recompiler (`external/psxport/tools/recomp/`), not in its output. Provisioning goes through
`tools/ensure_recomp.py` — one hash-gated step, so every machine builds an identical substrate.

## Where the framework source comes from — `external/psxport` is the shared tree

`external/psxport` is **not a submodule** (2026-08-16): it is a SYMLINK to the workspace's shared
framework clone (`$PSX/psxport`) when one exists, or a private clone at this repo's `psxport.pin` on a
fresh machine. `tools/psxport_sync.py --auto` (called by `run.sh`) establishes whichever applies. So a
framework edit made through either path is the SAME directory, live in every port at once — commit and
push framework work in `psxport/`, never here. `psxport.pin` records the framework commit this game
was built and VERIFIED against; `tools/psxport_sync.py --bump` updates it, and the gate's `--check`
fails when the framework you built against is not the recorded one.

Build this game against in-progress framework work:

```sh
cmake -S . -B build -DPSXPORT_DIR=$PSX/psxport   # or just ./run.sh — it resolves external/psxport itself
```

`PSXPORT_DIR` defaults to `external/psxport`, so a bare clone of this repo still builds standalone —
keep it that way. `run.sh` announces which framework checkout a run was built from and whether it was
dirty; read that line before trusting any measurement. The full protocol (area claims, how a framework
change lands, the standing USER rules) is `external/psxport/docs/workspace/PROTOCOL.md`; the workspace
map is `external/psxport/docs/workspace/WORKSPACE.md`.

## Keep the framework game-agnostic

psxport carries no game code and must keep compiling standalone (`psxport_smoke`). When the framework
needs a game-specific value, expose a narrow typed runtime fact interface and implement it in the
title-derived runtime; do not `#include` anything from `generated/` into framework code, bake an address
into `runtime/`, or add another field to the legacy configuration bag.

Where the framework already bakes one in, that is a **wart**: record it in
`docs/issues/framework-agnosticism-warts.md`, work around it through a public seam, and fix it
upstream rather than patching the submodule from here.

**Never edit, build, commit to, or bump the pin of a SIBLING consumer** (Tomba2Engine) from this
repo — not even to keep a legacy compatibility table compiling. Each consumer pins its own
psxport commit, so a framework change cannot break a sibling until that sibling chooses to move; the
lockstep feels obligatory and is not. Add the typed interface, update this port, push psxport, and
say in the commit message that other consumers must implement the interface when they bump. Their
schedule, their call.
(USER directive, 2026-07-28.)

## The PC owns subsystems — it does not emulate interrupts

When a guest subsystem blocks on PSX hardware, the answer is for the PORT to own that subsystem
natively, not to reproduce the hardware faithfully enough that the guest's own ISR runs. Emulating an
interrupt controller so a guest ISR can set a completion byte is the long way round to a value the
host already knows.

Concretely: let the title-derived runtime install the native owner at a binary-proven library chokepoint, do
the real work natively (serve the read from the disc image), and drive whatever completion the guest
is waiting on. Hardware modelling is
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

**The run gate is `python3 tools/gate.py boot`, and an agent NEVER runs `./run.sh`** — that is the
user's windowed play launcher and it re-syncs the framework submodule out from under an in-progress
measurement. Build explicitly (`cmake --build build --target spiderman_port -j$(nproc)`), then gate.
The gate drives the already-built binary headless, capped, and keys on the port's
own log lines — it cannot use the framework REPL, because this port never enters the frame loop that
services it. `--selftest` proves it still fails when it should; `check-log <path>` re-judges a captured
log. Refusals exit 2, GPU device loss exits 3. What it does NOT cover: pixels, the pc_render leg,
audio, input (INST-28 states the blind spots).

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
