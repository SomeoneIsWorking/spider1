# spider1 — a native PC port of Spider-Man (PSX)

A work-in-progress native PC port of the PlayStation *Spider-Man* (`SLUS_008.75`, USA), built on the
[**psxport**](https://github.com/SomeoneIsWorking/psxport) framework.

psxport **statically recompiles** the game's MIPS R3000A machine code into native C, then runs it
under a native platform layer — so the result behaves like a PC program, not an emulator. On top of
that substrate, the game's own engine is reimplemented natively function by function, each step
verified against the recompiled reference.

**No disc image, executable, or game asset is included or distributed here.** Supply your own
legally-obtained copy.

---

## Status — honest

The port **provisions, recompiles, builds, and boots**. It runs the game's real translated code
through crt0 into the guest's own `main` and through graphics init, then **stops in the game's own
disc-init retry loop**, because libcd `CdInit` has not been reverse-engineered yet.

It is not playable, and there is no picture. Nothing stands in for the missing reverse-engineering:
where the RE is not done, the port hangs or aborts loudly rather than fabricating behaviour. The
`⛔ hack` list in [`docs/re-frontier.md`](docs/re-frontier.md) is empty, and keeping it empty is the
point.

| | |
|---|---|
| Static recompilation | 1561 functions, 8 shards, 0 overlays — hash-gated, reproducible, seeded only from the binary |
| Build | configures and links clean |
| crt0 / boot seam | reverse-engineered and verified live |
| libetc `VSync` | reverse-engineered and reimplemented natively |
| libcd `CdInit` | **the current stopping point** |
| Frame loop, scheduler, input, renderer | not started — blocked behind libcd |

See [`docs/codemap.md`](docs/codemap.md) for the full subsystem map and
[`docs/re-frontier.md`](docs/re-frontier.md) for the ordered work queue.

---

## Requirements

- **Linux:** `cmake`, `pkg-config`, `SDL3`, `libzstd`, `zlib`, `python3`, a C/C++ toolchain
- **macOS:** `brew install cmake pkg-config sdl3 zstd zlib python3`
- A Vulkan-capable GPU + drivers
- Your own Spider-Man (PSX, USA) disc image as a `.chd`

## Running

```sh
git clone --recursive <this repo>
cd spider1
./run.sh /path/to/"Spider-Man (USA).chd"
```

`run.sh` does everything end to end: builds the CHD tooling, extracts the boot executable from your
disc, statically recompiles it, builds the port, and launches it. Instead of passing the path you can
set `PSXPORT_SPIDERMAN_DISC`, copy `.env.example` to `.env`, or drop a `*.chd` in the repo root.

Useful knobs: `PSXPORT_NOWINDOW=1` (headless), `PSXPORT_NOAUDIO=1`, `PSXPORT_WATCHDOG=<sec>`,
`PSXPORT_DEBUG=vsync` (channel-gated diagnostics).

## Layout

```
game/core/           the framework↔game seam — this repo's entire hand-written surface
external/psxport/    the framework (submodule): recompiler, runtime, PSX hardware, harness, renderer
generated/           the recompiled substrate — regenerated, never committed, never hand-edited
tools/               provisioning + reverse-engineering helpers
docs/                codemap, RE frontier, issue catalog, claims + instruments ledgers
```

## Working rules

The full rules are in [`CLAUDE.md`](CLAUDE.md). In short:

- **The generated substrate is sacrosanct** — mistranslations get fixed in the recompiler, never by
  hand-editing emitted shards.
- **Reverse-engineer first.** Every guest address in `game/` cites the instruction it came from and
  the command that reproduces the disassembly.
- **No bandaids.** No magic constants, no swallowed errors, no fake completions. If the RE is not
  done, the code says so and stops.
- **Verify before declaring done**, and record what was measured — including the dead ends — in
  `docs/`.

## Reference material

The PC release of this game has a community decompilation,
[**krystalgamer/spidey-decomp**](https://github.com/krystalgamer/spidey-decomp), which is a valuable
Rosetta stone: it is the same game by the same developer, so its recovered structure and naming can
guide the PSX reverse-engineering even though the target architecture differs. It has not been used
in this repo yet.

## Licensing

The framework's vendored beetle-psx backend is GPL-2.0; see its tree. No game assets, disc images,
executables, or BIOS files are included or distributed — supply your own.
