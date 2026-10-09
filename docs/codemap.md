# Codemap — spider1

This map answers which subsystem owns a responsibility, where it lives, and which class answers
"where does a key press go?". Product intent is in `docs/project-goals.md`; capability coverage and
current focus are in `docs/project-state.md`; atomic work is in `docs/issues/`; ordered RE evidence is
in `docs/re-frontier.md`.

The framework side of every chain below is `external/psxport/docs/codemap.md`.

## Namespaces

One namespace per directory, and the three never overlap:

| namespace | directory | what may live there |
|---|---|---|
| `spider` | `game/core/` | the address-free Neversoft lineage shared by both titles, and the process boot |
| `spider::render` | `game/render/` | title-neutral render contracts and producers (no title address, no game state) |
| `spider::spider1` | `titles/spiderman1/` | every `SLUS_008.75` fact, rule, mode and boundary |
| `spider::enterelectro` | `titles/spiderman2/` | every `SLUS_013.78` fact |

Nothing in `spider` or `spider::render` may name an address from either title.

## Architecture

```text
run.sh -> bootstrap.py -> tools/run.py
                            |
                            +-- title catalog -> authenticated PS-X EXE (no guest body emitted)
                                    |
game/core/main.cpp -> psx::host::ProductHost over Spider1Catalog (external/psxport/docs/title-host.md)
                                    |
              psx::host::TitleSession: generic boot, then one FrameLoopShell step per host frame
                                    |
                            Spider1Runtime::bootInit / createFrameDriver -> Spider1FrameDriver
                                    |
                            Spider1BootstrapTurn: the guest runs to a measured boundary,
                            Spider1Runtime::resumeBootstrapBoundary services it as one field
```

Spider-Man 1 is hosted: zero arguments open the in-window picker, one executable argument runs that
serial-identified executable (`tools/run.py` passes none for a title whose manifest says `hosted`).
`spider::runPort` is Enter Electro's process spine only (`titles/spiderman2/main.cpp`), until that
title joins the catalog (EE-02). `Spider1Runtime` holds the authenticated image, the platform-HLE
plan, the widescreen owner and the CD-stream service; `Spider1FrameDriver` owns cadence and field
delivery. `Spider1ModeDriver` and the host-stepped fiber are not reached from the product: nothing
calls `runBootPrefix`, so they wait for the native frame owner that issue 0021 describes.

## Directory → namespace → class → responsibility

### `game/core/` — namespace `spider`

| File | Class / function | Responsibility |
|---|---|---|
| `main.cpp` | `main` | composes `psx::host::ProductHost` over `Spider1Catalog`; `-h`/`--help` and the one-executable override. Composition only. |
| `spider_port.{h,cpp}` | `spider::runPort` | Enter Electro's process boot (`titles/spiderman2/main.cpp`): devices in measured order, render path install, control channel and crt0, until that title joins the host catalog |
| `spider_runtime.{h,cpp}` | `SpiderRuntime` | the address-free lineage seam: serial, disc key (`discEnvVar`) and refusal wording both titles share. Owns no guest address. |
| `executable_identity.{h,cpp}` | `ExecutableIdentity`, `ExecutableIdentityStatus`, `verifyExecutable`, `verifyExecutableFile` | shipping serial, size, magic and SHA-256 authentication of the player's executable |
| `guest_execution.{h,cpp}` | `GuestExecution`, `reportExecutionResult` | one `Core`'s entry into, resumption at, and scoped original call through psxport's executor |
| `native_execution.h` | `dispatchGuestOrPropagate`, `callOriginalOrPropagate` | the two leaves a native override uses: dispatch a guest body, or bypass the current override for one call. Header-only, no state. |

### `game/render/` — namespace `spider::render`

Compiled as `spider_render_contracts`; nothing in the product links these yet (see
`docs/project-state.md` S005/S006). Each has a unit test except where noted.

| File | Class / function | Responsibility |
|---|---|---|
| `frame_envelope.{h,cpp}` | `FrameEnvelope` | the native DRAWENV/DISPENV producer: page flip, drawing area, background clear |
| `gpu_env.{h,cpp}` | `DrawEnv`, `DispEnv`, `gpu_env_is_pal` | the guest libgpu environment records and the video standard they were written under |
| `scene_id.{h,cpp}` | `SceneName`, `classifyScene` | the engine's own level-name-to-scene encoder, ported, so a scene with no producer names itself |
| `mesh_face_format.{h,cpp}` | `MeshFaceHeader`, `MeshSourceVertex`, `deriveMeshLayout` and the decode functions | the retail mesh header, face stream and caller-family layout contract |
| `mesh_transform.{h,cpp}` | `inspectMeshDirectTransform` | pre-GTE camera/object/relative transform decode for the direct mesh path |
| `mesh_animated_vertex.{h,cpp}` | `decodeAnimatedVertexRecord`, `accumulateAnimatedVertex` | projection/reuse/retain and near/far fixed-point input semantics |
| `mesh_pose_contract.{h,cpp}` | `decodeMeshPoseInput`, `meshPoseSamplesCanInterpolate` | pre-GTE base, secondary and authored-pose decode plus temporal identity |
| `mesh_asset_cook.{h,cpp}` | `MeshAssetCookLedger` | which mesh bytes are already resident where, and how to bring the rest in |
| `mips_fixed_point.{h,cpp}` | `mipsSignedHalf`, `mipsSignedWord`, `mipsArithmeticShiftRight4` | signed packed decode and the retail arithmetic-shift-by-four |

### `titles/spiderman1/` — namespace `spider::spider1`

| File | Class / function | Responsibility |
|---|---|---|
| `spider1_catalog.{h,cpp}` | `Spider1Catalog` | the host catalog: Spider-Man 1's identity, built from `title.json` (id, label, serial, size, SHA-256, PS-X EXE header words). Spider-Man 2 joins when it boots (EE-02). |
| `spider1_bootstrap_turn.{h,cpp}` | `Spider1BootstrapTurn` | one host step of the retail program: enter main after the host's crt0, service the boundary the guest stopped at, resume to the next; a turn that ended on the cycle budget delivers the one display field that time covers and resumes |
| `spider1_runtime.{h,cpp}` | `Spider1Runtime` | the authenticated `SLUS_008.75` image policy: program image, platform-HLE plan, pad and CD callback layouts, render capabilities, widescreen owner, CD-stream service, overlay-heap arena for `stockCdReadLanded` (published through psxport `guest_code_module`) |
| `spider1_frame_driver.{h,cpp}` | `Spider1FrameDriver` | **the frame turn for this title**: field delivery, the one presentation fence per host step, the boot overrides, and the movie/stream/VSync boundaries. Implements `Spider1ModeHost`. |
| `spider1_host_stepped_fiber.{h,cpp}` | `Spider1HostSteppedFiber`, `Spider1FiberPhase` | the finite host-stepped fiber the non-returning boot prefix and mode loops run on, its outstanding field wait, and its single handoff from boot to the mode phase |
| `spider1_field_schedule.{h,cpp}` | `planFiberResume`, `planFiberYield` | the two pure rules that decide, from what the fiber is waiting for, whether this host step delivers a field, resumes, or commits a fence |
| `spider1_field_clock.{h,cpp}` | `Spider1FieldClock` | the display field counter and the value `VSync(0)` returns, read from the guest's own words |
| `spider1_mode_driver.{h,cpp}` | `Spider1ModeDriver`, `Spider1OuterRoute`, `outerRouteFor`, `modePreservesOuterClear` | the retail outer selector and the primary mode's route arms |
| `spider1_mode_decisions.h` | the `mode*` rules | every comparison a mode DECIDES by, as `constexpr` functions of what it observed |
| `spider1_mode_host.h` | `Spider1ModeHost` | the whole of what a mode may ask the frame owner for: fields and a presentation fence. `Spider1FrameDriver` is its only implementation. |
| `spider1_menu_mode.{h,cpp}` | `Spider1MenuMode` | the retail title menu function's begin/step/finish |
| `spider1_alternate_mode.{h,cpp}` | `Spider1AlternateMode` | the alternate (resource/overlay) mode function's begin/step/finish |
| `spider1_invalid_selector_input.{h,cpp}` | `Spider1InvalidSelectorInput` | the input wait an invalid outer selector falls into |
| `spider1_transition_wipe.{h,cpp}` | `Spider1TransitionWipe` | the 3D transition wipe between modes, including its per-pixel darkening rule |
| `spider1_mode_frame_boundary.{h,cpp}` | `modeDrainDrawFields`, `modeWaitUnadvancedField`, `modeCompleteFrameHandshake` | the display-field boundary all three retail modes share |
| `spider1_guest_call.h` | `enterGuestCall` | the mode-driver convention for entering a retail guest function and treating a bounded exit as a completed call |
| `spider1_guest_stack_frame.h` | `GuestStackFrame` | the bounded guest stack window one synchronous call chain owns |
| `spider1_guest_layout.h` | constants | **the one owner** of every named `SLUS_008.75` guest word, table offset, mode id and threshold, at a name; unattributed facts are named `kUnattributed…` |
| `spider1_platform_facts.h` | constants, `isMovieFieldReturn` | the measured service addresses and the pad/CD callback slots the runtime publishes to the framework |
| `spider1_gpu_reset.{h,cpp}` | `Spider1GpuReset::reset` | the `ResetGraph` body minus its `VSync(0)`, which the field owner supplies |
| `spider1_cd_initialization.{h,cpp}` | `Spider1CdInitialization` | the retail `CdInit` body: the four callback slots and the CD interrupt arm |
| `spider1_stream_driver.{h,cpp}` | `Spider1StreamDriver` | the `StGetNext` boundary: super-call the guest body, pump the controller on a dry poll, yield the field that poll waited through, and report the ring and producer state |
| `spider1_movie_execution.{h,cpp}` | `Spider1MovieExecution` | resumes the unchanged retail STR player through the runtime executor at its three authenticated field boundaries |
| `spider1_widescreen.{h,cpp}` | `Spider1Widescreen`, `Spider1ViewportOffset`, `installSpider1Widescreen` | the title's own projection and draw-clip publication: which aspect the player chose, and the measured widening of the guest's horizontal window |
| `title.json` | — | serial, executable identity and disc-environment key |

### `titles/spiderman2/` — namespace `spider::enterelectro`

| File | Class / function | Responsibility |
|---|---|---|
| `main.cpp` | `main` | composition only |
| `enter_electro_runtime.{h,cpp}` | `EnterElectroRuntime` | the measured `SLUS_013.78` image policy and the capability refusal that keeps its render path off until its own owners exist |

### `tests/` — CTest registrations in `CMakeLists.txt`

Hermetic contract and ownership tests: `spider1_mode_rules`, `spider1_mode_transition`,
`spider1_field_schedule`, `spider1_guest_boundary_owners`, `spider1_runtime_services`,
`spider1_widescreen`, `mesh_*`, `executable_identity`,
`cd_irq2_delivery`, plus the Python `title_catalog`, `executable_provision`, `launcher_help` and
`source_policy` checks.

## Who owns it

### The frame turn, including while a movie or a loading call blocks

| Hop | Owner | What it decides |
| --- | --- | --- |
| One field, services around the body | `psx::FieldTurn::beginField` / `endField` (`runtime/psx/frame/field_turn.*`) | pause answer, watchdog re-arm, RAM-dump frame, one queued command. The BODY is the title's. |
| The body | `Spider1FrameDriver::stepFrame` → `Spider1BootstrapTurn::step` | services the boundary the guest stopped at (one field, one fence, `Game::run.fieldDelivered()`), then resumes the guest; a non-boundary stop logs the exit, commits an unpresented fence and calls `Game::run.requestEnd()` |
| A retail mode step (not reached by the product) | `Spider1ModeDriver::step` → `Spider1MenuMode` / `Spider1AlternateMode` / `Spider1InvalidSelectorInput` / `Spider1TransitionWipe` | the guest calls of that mode, ending in one of `modeWaitUnadvancedField` / `modeDrainDrawFields` / `modeCompleteFrameHandshake` |
| Presenting a step (mode path, not reached) | `Spider1FrameDriver::commitSubmittedFrame` / `commitRepeatedFieldFrame` / `commitUnpresentedFrame` | submitted, repeated-field or unpresented, all through `claimFrameFence` — one fence per host step, a second is an abort |
| A fiber blocked at a field boundary (not reached) | `Spider1HostSteppedFiber::yieldField` / `resume` / `clearFieldWait` | what the next host step does about it; `planFiberResume` / `planFiberYield` are the whole rule |
| **While a movie or stream poll blocks** | `Spider1FrameDriver::deliverField` and `commitMovieField` | **the frame driver still owns the turn.** `serviceBootstrapMovieVsync` and `serviceBootstrapStreamWait` deliver exactly one field, commit one fence, and continue at the guest's own continuation PC; the guest resumes at its continuation PC in the same step |
| Boot | the host's `TitleSession::boot` (`crt0_setup` from `Spider1Runtime::guestProgramImage()`), then `Spider1Runtime::bootInit` → `Spider1BootstrapTurn::begin` | crt0's bss, stack, heap and libc init are the framework's, audited against the guest's own crt0; the title enters retail main at `gameMainEntry` with crt0's return address |
| Boot prefix → mode driver handoff (not reached) | `Spider1FrameDriver::runBootPrefix` → `beginBoot` → `finishBoot` | the finite prefix's one-time handoff from the boot fiber to the mode driver |

### Host input → guest pad buffer, movie skip and the control channel

| Hop | Owner | What it decides |
| --- | --- | --- |
| The ONE SDL drain | `psx::input::HostInput::drainEvents` (`runtime/psx/input/host_input.*`) | records key state, feeds the overlay, ends the process on a window close |
| This turn's mask | `psx::input::HostInput::poll(bool)` → `Pad::pollHostInput` (framework) | force/replay/hold wins over the host mask, so a headless leg keeps its own input |
| The per-frame pump | `serviceBootstrapMovieVsync` / `serviceBootstrapStreamWait` → `game_.pad.serviceFrame()` (framework `Pad`, `runtime/psx/input/pad_input.*`) | once per serviced movie or stream field, before the fence |
| The guest's field-time packet | framework `Pad::fillBuffer` into the slots `Spider1Runtime::guestPadBufferLayout()` names | the 4-byte digital packet per VBlank |
| A retail pad read from guest code | `Spider1FrameDriver::serviceBootTail` (override on `padRead`) | super-calls the guest body; during the post-logo boot wait it yields the field the guest is blocked on |
| **Movie skip** | the same `Pad` mask, sampled by `Spider1FrameDriver::serviceBootstrapMovieVsync` each STR field | the guest keeps its own `VSync(0)` answer; the host only supplies the field and the pad frame that goes with it. There is no second pad drain and no movie-local key rule. |
| The debug / control channel | framework `DbgServer` (`runtime/psx/debug/dbg_server.*`), attached by the host's `TitleSession::boot` | pause, step, `guest`, and the guest-call census. The port only runs its own bootstrap turns; `resumeBootstrapBoundary` is what accepts a resume there |

### Guest draw → presentation

| Hop | Owner | What it decides |
| --- | --- | --- |
| The guest's own draw | translated guest code through psxport's Lightrec runtime | retail OT/GTE output; no title code intercepts it |
| The projection the frame uses | `Spider1Widescreen::synchronizePresentation` (mode path only; the bootstrap path does not call it) | re-latches the plan for the live display extent and applies the guest projection/draw-clip widening |
| Which frame is being committed | `Spider1FrameDriver::commitSubmittedFrame` (`FrameDriver`'s three commit leaves) | the one fence, and whether the interpolation owner's captured queue is presented with it |
| The present itself | framework `FramePresenter::commit` / `commitUnpresented` (`runtime/psx/frame/frame_presenter.*`) | the real field, the pacer, and the widescreen canvas |
| Future native producers | `spider::render::FrameEnvelope`, `SceneName` | compiled, not attached; see `docs/project-state.md` S005/S006 |

### CD and streaming

| Hop | Owner | What it decides |
| --- | --- | --- |
| Command channel | framework CD override (`runtime/psx/cd/`); `Spider1Runtime::platformHlePlan()` names the title's `CdRead`/`CdReadSync`/`CD_cw` entries | the synchronous read primitive |
| Initialization | `Spider1CdInitialization::install` (override on `cdInit`) | the four callback slots and the CD interrupt arm (`docs/issues/0024`) |
| A dry `StGetNext` poll | `Spider1StreamDriver::poll` (override, super-calls the guest body) | one controller pump, one retry, then the field the poll waited through |
| The field that poll waited through | `Spider1FrameDriver::streamWaitField` | the fiber's field boundary while a finite fiber runs; a typed cooperative exit in the direct boot |
| The CD interrupt service | the guest's own `0x8008C3E0` through Lightrec (no override: it acknowledges the controller) | drain, acknowledge, the sync/ready result mask |
| The frame-ready DMA callback | framework `Hle::irqPoll` through `platformServices.dmaCallbackTable` (`0x800B4388`) | libstr's `0x8008DB44` once per STR frame |
| Callback delivery | framework `cd_ready_delivery` (`runtime/psx/cd/`), through the slots `Spider1Runtime::guestCdStreamCallbackLayout()` names | the ready/sync callbacks |

### Audio

| Hop | Owner | What it decides |
| --- | --- | --- |
| One audio frame per display field | `Spider1FrameDriver::deliverField` → `game_.spu_audio.frame()` (framework `runtime/psx/audio/spu_audio.*`) | the SPU advances exactly once per field the title delivers, wherever that field came from |
| Mixers and streaming | framework SPU/CDX owners, named by `Spider1Runtime` | not title code |

### Debug options

| Hop | Owner | What it decides |
| --- | --- | --- |
| The control channel | framework `DbgServer` (`runtime/psx/debug/`), attached by the host's `TitleSession::boot` (Enter Electro: `runPort`) | pause/step/`guest`/quit, always on loopback, plus the picker's `picker`, `pick <slug>`, `select` and `session return` |
| Environment knobs | the framework's configuration owner, read once at boot | `PSXPORT_DEBUG`, the control port, the asset directory. No title file reads the environment. |
| Title debug knobs | none; the title has no dev-only selector | the guest's own paths are the debug options |

## Source tree

```text
game/core/     — process boot and the address-free lineage seam
game/render/   — title-neutral render contracts (compiled, not attached)
titles/spiderman1/ — every SLUS_008.75 fact and every owner of a guest boundary
titles/spiderman2/ — the SLUS_013.78 image policy and its refusals
tools/         — the launcher, provisioning, verification and RE-query owners
tests/         — hermetic contract and ownership tests
```

Refresh the line counts with:

```sh
uv run --frozen python ../../shared/re-harness/tools/codemap.py tree game titles tools tests --depth 2 --min-lines 1
```

## Where does X go?

- A title serial, executable hash, PS-X EXE header word or target label → `titles/<title>/title.json`; the host catalog and the runtime read it through CMake definitions, never a second copy.
- A guest address, table offset, mode id or threshold → `titles/<title>/`'s one guest-layout header,
  declared at a name; `kUnattributed…` when the role is unknown.
- A rule a mode DECIDES by → a `constexpr` function in `spider1_mode_decisions.h` or
  `spider1_field_schedule.h`, so its whole input space is testable without a `Core`.
- A retail mode function with its own lifetime → its own owner under `titles/spiderman1/`, composed
  by `Spider1ModeDriver`. A driver that grows a mode's state inline is the smell.
- A title address, image identity or override → `titles/<title>/`, never `SpiderRuntime`.
- A display field, a presentation fence, or anything a movie or a stream poll blocks on →
  `Spider1FrameDriver`, and only there.
- PSX instruction semantics, cache ownership, bounded exits, pad transport, presentation pacing or
  the control channel → `external/psxport/runtime/psx/<subsystem>/`, never a title workaround.
- Host rendering orchestration → `game/render/`, address-free; title render addresses stay in
  `titles/<title>/`.
- A renderer or runtime capability declaration → the title-derived runtime, never the lineage base.
- A factual capability change → `docs/project-state.md`; a task, bug or finding → `docs/issues/`;
  a placement change → this file.