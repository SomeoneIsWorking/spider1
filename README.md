# Spider-Man PSX — both Neversoft games

A work-in-progress multi-title native PC port for PlayStation *Spider-Man* and *Spider-Man 2: Enter
Electro*, built on the
[**psxport**](https://github.com/SomeoneIsWorking/psxport) framework.

The serial/boot executable is the canonical title key:

| title | key | implementation status |
|---|---|---|
| Spider-Man | `SLUS_008.75` | current implementation |
| Spider-Man 2: Enter Electro | `SLUS_013.78` | provisions and reaches an explicit first-call refusal |

Names are descriptive labels, not selectors. The launcher reads the selected disc's `SYSTEM.CNF`,
chooses a target by its boot serial, and authenticates the freshly extracted executable against that
title's measured size and SHA-256. The native executable repeats the serial, size, PS-X EXE, and
SHA-256 checks before constructing `Game`, so a renamed or stale cache cannot select runtime facts.
Each target has a derived runtime and a separate generated namespace; shared `game/` code contains
the common boot/runtime mechanism plus still-unmigrated Spider-Man 1 compatibility modules. That
migration debt is explicit; Enter Electro does not link those modules.

psxport **statically recompiles** the game's MIPS R3000A machine code into native C, then runs it
under a native platform layer — so the result behaves like a PC program, not an emulator. On top of
that substrate, the game's own engine is reimplemented natively function by function, each step
verified against the recompiled reference.

**No disc image, executable, or game asset is included or distributed here.** Supply your own
legally-obtained copy.

---

## Status — honest

`SLUS_008.75` provisions, recompiles, builds, boots, and reaches gameplay. Its retail guest-GTE frame
path is runnable, but native display-list production is still incomplete (RE-21), so this is not a
complete native renderer or finished port. The explicitly recorded whole-guest-frame fallback is
visible debt; it does not count as a native producer.

`SLUS_013.78` has a serial-validated provisioning path, resident executable substrate, build target,
and direct derived runtime. Its crt0 boot group is measured 8/8 from the USA executable and audited
by the framework at boot. Execution then aborts deliberately at `gameMain 0x80031F54` because the
first game-owned call is not ported. It has no gameplay, rendering, widescreen, native producer, or
runtime-module claim yet.

See [`docs/project-state.md`](docs/project-state.md) for verified/partial/missing capability
coverage, [`docs/project-goals.md`](docs/project-goals.md) for durable product outcomes,
[`docs/codemap.md`](docs/codemap.md) for subsystem placement, and
[`docs/re-frontier.md`](docs/re-frontier.md) for the ordered RE dependency chain.

---

## Requirements

- [`uv`](https://docs.astral.sh/uv/) and a C++20 compiler (GCC, Clang, or AppleClang)
- **Linux:** `cmake`, `pkg-config`, `SDL3`, `SDL3_image`, FreeType, `libzstd`, `zlib`, OpenSSL,
  and `glslc` development/tool packages
- **macOS:** `brew install cmake pkg-config sdl3 sdl3_image freetype zstd zlib openssl shaderc`
- A Vulkan-capable GPU + drivers
- Your own supported USA disc image as a `.chd` (`SLUS_008.75` or `SLUS_013.78`)

If a native dependency is absent, the launcher stops before provisioning and prints the exact
`sudo dnf install`, `sudo apt install`, or Homebrew command for the detected host. It never installs
privileged system packages itself.

## Running

```sh
git clone <this repo>
cd spider1
./run.sh /path/to/"Spider-Man (USA).chd"
./run.sh /path/to/"Spider-Man 2 - Enter - Electro (USA).chd"
```

`run.sh` is the stable launcher. It enters the frozen uv environment and delegates policy to
`bootstrap.py` / `tools/run.py`. With no special mode it builds the CHD tooling, reads `SYSTEM.CNF`,
extracts the serial-bound boot executable,
authenticates its measured SHA-256, statically recompiles it, builds that title's target, and
launches it. Instead of passing the path
you can set `PSXPORT_SPIDERMAN_DISC` (first game), `PSXPORT_SPIDERMAN2_DISC` (Enter Electro),
`PSXPORT_DISC`, or drop a `.chd` in the repo root.

Player builds use compiler-and-policy-keyed directories below `scratch/build/player/`, set
`BUILD_TESTING=OFF` (and `PSXPORT_BUILD_TESTS=OFF` for the standalone framework tool build), and
build only the selected product target. They never reuse the maintainer
`build/` tree or run CTest. `./run.sh --prepare-only [disc.chd]` performs the same provisioning and
product build but stops before process launch.

Useful knobs for direct diagnostic runs: `PSXPORT_NOAUDIO=1`, `PSXPORT_WATCHDOG=<sec>`,
`PSXPORT_DEBUG=vsync` (channel-gated diagnostics).

## Verifying C++ changes

After building, run:

```sh
ctest --test-dir build --output-on-failure -R 'cpp_policy|launcher_policy'
```

Maintainer verification uses Clang: the C++ check covers all first-party C/C++ with the tracked Clang
format profile, runs clang-tidy on every compile-backed first-party C++ translation unit using the
real Clang commands, and enforces the 1,200-line ownership cap. That is separate from the player
launcher, which does not whitelist or blacklist compiler identities. The launcher check covers the
frozen uv shim, disc-path precedence, refusal discrimination, interpreter/compiler propagation, the
no-tests product path, exact dependency commands, the required title targets, and window/headless
launch environments. The shared C++ checker lives in psxport; this repo owns only its policy files
and CTest wiring. There is no pre-commit hook.

## Layout

```
game/core/           shared boot/runtime seam + bounded Spider-Man 1 compatibility modules
titles/              manifests, derived title runtimes, and generated-substrate seams
external/psxport/    resolved framework checkout: recompiler, runtime, PSX hardware, harness, renderer
generated/           per-title recompiled substrates — regenerated, never committed, never hand-edited
tools/               provisioning + reverse-engineering helpers
docs/                goals, project state, codemap, RE frontier, issues, claims + instruments
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
