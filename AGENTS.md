# Spider-Man port agent instructions

This repository targets both Neversoft PSX Spider-Man games as native PC products whose remaining
guest instructions execute through psxport's runtime Lightrec integration. Read `../AGENTS.md`,
`external/psxport/AGENTS.md`, and `../../shared/jit-common/docs/migration.md` before changing execution
architecture. Project intent, current coverage, placement, migration order, and binary-evidence order
live respectively in `docs/project-goals.md`, `docs/project-state.md`, `docs/codemap.md`,
`docs/migration.md`, and `docs/re-frontier.md`.

## Product execution contract

- The gameplay product is one native-plus-dynarec runtime. Native owners replace selected title
  behavior; Lightrec dynamically translates every remaining instruction from the authenticated user
  executable or runtime-loaded module.
- An interpreter may exist only in a separately built test/diagnostic target. The gameplay binary
  must not link it, expose an engine selector for it, or fall back to it.
- Offline guest translation is retired. Do not regenerate, build, run, extend, or diagnose the old
  generated-C product. Static analysis may emit symbols or other non-executable metadata, never guest
  function bodies.
- Generated functions are temporary migration evidence, not a product oracle or compatibility mode.
  Delete the generator, generated corpus, generated-symbol dispatch, seeds used only by emission, and
  generated-body tests once the representative-gameplay retirement gate in `docs/migration.md`
  passes.
- Keep native overrides image-aware. A normal call honors the override table; a scoped original call
  bypasses only the current override and executes the guest body through Lightrec. Executable-memory
  writes and module load/unload invalidate affected translated blocks.

## Spider-Man first discriminator

The first Spider-Man migration checkpoint is the already-recorded `dem1` route. Reach it through
Lightrec with nonzero translated blocks and the existing native owners active, and replace the
build-derived `FUN_8002AA0C` movie-fiber body with resumable runtime guest execution. The title field
owner must suspend and resume at the three authenticated STR VSync return PCs without regenerating or
rewriting the guest body. This checkpoint proves wiring only; it does not authorize removal of the
static path. Representative interactive gameplay, native and original-call dispatch, invalidation,
oracle comparison, and no-interpreter link/selector evidence are all required first.

Enter Electro remains behind Spider-Man under the single-title completion rule. Preserve its measured
`SLUS_013.78` identity, crt0 facts, and first game-owned call `0x80031F54`; do not infer any Spider-Man
address or behavior from lineage similarity.

## Working discipline

- Begin non-trivial work with `python3 tools/info.py brief <terms>`, then consult
  `python3 tools/re_frontier.py next` and the relevant issue. Update the one authority whose answer
  changes.
- Preserve verified binary addresses, behavior, native subsystem contracts, and real scenarios.
  Do not preserve static-recompiler methodology merely because it produced the earlier evidence.
- Native render producers consume pre-GTE game state. Guest GTE/OT/packet output can remain an
  explicit, non-interpolated whole-frame debt path, never native-producer input.
- Never guess a guest address or module identity. Complete identity is executable/module generation
  plus address wherever runtime-loaded images reuse addresses.
- Diagnostics must report their denominator and prove both answers. A clean trace or a boot screen is
  not gameplay conformance.
- Do not use `./run.sh` for agent verification. During this migration do not use it at all until its
  zero-argument path launches the native/Lightrec product without offline translation.

`CLAUDE.md` is a discovery pointer to this file; this file is the sole project-local instruction
authority.
