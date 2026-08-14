# Codemap — spider1

The orientation map: what is where, what is done, what is missing. Consult at the START of a task to
avoid re-deriving structure; update in the SAME commit that lands or changes a subsystem.

A subsystem is marked **done** only when VERIFIED on real data, and the verification is named. Nothing
is marked done to look better — an honest "missing" is what makes this file worth reading.

---

## Title coverage

| title | status |
|---|---|
| Spider-Man | 🟡 current implementation; blocked at disc initialization |
| Spider-Man 2: Enter Electro | ⬜ title slot only; executable identity and seam not yet derived |

The repository-level `game/` currently implements Spider-Man 1. Shared lineage code must be based on
measured common behavior when Enter Electro work begins.

## What this project is

A native PC port of the PSX Spider-Man (`SLUS_008.75`, USA) built on
[psxport](https://github.com/SomeoneIsWorking/psxport): the game's MIPS code is **statically
recompiled** into C (the "substrate"), which runs under a native platform layer — so the result is a
PC program, not an emulator. Native reimplementation then grows function by function on top, each
step gated against the recompiled reference.

The framework is a submodule and carries no game code. This repo adds only the game seam and the
recompiled substrate.

## Repo layout

```
game/core/           the framework↔game seam (boot spine, GameConfig, HLE'd primitives, loaders)
game/render/         the render seam on the engine's own submitFrame + the scene-identity lens
external/psxport/    the framework, as a submodule (recompiler, runtime, PSX HW, harness, renderer)
generated/           the recompiled substrate — REGENERATED, never committed, never hand-edited
tools/               provisioning + RE helpers
cmake/               the port build definition
docs/                this map, the RE frontier, the issue catalog, the claims/instruments ledgers
scratch/             all run artifacts (gitignored) — logs, dumps, screenshots, the extracted exe
```

## Subsystem status

**Reading the citations — there are TWO ID spaces per ledger and they do not line up.** The
single-file ledgers `docs/info/instruments.md` and `docs/info/claims.md` number `INST-01..19` and
`CLAIM-00..09`; the per-file registries `docs/info/instruments/` and `docs/info/claims/` number
`I001..I014` and `C001..C021`. A shared number means NOTHING: `INST-05` is `PSXPORT_DEBUG=cdc`
while `I005` is `tools/check_reloc_model.py`, and `CLAIM-08` (offline module relocation reproduces
the guest's loader, holds) is a DIFFERENT claim from `C008` (0x8002A5F4 is never called, falsified).
Cite the exact form. `CLAIM-Cnnn` in this file means the per-file `Cnnn`.

| Subsystem | Where | Status |
|---|---|---|
| Disc provisioning + static recompilation | `tools/ensure_recomp.py`, `game/recomp_seeds.json` | **done** — hash-gated; MAIN + **30 runtime-loaded modules**, seeded only from the binary |
| Runtime module extraction + relocation | `tools/extract_modules.py` | **done** — offline half of the guest's own loader; relocating `shell.bin` reproduces guest RAM byte-for-byte (CLAIM-08). Also writes each module's `<stem>.reloc.json` sidecar (HI16 offsets) that the recompiler needs to emit it base-relative |
| Relocation-model **static** check | `tools/check_reloc_model.py` | **done** — checks the shape assumptions base-relative emission rests on across all 30 modules (8883 sites); self-tested (`--selftest`), refuses rather than returning empty on a missing corpus (I005). **NOT a run gate** — it never launches the binary. This row used to call it "the relocation-model gate", which invited exactly that misreading (issue 0014) |
| **THE RUN GATE** — does the built binary still boot? | `tools/gate.py` | **done 2026-08-12** (issue 0014, which recorded that NOTHING in this repo drove the built binary). `python3 tools/gate.py boot` launches `scratch/bin/spiderman_port` headless under `gpuguard run`, capped, and asserts on the port's OWN log lines: boot exe loaded, guest main dispatched, render seam installed AND fired (`[rseam] submitFrame override REACHED`), counters ADVANCED at ≥35% of the recorded baseline rate (14007 frames / 5632 submits in 240s), ≥2 scene changes into NAMED scenes, and no failure pattern. It cannot use the REPL — this port never enters the framework frame loop that services it — so it keys on log lines from a plain capped launch. `--selftest` judges a known-good capture plus 16 broken variants (11 FAIL, 4 REFUSE, 1 GPU-device-loss STOP), 15 through the analyser and 2 through the REAL launcher; `check-log <path>` runs the same analyser over any captured log. Refusals exit 2, device loss exits 3. Launcher audited the same day: the hang branch had NO coverage and two real defects (a refusal that saved an empty log because the port logs only to stderr; a child-only kill that orphaned 2 GPU-holding processes per hang) — both fixed, `killpg` on its own session, and selftest case 17 was run against the pre-fix code to prove it discriminates |
| Build (framework + game + substrate) | `CMakeLists.txt`, `cmake/spiderman_port.cmake` | **done** |
| `GameConfig` boot/crt0 group | `game/core/game_config.cpp` | **done** — RE-verified against the crt0 at `0x8008739C` |
| Generated-substrate seam | `game/core/recomp_register.cpp` | **done** |
| `GameHooks` vtable | `game/core/game_hooks.cpp` | **done for Phase 0** — and note WHICH members are structurally unreachable here: `drawOTag`, `frameUpdate` and the PcScheduler hooks have call sites ONLY inside the framework's `native_step_frame`, which this port never enters (C025). Their fail-fast bodies are correct and must stay stubs; do not implement one expecting it to run |
| Boot spine | `game/core/main.cpp` | **done for Phase 0** — the guest's own `main` runs on the substrate |
| libetc `VSync` + field clock | `game/core/sync_native.cpp` | **done** — RE-verified; counter free-runs at 60000/1001 Hz |
| VSyncCallback + host turn | `game/core/sync_native.cpp`, framework `host_turn.cpp` | **done** — the port owns the frame clock and dispatches the guest's per-vblank callback |
| Windowed frame pacing (`GameConfig::paceQuota`) | `game/core/game_config.cpp` (the value + its derivation), `game/core/sync_native.cpp` (both call sites + the `PSXPORT_DEBUG=pace` instrument), framework `gpu_native.cpp` `gpu_pace_subframe` | **done, MEASURED WINDOWED** — `paceQuota` is the vblanks ONE `gpu_pace_frame` call represents, and both of this port's call sites are field-wait loop iterations, so it is 1. It was 2 (a guess at the game's display rate), which served every one-field guest wait with a two-field sleep and halved the frame rate: 15.57 rendered fps → 29.66, `presents/rebuild_geom` 3.85 → 2.02, at an unchanged 59.94 presents/s (issue 0010, C024). Instrument I020 (`PSXPORT_DEBUG=pace`) is **windowed-only** — `gpu_pace_subframe` early-returns without a window, so headless measures nothing here |
| CD stack (stock Sony libcd) | framework `cd_override.cpp` + `GameConfig` | **done** — every CD op served natively from the disc image; retries 38 → 0 |
| CD stream pump (XA / STR) | `game/core/cd_stream.cpp` | **delivery verified, pacing and audio NOT** — overrides `StGetNext` (0x80086B10): when the guest finds no ring slot ready, pump exactly one sector through the guest's own `CdReadyCallback` (0x800860B4 → `FUN_80085000`) and ask again, once. The port supplies only the drive's PACING; libstr keeps every ring invariant, and nothing here touches the ring. Named as RE-07's `where:`, and sector delivery is byte-correct there (C016). **Not verified:** this pump's audio and frame-pacing behaviour — every run so far used `PSXPORT_NOAUDIO=1` and no run has been compared against the movies' real duration; that measurement is IN FLIGHT (2026-08-05), so treat the pacing as unknown, not as working. Carries the `PSXPORT_DEBUG=ring` instrument — I011, which corrects and supersedes I008 (the older version printed a fixed 12 slots instead of the guest's own count) |
| Diagnostic entry probes | `game/core/diag_overrides.cpp` | **done as an instrument bank** — not a ported subsystem: eleven observe-only overrides across nine channels (`cdarg` 0x8008CE8C, `alloc` 0x800651C8, `s1trace` 0x8008C944, `cdinit` 0x8008D4E4 + 0x8008D3F4, `cdisr` 0x8008C3E0, `cdcb` 0x8009152C + 0x800913AC, `coroentry` 0x8002A338, `hedname` 0x80064B3C, `geomwatch` 0x80075D0C). Each installs ONLY when its channel is on and each super-calls the original body, so an armed run executes exactly what an unarmed one does; each prints an ARM line, so silence is a result rather than an absence. This is where the RE-16 evidence came from (the callee-contract check on 0x8002A338, the pre-registered falsifier for C011) and where C008's scope correction is recorded (C010). Wart: the older ten are still on the retiring `cfg_*` shim; `geomwatch` (added 2026-08-06) is on `lucent::` with an interned `lucent::Channel` guarding the INSTALL rather than a log call, which is the shape the rest should be converted to |
| Runtime module placement + routing | `game/core/module_loader.cpp`, framework `overlay_router.cpp` | **done** — modules are recompiled BASE-RELATIVE and the game's own allocator places each body, as the console does. This file only OBSERVES (name at load, base at allocation #2, eviction at free); nothing is pinned or redirected. Three modules co-resident at distinct bases verified on a real boot: 7176 frames, 0 `recomp-MISS` (CLAIM-C013, re-frontier RE-09/HACK-02) |
| Input (pad) | `GameConfig` pad group | **done** — verified behaviourally: a forced DOWN moves the menu selection and changes the screen |
| Memory card | `GameConfig` card group + framework `memcard.cpp` | **done** — 128 KB image created/formatted; the card check COMPLETES and the game advances |
| Rendering (via the framework's VK path) | framework `gpu_native.cpp`, `gpu_vk.cpp` (present stage: `present_plan.h` → `build_present_image` / `show_present_image`) | **renders in BOTH legs, and the present stage is now instrumented.** The windowed black screen is fixed (swapchain present mode, C021/issue 0005 — MAILBOX is requested at claim time; a windowed run reaches `vram_writes=11324` over 4354 presents and shows the picture). The composite no longer branches on the leg: `present()` used to return above `show_composite` when headless, so headless skipped the picture stage entirely — that was the mechanism behind every "99.95% non-black headless / black in a window" reading. **Verified 2026-08-05 on a NATIVELY RENDERED frame:** the main menu at present 3900 (15-bit 512x240, the game's own render mode) presents correctly — VRAM 99.4% non-black / 1048 colours, present shot 62.1% / 1048 at a 960x720 sink, the difference being exactly the widescreen letterbox (512:240 fills 450 of 720 rows = 62.5%; 99.4 x 0.625 = 62.1). **Not verified:** anything past the blit — the swapchain image itself is still unsampled (INST-20) |
| The GUEST's frame driver + engine render seam | guest `FUN_8002C174` (loop), `FUN_800612B8` (beginFrame), `FUN_80061308` (submitFrame) — nothing native yet | **RE-VERIFIED 2026-08-06, and this is where a native renderer attaches (RE-19).** The loop is `while (*0x800B4F34 == 0) { beginFrame; game logic; render walk; OT relink; ensure ≥1 field; DrawSync spin; submitFrame }`. `FUN_800612B8` flips the DB context `0x800B54A8` between `0x8009A6E4`/`0x8009A75C` and `ClearOTagR(ot, 0x1000)`; `FUN_80061308` does `ResetGraph(1)` / `PutDispEnv(ctx+0x5C)` / `PutDrawEnv(ctx+0)` / `DrawOTag(ot+0x3FFC)`. The libgpu leaves are identified by the strings libgpu itself prints — `DrawOTag` 0x80081ED0, `ClearOTagR` 0x80081DC8, `PutDispEnv` 0x80082000, `PutDrawEnv` 0x80081F40, `DrawSync` 0x800819A4, `ResetGraph` 0x8008173C. **`DrawOTag` has exactly 2 callers image-wide**, one library-internal, so `FUN_80061308` is the ONE game-side OT submit — and it is a MAIN-module address the recomp override table reaches today. Measured headless, 100 s, no input: 1761 calls to `0x80061308`, 1765 to `0x800612B8`, 2 entries to `FUN_8002C174`, 0 ABI violations (`scratch/re12/logs/fntrace1.log`) |
| Native frame loop / OT / packet pool | — | **layout RE-VERIFIED, native loop deliberately NOT built (RE-12, RE-22).** The DB pair is the Sony standard: `ctx[0]=0x8009A6E4`, `ctx[1]=0x8009A75C` (stride 0x78; DRAWENV +0x00 / DISPENV +0x5C / `ot` +0x70 / `pool` +0x74), a **512x240** framebuffer with pages at VRAM y=0 and y=256 (`FUN_80061140`'s SetDefDrawEnv/SetDefDispEnv args, independently confirmed by the runtime `[gpu] display depth -> 15-bit (GP1(08)=08000002, 512x240)` line). `FUN_80061230` **heap-allocates** ot=0x4000 x2 and pool=0x17000 x2; `PSXPORT_WWATCH=8009A754,8009A7D4` shows the four pointer slots written once each and never again (`scratch/re12/logs/wwatch_ot.log`). **So `GameConfig`'s `otRegionBase`/`packetPoolBase` group stays ZERO on purpose — it is literal-shaped and this game has no literal.** Feeding the framework's `native_step_frame` would need pointer-indirection fields, i.e. a framework change. Separately, `hooks->drawOTag`/`frameUpdate`/PcScheduler stay unreachable (C025) and their fail-fast stubs stay stubs — the port does not need them |
| Scheduler / outer modes | `FUN_8002C354`, selector `gp+0x740` (`0x800B4F34`) | **partially RE'd (RE-13)** — this is not a Tomba!2-style scheduler: after each mode loop returns, `main()` jump-table-dispatches selector values 1..10. Ghidra's 18 refs to the selector are direct reads/writes; reset is `FUN_8006BF9C`, with transition writers including `FUN_8006EE28` and `FUN_80049ED0`. **Still missing:** any task-slot/object model and the WITHIN-level state (front-end page, cutscene, play). **No longer the blocker for a scene classifier:** RE-23 found the level-name lens (`0x800A568C` + encoder `FUN_8005A734`, consumed by `FUN_80062CE0`'s per-frame switch over `0x201..0x803`). C026's module-registry census remains true but was the wrong source to generalise from. |
| **Render seam** (where the port owns the picture) | `game/render/render_seam.cpp` — installed from `game/core/main.cpp` before `native_boot_run()` | **done, PROVEN INSTALLED AND RUN (RE-20)** — a recomp override on the guest's own submitFrame `FUN_80061308`, the ONE game-side `DrawOTag` caller. Two legs: `psx_render` super-calls the recompiled body (the reference, and this port's DEFAULT — see the next row for why), `pc_render` runs no gen body and dispatches `renderScene()`. Everything outside the super-call runs inside a framework `DisplayPassGuard`, so a guest store from this file aborts with a backtrace rather than being asserted away. Reach evidence is emitted DURING the run, not at exit (`[rseam] submitFrame override REACHED — call #1 at frame 2, ra=80061218`, matching RE-19's independent fntrace measurement digit-for-digit, then a count every 512 calls): the watchdog owns SIGINT/SIGTERM and `_exit(130)`s, so an `atexit` summary would silently never print — instrument I027. **BREAK-FIRST CONTROL MEASURED (C031):** with the submit suppressed in a throwaway build the presented picture FREEZES — 1 distinct picture over 20 presents, 0 of 13,132,800 pixel comparisons changed — while the same instrument on the reference leg shows 3 pictures and up to 32.75% of the frame changing. So this seam demonstrably owns the picture |
| Scene identity (`classifyScene`'s data) | `game/render/scene_id.cpp` — `SceneName`, the port of guest `FUN_8005A734` | **RE-VERIFIED + RUNTIME CENSUS (RE-23, C032)** — the current level name at `0x800A568C`, folded into `(level<<8)\|sub`, the value the engine's own per-frame state machine `FUN_80062CE0` switches on. Census over a 200 s headless run (1024+ submitFrame calls / 3300 presents): unset → `dem1` (0x9901) → `l1a1` (0x0101), against the module registry's effectively-one value over 13757 presents (C026). **Not covered:** nothing correlates a code with a PICTURE yet, so `l1a1` has NOT been shown to separate the attract fly-through from live gameplay; within-level substate is still RE-13 |
| Native renderer (`pc_render`) + scene classifier | `game/render/render_seam.cpp` `renderScene()` / `abortUnimplemented()` | **ONE producer lands; ZERO display-list producers, and every scene but one still aborts — the correct state (RE-18 `re-partial`).** `PSXPORT_RENDER_PSX=0` now RENDERS the boot-init scene `'....'` natively (the frame envelope — next row) and aborts at submitFrame **call #2**, scene `'dem1'`, naming it. The abort carries the DB lens, `geomValid`, the envelope's own `produced=`/`clears=` counters, and dumps the whole 1024x512 CPU VRAM to `scratch/raw/native_abort_vram.ppm` so 'it aborted' is separable from 'the producers that did run drew nothing'. The `'....'` case is NOT a hardcoded pass: `renderScene()` re-walks the OT and aborts if that frame ever carries a pixel-writing primitive, so a build that silently dropped part of the boot picture crashes instead of looking finished. No OT-walk fallback, no escape hatch. **The DEFAULT is still the reference leg**, and the condition for flipping it is not 'a producer exists' but 'every scene a boot passes through has one' — today that is `'....'` and not `'dem1'`. **What is missing is RE-21**: no display-object type is decoded, so the abort still names the SCENE and not the unhandled TYPE |
| **Frame envelope** — THE FIRST NATIVE PRODUCER | `game/render/frame_envelope.cpp` + `game/render/gpu_env.cpp` (the DRAWENV/DISPENV lenses and the ported libgpu word builders) | **done, and WORD-EXACT against libgpu (C033).** The port of what submitFrame does either side of `DrawOTag`: the GP1(05) page flip, GP1(08)/GP1(07) display geometry, the GP0 E3/E4/E5/E1/E2/E6 drawing state, and the GP0(02)/GP0(60) background clear `isbg` performs. RE'd with Ghidra headless from `FUN_80081F40` (PutDrawEnv), `FUN_80082770` (the DR_ENV builder), `FUN_80082000` (PutDispEnv) and the five field->word leaves `FUN_80082A00/80082A98/80082B30/800829E0/80082B4C`; every input is the game's own submission struct, nothing is recovered from an OT link, a GP0 packet or the GTE. VERIFIED: `PSXPORT_DEBUG=envcheck` compares the port's 9 words against the guest's own DR_ENV packet at `DRAWENV+0x1C` after the super-call — **checked=2560 mismatch=0**, with a one-bit perturbation control that takes mismatch to 100%% (I029). **NOT verified: pixels.** The A/B against a producer-disabled build (one `#define`) measured **0 of 524288 pixels differing**, because in the only scene this producer fully owns both framebuffer pages are entirely black — C034, which records the exact condition that will make that gate meaningful. **NOT ported, deliberately:** `ResetGraph(1)` (a DMA-queue reset with no picture, `FUN_8008173C` mode 1) and `GP1(06)` (needs libgpu's un-RE'd CRT timing tables at 0x800B0EFC/0x800B0F24, and this framework discards GP1(06) outright). **UNTESTED CODE, stated:** every DRAWENV observed had `clip.x=0`, `ofs.x=0`, a 64-aligned width and `isbg=1`, so the GP0(60) non-aligned clear branch and any non-zero texture window are ported but never exercised |
| Display-list inventory (RE-21's missing measurement) | `game/render/frame_census.cpp` | **done as an instrument (I028)** — `PSXPORT_DEBUG=fcensus` classifies each submitFrame call's ordering table by GP0 class next to the DRAWENV/DISPENV it submits; `fcensusv` dumps primitives capped by novelty. It supplied the fact the first producer rests on (the boot-init frame's entire display list is libgpu's own 2x1 self-copy terminator plus four no-ops) and it sizes the dem1 backlog (59 pixel-writing primitives on the first attract frame, ~400 polys + 115 lines at its peak). Diagnostic ONLY — a producer may not resolve geometry from it |
| Render walk / display-object list | guest `FUN_8002BD5C`, world render `FUN_80075D0C`, list heads `0x800B5238` / `0x800B5234` | **mapped at call-graph level only (RE-21)** — the frame's render phase calls `FUN_80075D0C(cam, viewport, ctx->ot)` (the sole caller of the libgte projection setters RE-17 already records), then a fixed list of per-object renderers (`FUN_80076480` x9, `FUN_80076FC0`, `FUN_800762D4`, `FUN_8007A764`/`FUN_8007ABDC`), then **two walks of a typed display-object linked list** — next `+0x1C`, 16-bit type `+0x34`, class/vtable `+0x3C` with `{+0x30 off, +0x34 fn}` for types 0x141/0x142/0x143 and `{+0x88 off, +0x8C fn}` for 0x134/0x135/0x136. **No struct is decoded and no type code is named** — that is the actual porting backlog |
| Renderer: GTE tap → native depth / widescreen | recompiler tap in `generated/` | **wired; execution status UNKNOWN — the old reason here is STALE** — 10 `gte_record_pz` sites in 6 projection fns. This row read "`records=0` over a full boot **because the port never reaches 3D**"; that reason no longer holds. The port demonstrably reaches live 3D gameplay: present 9500 of a 230 s windowed run is Spider-Man on a New York rooftop with the full HUD, 99.82% non-black / 1986 colours (`scratch/g8_shots/present_9500.png`, 2026-08-06) — which the "Current state" section below already says. **`records` has NOT been re-measured since the port started reaching 3D**, and the obvious instrument for it (`PSXPORT_DEBUG=ndepth`) is DISTRUSTED (I015), so do not assume either answer. Re-measuring this is the first move on RE-08 |
| Camera projection (OFX / OFY / H) | `game/core/game_config.cpp` `hle.setGeomOffset` = 0x8008BF24, `hle.setGeomScreen` = 0x8008BF14; recording in framework `proj_params.cpp`; probe in `game/core/diag_overrides.cpp` (`PSXPORT_DEBUG=geomwatch`) | **done — recorded at the setter, never tapped out of the GTE** (RE-17). The two libgte leaves were found by the instruction that marks them (`tools/ghidra_query.py scan ctc2`: 10 of 1404 `ctc2` sites touch CR24/25/26), and a raw scan of all 524,288 words of `ram.bin` confirms Ghidra's 24.9% code coverage hid no other site or caller. On a real boot `requireGeom()` returns **OFX=256 OFY=120 H=276** and does not abort; with the two addresses zeroed the same probe reports `valid=0` and `requireGeom` aborts. **Spider-Man states NO constants** — `FUN_80075D0C` (sole caller) computes them per viewport, so nothing is baked into `GameConfig` and nothing may be. OFX=256 is 512/2, this game's framebuffer centre, not 160. **Not verified:** only the boot/front-end viewport has been observed; a gameplay viewport switch (which will change H) has not, and nothing is compared against real hardware |
| Audio | `game/core/sync_native.cpp` (`vblank_advance` drives `spu_audio.frame()` once per owed field) + framework `spu_audio.cpp` / `xa_stream.cpp` | **PRODUCES AUDIO headless, NOT user-confirmed** (C022, issue 0005). The port previously had NO audio at all: `spu_audio.init()` was called and `spu_audio.frame()` was called nowhere, so the SPU mixer never advanced and nothing ever pulled `CDC_GetCDAudioSample` — the XA streamer armed at the intro movie's LBA and stopped there having decoded 0 sectors. Now 149940 pulls / 16 sectors on ATVILOGO and 160965 / 69 on LOGO, and a headless WAV capture is 115 audible buckets of 198. Census instrument: `PSXPORT_DEBUG=xa` reports pulls AND sectors at stream stop, so a silent stream says which side failed. **Unmeasured:** A/V sync — the XA streamer and the guest's video path run two independent read heads over the same file. `SpuAudio::init` still bails on `!gpu_windowed()`, which is the SDL SINK only; the mixer, the XA decode and `PSXPORT_WAV` all run headless |
| FMV / intro movies | framework DMA/MDEC path; guest `FUN_8002AA0C` player, `FUN_8002A338` decoder, `0x8002B28C` callback | **re-partial (RE-07).** Both boot logos decode into guest VRAM and the window starvation defect was fixed upstream. C036 now has bounded runtime skip evidence: boot ID0 produced Start and Cross edges after tick 30 and reached the common full teardown with active state cleared. Cross reached ID1 but did not produce its edge; queued and dispatcher paths were not reached. Remaining gaps: ID1/queued responsiveness, A/V sync, pacing against STR duration, and every non-STR in-engine sequence. |
| STR skip runtime discriminator | `game/core/str_skip_oracle.cpp`, qualified PCs in `game/recomp_seeds.json` | **instrument landed; bounded live evidence only.** `start` produced a post-30 edge and common teardown for boot ID0 (1/1), then retail held-Start suppression omitted ID1. `cross` reached boot IDs 0/1 (2/2), but produced a post-30 edge only for ID0 (1/2). Queued path is **MISSING CORPUS: 0 invocations; no verdict**. No behavior patch, menu-arrival claim, or non-STR claim. |
| DMA interrupt registers (DPCR/DICR) | framework `dma_irq.h`, `mem.cpp` | **done for DMA3** — the guest's per-channel IRQ enable is honoured, so a transfer it deliberately runs silent no longer signals its callback. Hermetic gate `tests/test_dma_irq_gate.cpp` (7 cases / 30 checks). DPCR is storage only, deliberately: two of the three consuming ports never write it, so gating transfers on it would read their untouched 0 as "all channels disabled" |
| The framework itself | `external/psxport/` | **not this repo's subsystem** — a submodule with its own history and its own codemap. Changes to it are made here but land in that repo; consumers pin their own commit |
| The recompiled substrate | `generated/` | **not a subsystem** — regenerated output of `tools/ensure_recomp.py`, never committed and never hand-edited. A mistranslation is fixed in the recompiler, not in its output |

## Where is X

- **The RE'd guest addresses** → `game/core/game_config.cpp`, each cited with its instruction.
- **Why a value is zero** → `docs/re-frontier.md`, by step. Query it with
  `RE_FRONTIER_ROADMAP=docs/re-frontier.md python3 tools/re_frontier.py next` (run from the repo
  root) — **the env var is required**, or the tool points at a path that does not exist (INST-14;
  it now exits 1 saying it checked NOTHING instead of printing OK). Do not use a
  `$CLAUDE_SKILLS`-relative path: that variable is unset in a plain shell, so the command
  collapses to `/re-frontier/re_frontier.py` and fails. **`set`/`add` are safe on a prose-bearing
  roadmap again** (issue 0003): they edit only the field lines named and refuse the write, naming
  what would be lost, if anything else would go missing. Guarded by
  `pytest tools/test_re_frontier.py` / `python3 tools/re_frontier.py selftest`.
- **What is proven and whether it still holds** → `docs/info/claims.md`.
- **Whether a measurement tool can be trusted** → `docs/info/instruments.md`.
- **A bug, a ruled-out cause, or a framework wart** → `docs/issues/`.
- **How to disassemble a guest address** → `tools/redump_ram.py`, then the framework's `disasm.py`.
  **Read the disassembly, not the decompiler**, for anything control-flow or timing shaped: Ghidra has
  twice sent this project down a wrong path (it hid that `$fp` was a global base register, and renders
  a snapshotted wait loop as an infinite one).
- **A picture of what the port actually draws** — TWO instruments, and they answer DIFFERENT
  questions. Confusing them cost this project a whole session (issue 0005), so pick deliberately:
  - `PSXPORT_SHOT_AT=<present>,...` → `scratch/screenshots/shot_<n>.ppm`. **Guest VRAM** at that
    present: what the game DREW. Blind to the entire composite (INST-18).
  - `python3 tools/present_geometry.py <shot.ppm>` — **is the picture the right SHAPE?** Every other
    check here is invariant under a stretch (issue 0008 hid behind that for a whole session), so
    this is the only one that can answer it. `--expect 16:9` when the widescreen mod is on.
  - `PSXPORT_PRESENT_SHOT_AT=<present>,...` → `scratch/screenshots/present_<n>.ppm`. **The presented
    picture**: after letterbox, fade, native-vs-ires selection and 24bpp decode — what a player
    SEES. Works in both legs (the headless sink defaults to the window's 960x720; override with
    `PSXPORT_PRESENT_SINK=WxH`). Still blind to the swapchain hop itself — INST-20 states where.

  Prefer either over primitive counters: counters say the pipeline moved data, only an image says
  what a player sees.
- **Driving the game without a controller** → `PSXPORT_FORCE_BUTTONS=<hex active-low mask>`
  (`0040` DOWN, `4000` CROSS, `0008` START).
- **Did my change break boot?** → `python3 tools/gate.py boot` (build first: `cmake --build build
  --target spiderman_port -j$(nproc)`). Never `./run.sh` — that is the user's windowed launcher and it
  re-syncs the framework submodule out from under an in-progress measurement.

## Current state in one line

The port provisions, recompiles, builds, boots, loads and rotates its 30 runtime code modules,
renders, and responds to input. It plays its intro FMVs, draws its front-end correctly (main menu →
memory-card check → new game → name entry), and **reaches 3D GAMEPLAY**.

**CORRECTED 2026-08-05 — this line previously said the port "stops on an unmapped read at
`0x80800004` when advancing further" past name entry. It does not.** Driving the front-end with
pulsed CROSS (`PSXPORT_FORCE_BUTTONS=4000`) runs to present 10000+ over 200 s with no abort, no
recomp miss and no watchdog trip, through the city-skyline screen and into a third-person 3D scene
with plausible geometry, camera and HUD. The stale line understated the port by a whole phase, and
it survived because until this session nothing could show the presented picture — so nobody had
driven past the front-end and *looked*. Treat the reach as verified; treat what it draws as not:

**AND IT DRAWS CORRECTLY THERE (2026-08-06).** Two independent defects were stacked on the same
frame and neither was visible while the other stood. (1) `psxport` implemented no VRAM->CPU readback
(`GP0(0xC0)`, GPUREAD and the DMA2 read direction were all dead), so the guest's save/modify/restore
over the palette strip restored allocator poison `0x33333333` over the live CLUTs and every textured
pixel became `vertex_colour x RGB(152,200,96)` — issue 0007. (2) The present letterbox used the
framebuffer width as the display aspect, stretching a 512x240 frame 1.6x wide — issue 0008.
Verified at present 10000: **1581 colours, 99.7% non-black, correct 4:3, no bars** (was 369 colours
of pale green in a 960x450 band). Spider-Man renders in red and blue on a New York rooftop with a
working HUD. **Still open:** the city-skyline screen (present 4500) shows heavy blocky corruption —
separate symptom, unexamined, and NOT assumed to be covered by either fix.


**AND THE PORT NOW HAS A RENDER SEAM (2026-08-06, RE-20).** The picture path is no longer entirely
the guest's: a game-side override on the engine's own `submitFrame` (`FUN_80061308`) is installed,
proven to run (first call at frame 2 from `ra=80061218`), and proven to OWN the picture — suppressing
it freezes the presented image dead (0 of 13,132,800 pixel comparisons change over 20 presents) while
the reference leg changes up to 32.75% of a frame in the same window (C031). **The native leg has
zero producers and aborts on its first engine frame naming the scene, which is the correct result** —
the work that remains is RE-21, decoding the display-object list the render walk `FUN_8002BD5C`
traverses. Until a producer exists the DEFAULT leg stays `psx_render`, so every other run is
unaffected.
