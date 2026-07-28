# Codemap — spider1

The orientation map: what is where, what is done, what is missing. Consult at the START of a task to
avoid re-deriving structure; update in the SAME commit that lands or changes a subsystem.

A subsystem is marked **done** only when VERIFIED on real data. Nothing is marked done to look
better — an honest "missing" is what makes this file worth reading.

---

## What this project is

A native PC port of the PSX Spider-Man (`SLUS_008.75`, USA) built on
[psxport](https://github.com/SomeoneIsWorking/psxport): the game's MIPS code is **statically
recompiled** into C (the "substrate"), which runs under a native platform layer — so the result is a
PC program, not an emulator. Native reimplementation then grows function by function on top, each
step gated against the recompiled reference.

The framework is a submodule and carries no game code. This repo adds only the game seam and the
recompiled substrate.

## Repo layout

```
game/core/           the framework↔game seam (this repo's entire hand-written surface, today)
external/psxport/    the framework, as a submodule (recompiler, runtime, PSX HW, harness, renderer)
generated/           the recompiled substrate — REGENERATED, never committed, never hand-edited
tools/               provisioning + RE helpers
cmake/               the port build definition
docs/                this map, the RE frontier, the issue catalog, the claims/instruments ledgers
scratch/             all run artifacts (gitignored) — logs, dumps, the extracted executable
```

## Subsystem status

| Subsystem | Where | Status |
|---|---|---|
| Disc provisioning + static recompilation | `tools/ensure_recomp.py`, `game/recomp_seeds.json` | **done** — hash-gated; 1561 functions, 8 shards, 0 overlays, seeded only from the binary |
| Build (framework + game + substrate) | `CMakeLists.txt`, `cmake/spiderman_port.cmake` | **done** — configures and links clean |
| `GameConfig` boot/crt0 group | `game/core/game_config.cpp` | **done** — RE-verified against the crt0 at `0x8008739C` |
| `GameConfig` everything else | `game/core/game_config.cpp` | **missing** — deliberately zero; see `docs/re-frontier.md` |
| Generated-substrate seam | `game/core/recomp_register.cpp` | **done** |
| `GameHooks` vtable | `game/core/game_hooks.cpp` | **done for Phase 0** — neutral where nothing is owned, fail-fast where a path is not stood up |
| Boot spine | `game/core/main.cpp` | **done for Phase 0** — boots to the guest's own `main` on the substrate |
| libetc `VSync` | `game/core/sync_native.cpp` | **done** — RE-verified, native, real-time-driven counter |
| libcd `CdInit` | — | **missing** — the current stopping point (`re-frontier` RE-03) |
| Native frame loop / OT | — | **missing** — blocked on RE-03 |
| Scheduler | — | **missing** — blocked; the SDK task model may not apply to this engine |
| Input | — | **missing** — framework override installed, no game buffers RE'd |
| Renderer (native depth / wide / interpolation) | — | **not started** — `re-frontier` RE-08 |
| Audio | — | framework SPU is up; no game-side music engine is owned |

## Where is X

- **The RE'd guest addresses** → `game/core/game_config.cpp`, each cited with its instruction.
- **Why a value is zero** → `docs/re-frontier.md`, by step.
- **What is proven and whether it still holds** → `docs/info/claims.md`.
- **Whether a measurement tool can be trusted** → `docs/info/instruments.md`.
- **A bug or a ruled-out cause** → `docs/issues/`.
- **How to disassemble a guest address** → `tools/redump_ram.py`, then the framework's `disasm.py`.
- **Why a recompiler seed exists** → `game/recomp_seeds.json` (empty by design; every entry needs a rationale).
- **The framework's porting methodology** → `external/psxport/docs/porting-a-new-psx-game.md`.

## Current state in one line

The port provisions, recompiles, builds, and boots: crt0 → the guest's `main` → graphics init → and
stops in the game's own disc-init retry loop, because libcd `CdInit` has not been reverse-engineered
yet. No hacks stand in for that; it hangs honestly.
