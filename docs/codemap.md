# Codemap — spider1

This map answers only which subsystem owns a responsibility and where related work belongs.
Product intent is in `docs/project-goals.md`; capability coverage and current focus are in
`docs/project-state.md`; atomic work is in `docs/issues/`; RE dependency order is in
`docs/re-frontier.md`; evidence is in `docs/info/`.

## Architecture

```text
run.sh -> bootstrap.py -> tools/run.py
                            |
                            +-- title manifest / authenticated executable
                            +-- authenticated PS-X EXE
                                    |
                            psxport guest execution
                                    |
                            +-- title-specific runtime
                                    |
                  +-----------------+------------------+
                  |                                    |
            Spider-Man 1                        Enter Electro
        authenticated crt0 entry            authenticated crt0 entry
                  |                                    |
                  +---------- psxport JIT --------------+
```

The shared `SpiderRuntime` owns address-free Neversoft-lineage mechanism. Executable addresses,
runtime image identity, renderer capabilities, and boot policy stay in `titles/<title>/`. Host entry
points compose peer owners; they do not absorb rendering, input, storage, or diagnostics.

## Ownership

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
|---|---|---|---|---|
| Launcher | Frozen Python environment, pre-discovery help, dependency refusal, framework pin/configure provenance, title selection, provisioning, build, launch | `run.sh`, `bootstrap.py`, `tools/run.py`, `tools/psxport_sync.py`, `tools/launcher_dependencies.py`, `tools/disc_path.py` | `tools/run.py::main` | `README.md` |
| Product process CLI | Pre-identity help plus authenticated executable boot composition shared by both title products | `game/core/spider_port.*` | `spider::runPort` | `CLAUDE.md` |
| Title catalog | Serial-keyed labels, executable identity, and title target metadata | `titles/*/title.json`, `tools/title_catalog.py`, `cmake/title_manifest.cmake` | `tools/title_catalog.py::load_catalog` | `CLAUDE.md` |
| Runtime image provisioning | Extract and authenticate the selected executable without emitting guest bodies | `tools/provision.py`; title manifests beneath `titles/` remain fact authority | `provision_executable` | `docs/migration.md` |
| PSX guest executor | Per-`Core` Lightrec ownership, CPU/device synchronization, bounded exits, block cache, and invalidation | `external/psxport/runtime/cpu/`; no title-local executor | `psx::cpu::dispatchGuest` | `docs/migration.md` |
| Runtime dispatch | Image-aware native overrides, scoped original calls, and override-change invalidation | `external/psxport/runtime/cpu/native_dispatch.*`; title wrappers in `game/core/guest_execution.*` | `dispatchGuest`, `callOriginal` | `docs/migration.md` |
| Product targets | One executable per title, each consuming only its authenticated runtime image and title policy | `CMakeLists.txt` | `spider_add_runtime_target` | `docs/migration.md` |
| Lineage runtime | Address-free two-title identity and refusal mechanism | `game/core/spider_runtime.*` | `spider::SpiderRuntime` | `CLAUDE.md` |
| Spider-Man runtime | Authenticated image policy and JIT entry | `titles/spiderman1/` | `spider::Spider1Runtime` in `spider1_runtime.cpp` | `docs/re-frontier.md` |
| Enter Electro runtime | Direct runtime, executable facts, capability refusal, and EE boot boundary | `titles/spiderman2/enter_electro_runtime.*` | `spider::EnterElectroRuntime` | `docs/migration.md` |
| Enter Electro enhanced renderer | Title-derived render seam, native producer, wide projection, and temporal history | target beneath `titles/spiderman2/`, plus address-free lineage peers in `game/render/` | target Enter Electro render installer | `docs/re-frontier.md` |
| Executable identity | Shipping serial, size, magic, and SHA-256 authentication | `game/core/executable_identity.*` | `verifyExecutableIdentity` | `CLAUDE.md` |
| Frame cadence | Preserved finite Spider-Man 1 field/movie/mode owners awaiting attachment after executable JIT conformance | `titles/spiderman1/spider1_frame_driver.*`, `spider1_movie_execution.*`, `spider1_mode_driver.*` | typed guest-execution calls in those owners | `docs/migration.md` |
| Platform/HLE bridge | Framework PSX services plus preserved Spider-Man CD-stream owner | framework `PlatformHle`; `game/core/cd_stream.cpp` | image-aware native registration | `docs/re-frontier.md` |
| Runtime modules | Guest allocator placement, authenticated image activation, and cache invalidation | title loader observation plus `external/psxport/runtime/cpu/image_identity.*` and `invalidation.*` | image catalog activation | `docs/issues/0001-recomp-miss-0x800c6684-three-cd-wad-modules-live.md` |
| Scene identity | Binary-derived level/sublevel identity for render policy | `game/render/scene_id.*` | `classifyScene` | `docs/re-frontier.md` |
| Frame fence | Target mapping from finite retail mode steps onto submitted, repeated-field, or unpresented framework boundaries | preserved `titles/spiderman1/spider1_frame_driver.*`, `spider1_mode_driver.*` | attach only after JIT conformance | `docs/issues/0017-spider-man-aborts-after-entering-dem1-because-ca.md` |
| Frame envelope | Native DRAWENV/DISPENV and background-clear production | `game/render/frame_envelope.*`, `game/render/gpu_env.*` | `FrameEnvelope::submit` | `docs/issues/0013-a-native-producer-whose-only-scene-is-the-boot-i.md` |
| Asset ownership | Retained texture/CLUT bytes and upload lifetime | `game/render/asset_upload_ledger.*`, `game/render/mesh_asset_cook.*` | `AssetUploadLedger`, `cookMeshAsset` | `docs/issues/0016-first-dem1-mesh-has-no-renderer-time-raw-texture.md` |
| Mesh source format | Retail header, face stream, and caller-family contracts | `game/render/mesh_face_format.*`, `game/render/face_builder_census.*` | `deriveMeshLayout`, `FaceBuilderCensus::record` | `docs/re-frontier.md` |
| MIPS fixed-point decode | Signed packed values and retail arithmetic shift-by-four semantics | `game/render/mips_fixed_point.*` | `mipsSignedHalf`, `mipsSignedWord`, `mipsArithmeticShiftRight4` | `docs/re-frontier.md` |
| Direct mesh transform | Pre-GTE camera/object/relative transform decode | `game/render/mesh_transform.*` | `inspectMeshDirectTransform` | `docs/info/claims/040-spider-man-s-fun-80077d64-direct-mesh-path-trans.md` |
| Animated vertex staging | Projection/reuse/retain and near/far fixed-point input semantics | `game/render/mesh_animated_vertex.*` | `decodeAnimatedVertexRecord` | `docs/info/claims/054-spider-man-animated-vertex-flag-0x0002-reinterpr.md` |
| Animated pose contract | Pre-GTE base, secondary, and authored-pose decode plus temporal identity | `game/render/mesh_pose_contract.*` | `decodeMeshPoseInput` | `docs/info/claims/055-spider-man-s-animated-pose-composers-consume-thr.md` |
| Temporal pose history | Previous/current authored poses and interpolation sampling | target `game/render/mesh_pose_history.*` | target `MeshPoseHistory::record` / `sample` | `docs/re-frontier.md` |
| Native animated producer | PC matrix composition, projection/outcodes, common face rules, and queue emission | target `game/render/mesh_native_producer.*` | target `NativeMeshProducer::submit` | `docs/re-frontier.md` |
| Spider-Man projection | Pure 16:9 projection calculation preserving focal length; runtime publication is not attached | `titles/spiderman1/spider1_widescreen.*` | `Spider1Widescreen::presentationAspect` | `docs/re-frontier.md` |
| Enter Electro projection | Publish title-owned wide projection after its viewport boundary is measured | target beneath `titles/spiderman2/` | target Enter Electro projection owner | `docs/re-frontier.md` |
| Historical render evidence | Durable claims and instrument records only; retired runtime probes are absent from product source | `docs/info/` | `tools/info.py brief` | `docs/info/instruments/` |
| Hermetic tests | Production-contract falsifiers and title runtime ownership tests | `tests/` | CTest registrations in `CMakeLists.txt` | `README.md` |
| Consumer verification | Title-owned configuration for the shared Clang/Ninja build, CTest, and linked execution-boundary inspector | `tools/verify.py`; engine in `external/psxport/tools/port/consumer_verify.py` | `tools/verify.py::main` | `README.md` |
| Hosted verification | Real asset-free Linux x86-64 product and consumer-boundary verification; unsupported host gaps remain explicit in project state | `.github/workflows/ci.yml` | `linux-x86_64` job | `docs/project-state.md` |
| Project registries | Epic intent, capability state, atomic issues, ownership, RE order, and evidence | `docs/project-goals.md`, `docs/project-state.md`, `docs/issues/`, `docs/codemap.md`, `docs/re-frontier.md`, `docs/info/` | `tools/info.py brief` | `CLAUDE.md` |
| Framework | Game-agnostic Lightrec executor, PSX services, verification harness, and renderer | `external/psxport/` resolved checkout | framework runtime seam | framework `AGENTS.md` |

## Source tree

```text
game/  —  3,161 lines, 35 files
├─ core/  517 lines, 11 files
└─ render/  2,644 lines, 24 files
titles/  —  2,040 lines, 15 files
├─ spiderman1/  1,930 lines, 12 files
└─ spiderman2/  110 lines, 3 files
tools/  —  6,387 lines, 28 files
tests/  —  411 lines, 12 files
```

Refresh with:

```sh
uv run --frozen python ../../shared/re-harness/tools/codemap.py tree game titles tools tests --depth 2 --min-lines 1
```

## Where does X go?

- A title serial, executable hash, or target label → `titles/<title>/title.json`.
- A title-specific guest address, image identity, or override → `titles/<title>/`, never
  `SpiderRuntime`.
- PSX instruction semantics, cache ownership, bounded exits, or executable-memory invalidation →
  `external/psxport/`, never a title workaround.
- A title's finite boot, mode state, field cadence, or frame phase order → its own
  `titles/<title>/` FrameDriver; never infer another title's addresses from lineage.
- Shared address-free Neversoft lineage behavior → `game/core/spider_runtime.*` or a cohesive peer.
- Host rendering orchestration → a new cohesive owner under `game/render/` after JIT conformance;
  draw implementation stays in producer modules.
- Animated pose decoding → `game/render/mesh_pose_contract.*`; temporal storage is a separate
  `game/render/mesh_pose_history.*` owner.
- Widescreen projection derived from Spider-Man game state → `game/render/spider_projection.*`.
- A diagnostic that answers a render question → a probe module, never the producer.
- A factual capability change → `docs/project-state.md`; a task/bug/finding → `docs/issues/`.
- A renderer or runtime capability declaration → the title-derived runtime, never the lineage base.
- Enter Electro render addresses and policy → a render subtree under `titles/spiderman2/`; only
  proven address-free mechanism may move into `game/render/`.
