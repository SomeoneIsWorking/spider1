# Codemap — spider1

This map answers only which subsystem owns a responsibility and where related work belongs.
Product intent is in `docs/project-goals.md`; capability coverage and current focus are in
`docs/project-state.md`; atomic work is in `docs/issues/`.

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
| Launcher | Frozen Python environment, pre-discovery help, dependency refusal, framework resolution, title selection, provisioning, build, launch | `run.sh`, `bootstrap.py`, `tools/run.py`, `tools/psxport_fetch.py`, `tools/launcher_dependencies.py`, `tools/disc_path.py` | `tools/run.py::main` | `README.md` |
| Product process CLI | Pre-identity help, authenticated executable boot composition, and bounded typed-exit resumption shared by both title products | `game/core/spider_port.*`, `game/core/guest_execution.*` | `spider::runPort` | `CLAUDE.md` |
| Title catalog | Serial-keyed labels, executable identity, and title target metadata | `titles/*/title.json`, `tools/title_catalog.py`, `cmake/title_manifest.cmake` | `tools/title_catalog.py::load_catalog` | `CLAUDE.md` |
| Runtime image provisioning | Extract and authenticate the selected executable without emitting guest bodies | `tools/provision.py`; title manifests beneath `titles/` remain fact authority | `provision_executable` | `docs/migration.md` |
| PSX guest executor | Per-`Core` Lightrec ownership, CPU/device synchronization, bounded exits, block cache, and invalidation | `external/psxport/runtime/cpu/`; no title-local executor | `psx::cpu::dispatchGuest` | `docs/migration.md` |
| Runtime dispatch | Image-aware native overrides, scoped original calls, and override-change invalidation | `external/psxport/runtime/cpu/native_dispatch.*`; title wrappers in `game/core/guest_execution.*` | `dispatchGuest`, `callOriginal` | `docs/migration.md` |
| Product targets | One executable per title, each consuming only its authenticated runtime image and title policy | `CMakeLists.txt` | `spider_add_runtime_target` | `docs/migration.md` |
| Lineage runtime | Address-free two-title identity and refusal mechanism | `game/core/spider_runtime.*` | `spider::SpiderRuntime` | `CLAUDE.md` |
| Spider-Man runtime | Authenticated image policy and JIT entry | `titles/spiderman1/` | `spider::Spider1Runtime` in `spider1_runtime.cpp` | `docs/migration.md` |
| Enter Electro runtime | Direct runtime, executable facts, capability refusal, and EE boot boundary | `titles/spiderman2/enter_electro_runtime.*` | `spider::EnterElectroRuntime` | `docs/migration.md` |
| Enter Electro enhanced renderer | Title-derived render seam, native producer, wide projection, and temporal history | target beneath `titles/spiderman2/`, plus address-free lineage peers in `game/render/` | target Enter Electro render installer | `docs/migration.md` |
| Executable identity | Shipping serial, size, magic, and SHA-256 authentication | `game/core/executable_identity.*` | `verifyExecutableIdentity` | `CLAUDE.md` |
| Frame cadence | Spider-Man 1 pre-main and exact retail-movie VSync field continuations, the display field clock `VSync(0)` is defined in terms of, and the finite host-stepped fiber the boot prefix and the mode steps run on | `titles/spiderman1/spider1_frame_driver.*`, `spider1_field_clock.*`, `spider1_host_stepped_fiber.*`, `spider1_movie_execution.*` | `Spider1FrameDriver::serviceBootstrapVsync`, `serviceBootstrapMovieVsync`, `Spider1FieldClock::vsyncReturnValue`, `Spider1HostSteppedFiber` | `docs/migration.md` |
| Guest field facts | Every named SLUS_008.75 guest field, mode id, table offset, service entry, and threshold the title's drivers touch, declared ONCE at the name — including the ones whose role is genuinely unknown, named `kUnattributed…` so the open question is visible instead of hidden | `titles/spiderman1/spider1_guest_layout.h`; the measured platform and CD facts stay in `spider1_platform_facts.h` | `spider1::padRead`, `spider1::asyncModeState`, `spider1::kUnattributedWord4F38` | `titles/spiderman1/spider1_guest_layout.h` |
| Retail mode state machine | Spider-Man 1's outer selector, its primary mode, its 3D transition, and its level route, with one named method per route arm rather than one long switch | `titles/spiderman1/spider1_mode_driver.*` | `spider1OuterRoute`, `Spider1ModeDriver::dispatchPrimaryExit` | `docs/migration.md` |
| Retail mode functions | The mode functions the state machine COMPOSES rather than absorbs: the title menu, the alternate mode, the invalid-selector input wait, the 3D wipe, and the display-field boundary the three modes share | `titles/spiderman1/spider1_menu_mode.*`, `spider1_alternate_mode.*`, `spider1_invalid_selector_input.*`, `spider1_transition_wipe.*`, `spider1_mode_frame_boundary.*`, `spider1_mode_host.h`, `spider1_mode_decisions.h`, `spider1_guest_call.h`, `spider1_guest_stack_frame.h` | `Spider1MenuMode`, `Spider1AlternateMode`, `Spider1InvalidSelectorInput`, `Spider1TransitionWipe` | `docs/migration.md` |
| Platform/HLE bridge | Measured SCEI service entries and pad receive buffers; framework handlers and Spider-Man's per-Core StGetNext stream and ring-diagnostic owner | `titles/spiderman1/spider1_platform_facts.h`, `spider1_stream_driver.*`; framework `PlatformHle` | `Spider1Runtime::platformHlePlan`, `guestPadBufferLayout`, `guestCdStreamCallbackLayout`, `createContext`, `registerOverrides` | `docs/migration.md` |
| Runtime modules | Guest allocator placement, authenticated image activation, and cache invalidation | title loader observation plus `external/psxport/runtime/cpu/image_identity.*` and `invalidation.*` | image catalog activation | `docs/migration.md` |
| Scene identity | Binary-derived level/sublevel identity for render policy | `game/render/scene_id.*` | `classifyScene` | `docs/migration.md` |
| Frame fence | Target mapping from finite retail mode steps onto submitted, repeated-field, or unpresented framework boundaries | preserved `titles/spiderman1/spider1_frame_driver.*`, `spider1_mode_driver.*`; the pure decisions stay in `spider1_field_schedule.h` and `spider1_mode_decisions.h`, and the frame driver's four presentation commits share one `claimFrameFence` check | attach only after JIT conformance | `docs/migration.md` |
| Frame envelope | Native DRAWENV/DISPENV and background-clear production | `game/render/frame_envelope.*`, `game/render/gpu_env.*` | `FrameEnvelope::submit` | `docs/issues/0013-a-native-producer-whose-only-scene-is-the-boot-i.md` |
| Asset ownership | Retained texture/CLUT bytes and upload lifetime | `game/render/asset_upload_ledger.*`, `game/render/mesh_asset_cook.*` | `AssetUploadLedger`, `cookMeshAsset` | `docs/migration.md` |
| Mesh source format | Retail header, face stream, and caller-family contracts | `game/render/mesh_face_format.*`, `game/render/face_builder_census.*` | `deriveMeshLayout`, `FaceBuilderCensus::record` | `docs/migration.md` |
| MIPS fixed-point decode | Signed packed values and retail arithmetic shift-by-four semantics | `game/render/mips_fixed_point.*` | `mipsSignedHalf`, `mipsSignedWord`, `mipsArithmeticShiftRight4` | `docs/migration.md` |
| Direct mesh transform | Pre-GTE camera/object/relative transform decode | `game/render/mesh_transform.*` | `inspectMeshDirectTransform` | `docs/migration.md` |
| Animated vertex staging | Projection/reuse/retain and near/far fixed-point input semantics | `game/render/mesh_animated_vertex.*` | `decodeAnimatedVertexRecord` | `docs/migration.md` |
| Animated pose contract | Pre-GTE base, secondary, and authored-pose decode plus temporal identity | `game/render/mesh_pose_contract.*` | `decodeMeshPoseInput` | `docs/migration.md` |
| Temporal pose history | Previous/current authored poses and interpolation sampling | target `game/render/mesh_pose_history.*` | target `MeshPoseHistory::record` / `sample` | `docs/migration.md` |
| Native animated producer | PC matrix composition, projection/outcodes, common face rules, and queue emission | target `game/render/mesh_native_producer.*` | target `NativeMeshProducer::submit` | `docs/migration.md` |
| Spider-Man projection | Pure 16:9 projection calculation preserving focal length; runtime publication is not attached | `titles/spiderman1/spider1_widescreen.*` | `Spider1Widescreen::presentationAspect` | `docs/migration.md` |
| Enter Electro projection | Publish title-owned wide projection after its viewport boundary is measured | target beneath `titles/spiderman2/` | target Enter Electro projection owner | `docs/migration.md` |
| Bounded product runs | One bounded headless run with live CD-channel sampling over frames, single product slot, captured-PID lifetime | `tools/probe_spider1_headless_run.py` | `main` | `docs/issues/0026` |
| Widescreen picture pair | A matched 4:3 / 16:9 capture from the real product, one process per aspect, ruled on by the framework's own discriminator | `tools/probe_spider1_widescreen_pair.py` | `main` | `docs/issues/0025` |
| Pad replay | A deterministic pad-mask file so a present index means the same content twice | `tools/make_pad_replay.py` | `main` | `docs/issues/0009` |
| Decomp pipeline | Ghidra headless query/export/import over the image's own RAM dump | `tools/ghidra_query.py`, `tools/ghidra_export.py`, `tools/ghidra_import.py`, `tools/redump_ram.py` | `main` | `docs/codemap.md` |
| Guest CD-ROM service | The polled service `0x8008C3E0` recovered as named structs, constants and a five-arm dispatch, owned natively at the one site retail executes. It is a POLL, not an interrupt handler, and the framework's callback delivery is not a substitute for it | `titles/spiderman1/spider1_cd_stream.*` | `CdStreamService::service`, `runArm`, `report` | `docs/issues/0026` |
| Hermetic tests | Production-contract falsifiers and title runtime ownership tests | `tests/` | CTest registrations in `CMakeLists.txt` | `README.md` |
| Consumer verification | Title-owned configuration for the shared Clang/Ninja build, CTest, and linked execution-boundary inspector | `tools/verify.py`; engine in `external/psxport/tools/port/consumer_verify.py` | `tools/verify.py::main` | `README.md` |
| Hosted verification | Real asset-free Linux x86-64 product and consumer-boundary verification; unsupported host gaps remain explicit in project state | `.github/workflows/ci.yml` | `linux-x86_64` job | `docs/project-state.md` |
| Project documents | Epic intent, capability state, atomic issues, ownership, and the execution plan | `docs/project-goals.md`, `docs/project-state.md`, `docs/issues/`, `docs/codemap.md`, `docs/migration.md` | — | `CLAUDE.md` |
| Framework | Game-agnostic Lightrec executor, PSX services, verification harness, and renderer | `external/psxport/` resolved checkout | framework runtime seam | framework `AGENTS.md` |

## Source tree

```text
game/  —  3,138 lines, 34 files
├─ core/  494 lines, 10 files
└─ render/  2,644 lines, 24 files
titles/  —  5,864 lines, 46 files
├─ spiderman1/  5,723 lines, 41 files
└─ spiderman2/  141 lines, 5 files
tools/  —  3,611 lines, 17 files
tests/  —  2,788 lines, 17 files
```

Refresh with:

```sh
uv run --frozen python ../../shared/re-harness/tools/codemap.py tree game titles tools tests --depth 2 --min-lines 1
```

## Where does X go?

- A title serial, executable hash, or target label → `titles/<title>/title.json`.
- A guest address, table offset, mode id, or threshold the title's drivers read → the ONE owner,
  `titles/<title>/spider1_guest_layout.h`, declared at a name. If the role is not known, the name says
  so (`kUnattributed…`); do not invent a confident name for a word nobody has read out of the image.
- A rule a mode DECIDES by (a comparison, a threshold, a which-of-two answer) → a `constexpr` function
  in the mode's own decision header, so its whole input space is testable without a `Core`.
- A retail mode function with its own object lifetime → its own owner under `titles/<title>/`, and the
  state machine COMPOSES it. A mode driver that grows a mode's state inline is the smell.
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
