# Spider-Man PSX — both Neversoft games

A work-in-progress native PC port of *Spider-Man* and *Spider-Man 2: Enter Electro*. The target
architecture combines title-owned native subsystems with psxport's runtime Lightrec executor for all
remaining MIPS instructions read from the player's authenticated game files.

No disc image, executable, or game asset is included. Supply a legally obtained USA disc image.

## Titles

| Title | Executable identity | Current evidence |
| --- | --- | --- |
| Spider-Man | `SLUS_008.75` | The retired generated-code path reached both intro movies and early `dem1`; native frame, service, render-seam, and widescreen owners exist. Runtime Lightrec gameplay is not implemented yet. |
| Spider-Man 2: Enter Electro | `SLUS_013.78` | Identity, eight crt0 facts, and first game-owned call `0x80031F54` are measured. Gameplay and rendering are absent. |

The serial, PS-X EXE header, size, and SHA-256 jointly select a title. Names are labels, never
selectors, and no title may borrow another title's addresses or behavior.

## Migration status

The former product emitted guest code as C at provisioning time and compiled a per-title generated
corpus. That architecture is retired. Do not regenerate, build, or run it for new evidence. The
current work is documentation and planning for the replacement described in
[`docs/migration.md`](docs/migration.md).

The first implementation discriminator is deliberately narrow: execute the authenticated Spider-Man
image through Lightrec, preserve the current native owners, reach `dem1`, and replace the generated
movie-fiber derivative with resumable runtime guest execution at the three binary-proven STR field
boundaries. This demonstrates that real title code is running dynamically; it is not permission to
delete the old corpus.

Deletion requires one bounded representative interactive gameplay milestone that also proves:

- nonzero Lightrec blocks and no interpreter in the gameplay link or selector surface;
- a native override and an override-bypassing original guest call through the shipping dispatcher;
- positive and controlled-negative invalidation for executable module changes;
- timing, memory, interrupts, and relevant device state against an independent oracle; and
- the declared correctness and frame-time budget on every released host architecture.

Only after that gate passes are the generator, generated corpus, emission-only seeds, static
dispatcher, and generated-symbol tests removed together. No compatibility mode remains.

## Intended player experience

The finished `./run.sh` path will enter the frozen `uv` environment, select and authenticate the
disc, build the native/Lightrec product without offline guest translation, and launch the selected
title. The existing launcher still drives the retired generated pipeline, so it is not a valid
product or verification path during this migration.

Player builds will accept GCC, Clang, and AppleClang. Maintainer C++ verification uses Clang together
with the tracked `clang-format` and `clang-tidy` policy. Missing native dependencies must be refused
with an exact platform package command rather than installed automatically. Ghidra and other RE tools
are never player prerequisites.

## Architecture

```text
authenticated executable/module bytes
              |
              v
    psxport per-Core Lightrec executor
      | runtime cache | invalidation |
      +---------------+--------------+
                      |
       image-aware title dispatch
          |                    |
    native override      original guest body
                          through Lightrec
```

`game/` holds address-free lineage mechanism and cohesive Spider-Man native owners.
`titles/<title>/` owns title identity, addresses, execution policy, and title-local behavior.
`external/psxport/` is the shared framework checkout. Static analysis metadata may remain reviewable;
generated executable guest bodies are not part of the target architecture.

See [`docs/project-goals.md`](docs/project-goals.md) for durable outcomes,
[`docs/project-state.md`](docs/project-state.md) for factual coverage,
[`docs/codemap.md`](docs/codemap.md) for ownership, and
[`docs/re-frontier.md`](docs/re-frontier.md) for the ordered binary-evidence chain.

## Legal

The framework's vendored beetle-psx backend is GPL-2.0. Spider-Man names and assets belong to their
respective rights holders. This project is not affiliated with or endorsed by them.
