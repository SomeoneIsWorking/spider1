# Spider-Man PSX — both Neversoft games

A work-in-progress native PC port of *Spider-Man* and *Spider-Man 2: Enter Electro*. The target
architecture combines title-owned native subsystems with psxport's runtime Lightrec executor for all
remaining MIPS instructions read from the player's authenticated game files.

No disc image, executable, or game asset is included. Supply a legally obtained USA disc image.

## Titles

| Title | Executable identity | Current evidence |
| --- | --- | --- |
| Spider-Man | `SLUS_008.75` | Historical evidence reached both intro movies and early `dem1`; finite native frame/service work is preserved but not attached to the product. The asset-free product now builds against Lightrec, but authenticated gameplay has not been run through it. |
| Spider-Man 2: Enter Electro | `SLUS_013.78` | Identity, eight crt0 facts, and first game-owned call `0x80031F54` are measured. Gameplay and rendering are absent. |

The serial, PS-X EXE header, size, and SHA-256 jointly select a title. Names are labels, never
selectors, and no title may borrow another title's addresses or behavior.

## Migration status

The former product emitted guest code as C at provisioning time and compiled a per-title generated
corpus. That architecture is removed. The generator, corpora, emission-only seeds, static
dispatcher/tests, and old build/provisioning path are absent. The only product target accepts the
authenticated executable and enters psxport's runtime guest-execution boundary.

The first implementation discriminator is deliberately narrow: execute the authenticated Spider-Man
image through Lightrec, preserve the current native owners, reach `dem1`, and replace the generated
movie-fiber derivative with resumable runtime guest execution at the three binary-proven STR field
boundaries. This demonstrates that real title code is running dynamically.

A bounded representative interactive gameplay milestone must also prove:

- nonzero Lightrec blocks, no interpreter gameplay selector, and bounded reason-coded fallback below
  its declared threshold;
- a native override and an override-bypassing original guest call through the shipping dispatcher;
- positive and controlled-negative invalidation for executable module changes;
- timing, memory, interrupts, and relevant device state against an independent oracle; and
- the declared correctness and frame-time budget on every released host architecture.

The removed pipeline cannot return as a compatibility mode while this work is incomplete.

The consumer is pinned to PSXPort
`eb5f23a8b3506f8853b3cfadcedc024cd90818a0`, whose runtime dependency requires maintained Lightrec
`b1457137c31cedff5f440d59da29401d021ba2da`. Both revisions are immutable build inputs; CMake refuses
a dirty or mismatched Lightrec checkout and the consumer pin test refuses a different PSXPort build.

## Intended player experience

`./run.sh` enters the frozen `uv` environment, selects and authenticates the disc, provisions only
the PS-X EXE, builds the native/Lightrec product without offline guest translation, and launches the
selected title. The maintained Lightrec backend is linked; title gameplay remains unverified until a
real authenticated run reaches the declared discriminator with measured translation and fallback
counters.

Hosted CI is configured to build and test the real asset-free Linux x86-64 boundary; the new
workflow has not run from this working tree yet. Windows, macOS, Apple Silicon, and Android jobs
remain absent because their PSXPort/Lightrec product backends are not implemented or qualified;
policy-only jobs do not stand in for those missing platform boundaries.
The local and hosted verifier is `uv run --frozen python tools/verify.py`; it delegates the shared
Clang/Ninja configure, build, CTest, and linked-product inspection sequence to the pinned PSXPort.

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
