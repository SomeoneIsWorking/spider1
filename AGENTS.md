# Spider-Man port agent instructions

This repository targets both Neversoft PSX Spider-Man games as native PC products whose remaining
guest instructions execute through psxport's runtime Lightrec integration. Read `../AGENTS.md` and
`external/psxport/AGENTS.md`, and use the shared dynarec skills before changing execution
architecture. Project intent, current coverage, placement, migration order, and binary-evidence order
live respectively in `docs/project-goals.md`, `docs/project-state.md`, `docs/codemap.md`,
`docs/migration.md`, and `docs/re-frontier.md`.

## Product execution contract

- The gameplay product is one native-plus-dynarec runtime. Native owners replace selected title
  behavior; Lightrec dynamically translates every remaining instruction from the authenticated user
  executable or runtime-loaded module.
- There is no interpreter gameplay mode or engine selector. The maintained backend may use only its
  classified, bounded automatic block fallback after a JIT refusal, with reason and instruction
  counters; a separately selected interpreter remains test/diagnostic-only.
- Offline guest translation is retired. Do not regenerate, build, run, extend, or diagnose the old
  generated-C product. Static analysis may emit symbols or other non-executable metadata, never guest
  function bodies.
- The generator, emitted corpus, generated-symbol dispatch, emission-only seeds, and generated-body
  tests are already removed break-first. Do not restore any of them as a compatibility or fallback
  path while the runtime executor is incomplete.
- Keep native overrides image-aware. A normal call honors the override table; a scoped original call
  bypasses only the current override and executes the guest body through Lightrec. Executable-memory
  writes and module load/unload invalidate affected translated blocks.

## Spider-Man first discriminator

The first Spider-Man migration checkpoint is the already-recorded `dem1` route. Reach it through
Lightrec with nonzero translated blocks and the existing native owners active. The build-derived
`FUN_8002AA0C` movie-fiber body is replaced by resumable runtime guest execution. The title field
owner must suspend and resume at the three authenticated STR VSync return PCs without regenerating or
rewriting the guest body. This checkpoint proves wiring only. Representative interactive gameplay,
native and original-call dispatch, invalidation,
oracle comparison, and bounded-fallback/no-interpreter-selector evidence are all required first.

Enter Electro remains behind Spider-Man under the single-title completion rule. Preserve its measured
`SLUS_013.78` identity, crt0 facts, and first game-owned call `0x80031F54`; do not infer any Spider-Man
address or behavior from lineage similarity.

## Working discipline

- Begin non-trivial work with `uv run --frozen python tools/info.py brief <terms>`, then consult
  `uv run --frozen python tools/re_frontier.py next` and the relevant issue. Update the one authority
  whose answer changes.
- Preserve verified binary addresses, behavior, native subsystem contracts, and real scenarios.
  Do not preserve static-recompiler methodology merely because it produced the earlier evidence.
- Native render producers consume pre-GTE game state. Guest GTE/OT/packet output can remain an
  explicit, non-interpolated whole-frame debt path, never native-producer input.
- Never guess a guest address or module identity. Complete identity is executable/module generation
  plus address wherever runtime-loaded images reuse addresses.
- Diagnostics must report their denominator and prove both answers. A clean trace or a boot screen is
  not gameplay conformance.
- Do not use `./run.sh` for agent verification; use focused build/test commands. The launcher is a
  player surface and now builds only the native/Lightrec product.

`CLAUDE.md` is a discovery pointer to this file; this file is the sole project-local instruction
authority.
