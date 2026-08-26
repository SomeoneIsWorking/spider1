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
                            +-- title-specific derived runtime
                                    |
                  +-----------------+------------------+
                  |                                    |
            Spider-Man 1                        Enter Electro
       title-owned host modules            measured refusal boundary
                  |
          game/render/render_seam.cpp
                  |
       frame policy + cohesive producers
                  |
          psxport framework queues/GPU
```

The shared `SpiderRuntime` owns address-free Neversoft-lineage mechanism. Executable addresses,
generated thunks, renderer capabilities, and boot policy stay in `titles/<title>/`. Host entry
points compose peer owners; they do not absorb rendering, input, storage, or diagnostics.

## Ownership

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
|---|---|---|---|---|
| Launcher | Frozen Python environment, dependency refusal, title selection, provisioning, build, launch | `run.sh`, `bootstrap.py`, `tools/run.py`, `tools/launcher_dependencies.py`, `tools/disc_path.py` | `tools/run.py::main` | `README.md` |
| Title catalog | Serial-keyed labels, executable identity, and title target metadata | `titles/*/title.json`, `tools/title_catalog.py`, `cmake/title_manifest.cmake` | `tools/title_catalog.py::load_catalog` | `CLAUDE.md` |
| Static recompilation | Executable/module extraction and reproducible generated substrate | `tools/ensure_recomp.py`, `tools/extract_modules.py`, per-title `recomp_seeds.json` | `tools/ensure_recomp.py::main` | `docs/re-frontier.md` |
| Product targets | One executable per title and no cross-title generated-symbol linkage | `CMakeLists.txt`, `cmake/spiderman_port.cmake`, `titles/spiderman2/port.cmake` | CMake title selection loop | `docs/plans/spider2-multi-title.md` |
| Lineage runtime | Address-free two-title identity and refusal mechanism | `game/core/spider_runtime.*` | `spider::SpiderRuntime` | `CLAUDE.md` |
| Spider-Man runtime | Measured compatibility seam, renderer capability declaration, overrides, and boot dispatch | `titles/spiderman1/` | `spider::Spider1Runtime` | `docs/re-frontier.md` |
| Enter Electro runtime | Direct runtime, executable facts, capability refusal, and EE boot boundary | `titles/spiderman2/enter_electro_runtime.*` | `spider::EnterElectroRuntime` | `docs/plans/spider2-multi-title.md` |
| Enter Electro enhanced renderer | Title-derived render seam, native producer, wide projection, and temporal history | target a `render/` subtree beneath `titles/spiderman2/`, plus address-free lineage peers in `game/render/` | target Enter Electro render installer | `docs/re-frontier.md` |
| Executable identity | Shipping serial, size, magic, and SHA-256 authentication | `game/core/executable_identity.*` | `verifyExecutableIdentity` | `CLAUDE.md` |
| Platform/HLE bridge | Spider-Man-owned sync, CD stream, card, IRQ, and compatibility facts | `game/core/` cohesive modules | `Spider1Runtime::registerOverrides` | `docs/re-frontier.md` |
| Runtime modules | Guest allocator placement and base-relative module routing | `game/core/module_loader.cpp`, `tools/extract_modules.py` | `spiderman_install_module_loader` | `docs/issues/0001-recomp-miss-0x800c6684-three-cd-wad-modules-live.md` |
| Core diagnostics | Observe-only title probes; never picture production | `game/core/diag_overrides.cpp` | `spiderman_install_diag_overrides` | `docs/info/instruments.md` |
| Render orchestration | Scene ownership decision, frame envelope composition, and reference super-call boundary | `game/render/render_seam.*` | `spiderman_install_render_seam` | `docs/re-frontier.md` |
| Scene identity | Binary-derived level/sublevel identity for render policy | `game/render/scene_id.*` | `classifyScene` | `docs/re-frontier.md` |
| Guest frame debt | Mutually exclusive, non-interpolated whole retail frame for scenes without a native producer | `game/render/guest_frame_fallback.*` | `decideGuestFrameFallback` | `docs/re-frontier.md` |
| Frame fence | Map retail submit-frame completion onto the framework temporal queue boundary | `game/render/guest_frame_commit.*` | `commitCapturedGuestFrame` | `docs/issues/0017-spider-man-aborts-after-entering-dem1-because-ca.md` |
| Frame envelope | Native DRAWENV/DISPENV and background-clear production | `game/render/frame_envelope.*`, `game/render/gpu_env.*` | `FrameEnvelope::submit` | `docs/issues/0013-a-native-producer-whose-only-scene-is-the-boot-i.md` |
| Asset ownership | Retained texture/CLUT bytes and upload lifetime | `game/render/asset_upload_ledger.*`, `game/render/mesh_asset_cook.*` | `AssetUploadLedger`, `cookMeshAsset` | `docs/issues/0016-first-dem1-mesh-has-no-renderer-time-raw-texture.md` |
| Mesh source format | Retail header, face stream, and caller-family contracts | `game/render/mesh_face_format.*`, `game/render/face_builder_census.*` | `deriveMeshLayout`, `FaceBuilderCensus::record` | `docs/re-frontier.md` |
| MIPS fixed-point decode | Signed packed values and retail arithmetic shift-by-four semantics | `game/render/mips_fixed_point.*` | `mipsSignedHalf`, `mipsSignedWord`, `mipsArithmeticShiftRight4` | `docs/re-frontier.md` |
| Direct mesh transform | Pre-GTE camera/object/relative transform decode | `game/render/mesh_transform.*` | `inspectMeshDirectTransform` | `docs/info/claims/040-spider-man-s-fun-80077d64-direct-mesh-path-trans.md` |
| Animated vertex staging | Projection/reuse/retain and near/far fixed-point input semantics | `game/render/mesh_animated_vertex.*` | `decodeAnimatedVertexRecord` | `docs/info/claims/054-spider-man-animated-vertex-flag-0x0002-reinterpr.md` |
| Animated pose contract | Pre-GTE base, secondary, and authored-pose decode plus temporal identity | `game/render/mesh_pose_contract.*` | `decodeMeshPoseInput` | `docs/info/claims/055-spider-man-s-animated-pose-composers-consume-thr.md` |
| Temporal pose history | Previous/current authored poses and interpolation sampling | target `game/render/mesh_pose_history.*` | target `MeshPoseHistory::record` / `sample` | `docs/re-frontier.md` |
| Native animated producer | PC matrix composition, projection/outcodes, common face rules, and queue emission | target `game/render/mesh_native_producer.*` | target `NativeMeshProducer::submit` | `docs/re-frontier.md` |
| Spider projection | Publish title-owned wide projection from the active viewport | target `game/render/spider_projection.*` | target `publishSpiderProjection` | `docs/re-frontier.md` |
| Render diagnostics | Census, asset, texture, mesh, and source/oracle probes; no shipping draw ownership | `game/render/frame_census.*`, `texture_asset_probe.*`, `mesh_probe.cpp` | `spiderman_install_*_probe` | `docs/info/instruments/` |
| Hermetic tests | Production-contract falsifiers and title runtime ownership tests | `tests/` | CTest registrations in `CMakeLists.txt` | `README.md` |
| Project registries | Epic intent, capability state, atomic issues, ownership, RE order, and evidence | `docs/project-goals.md`, `docs/project-state.md`, `docs/issues/`, `docs/codemap.md`, `docs/re-frontier.md`, `docs/info/` | `tools/info.py brief` | `CLAUDE.md` |
| Framework | Game-agnostic recompilation/runtime/render infrastructure | `external/psxport/` resolved checkout | framework `GameRuntime` seam | framework `CLAUDE.md` |
| Generated substrate | Regenerated compiler output; no hand-owned behavior | `generated/` | `tools/ensure_recomp.py` | `CLAUDE.md` |

## Source tree

```text
game/  —  8,083 lines, 56 files
├─ core/  3,335 lines, 22 files
└─ render/  4,748 lines, 34 files
titles/  —  285 lines, 6 files
├─ spiderman1/  146 lines, 2 files
└─ spiderman2/  139 lines, 4 files
tools/  —  9,233 lines, 35 files
tests/  —  497 lines, 12 files
```

Refresh with:

```sh
python "$CODEX_HOME/skills/codemap/codemap.py" tree game titles tools tests --depth 2 --min-lines 1
```

## Where does X go?

- A title serial, executable hash, or target label → `titles/<title>/title.json`.
- A title-specific guest address or generated thunk → `titles/<title>/`, never `SpiderRuntime`.
- Shared address-free Neversoft lineage behavior → `game/core/spider_runtime.*` or a cohesive peer.
- Host rendering orchestration → `game/render/render_seam.*`; draw implementation stays in a
  producer module.
- Animated pose decoding → `game/render/mesh_pose_contract.*`; temporal storage is a separate
  `game/render/mesh_pose_history.*` owner.
- Widescreen projection derived from Spider-Man game state → `game/render/spider_projection.*`.
- A diagnostic that answers a render question → a probe module, never the producer.
- A factual capability change → `docs/project-state.md`; a task/bug/finding → `docs/issues/`.
- A renderer or runtime capability declaration → the title-derived runtime, never the lineage base.
- Enter Electro render addresses and policy → a render subtree under `titles/spiderman2/`; only
  proven address-free mechanism may move into `game/render/`.
