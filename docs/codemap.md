# Codemap — spider1

The orientation map: what is where, what is done, what is missing. Consult at the START of a task to
avoid re-deriving structure; update in the SAME commit that lands or changes a subsystem.

A subsystem is marked **done** only when VERIFIED on real data, and the verification is named. Nothing
is marked done to look better — an honest "missing" is what makes this file worth reading.

---

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
game/core/           the framework↔game seam (this repo's entire hand-written surface, today)
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
| Relocation-model gate | `tools/check_reloc_model.py` | **done** — checks the shape assumptions base-relative emission rests on across all 30 modules (8883 sites); self-tested (`--selftest`), refuses rather than returning empty on a missing corpus (I005) |
| Build (framework + game + substrate) | `CMakeLists.txt`, `cmake/spiderman_port.cmake` | **done** |
| `GameConfig` boot/crt0 group | `game/core/game_config.cpp` | **done** — RE-verified against the crt0 at `0x8008739C` |
| Generated-substrate seam | `game/core/recomp_register.cpp` | **done** |
| `GameHooks` vtable | `game/core/game_hooks.cpp` | **done for Phase 0** |
| Boot spine | `game/core/main.cpp` | **done for Phase 0** — the guest's own `main` runs on the substrate |
| libetc `VSync` + field clock | `game/core/sync_native.cpp` | **done** — RE-verified; counter free-runs at 60000/1001 Hz |
| VSyncCallback + host turn | `game/core/sync_native.cpp`, framework `host_turn.cpp` | **done** — the port owns the frame clock and dispatches the guest's per-vblank callback |
| CD stack (stock Sony libcd) | framework `cd_override.cpp` + `GameConfig` | **done** — every CD op served natively from the disc image; retries 38 → 0 |
| CD stream pump (XA / STR) | `game/core/cd_stream.cpp` | **delivery verified, pacing and audio NOT** — overrides `StGetNext` (0x80086B10): when the guest finds no ring slot ready, pump exactly one sector through the guest's own `CdReadyCallback` (0x800860B4 → `FUN_80085000`) and ask again, once. The port supplies only the drive's PACING; libstr keeps every ring invariant, and nothing here touches the ring. Named as RE-07's `where:`, and sector delivery is byte-correct there (C016). **Not verified:** this pump's audio and frame-pacing behaviour — every run so far used `PSXPORT_NOAUDIO=1` and no run has been compared against the movies' real duration; that measurement is IN FLIGHT (2026-08-05), so treat the pacing as unknown, not as working. Carries the `PSXPORT_DEBUG=ring` instrument — I011, which corrects and supersedes I008 (the older version printed a fixed 12 slots instead of the guest's own count) |
| Diagnostic entry probes | `game/core/diag_overrides.cpp` | **done as an instrument bank** — not a ported subsystem: ten observe-only overrides across nine channels (`cdarg` 0x8008CE8C, `alloc` 0x800651C8, `s1trace` 0x8008C944, `cdinit` 0x8008D4E4 + 0x8008D3F4, `cdisr` 0x8008C3E0, `cdcb` 0x8009152C + 0x800913AC, `coroentry` 0x8002A338, `hedname` 0x80064B3C). Each installs ONLY when its channel is on and each super-calls the original body, so an armed run executes exactly what an unarmed one does; each prints an ARM line, so silence is a result rather than an absence. This is where the RE-16 evidence came from (the callee-contract check on 0x8002A338, the pre-registered falsifier for C011) and where C008's scope correction is recorded (C010). Wart: still on the retiring `cfg_*` shim rather than `lucent::` |
| Runtime module placement + routing | `game/core/module_loader.cpp`, framework `overlay_router.cpp` | **done** — modules are recompiled BASE-RELATIVE and the game's own allocator places each body, as the console does. This file only OBSERVES (name at load, base at allocation #2, eviction at free); nothing is pinned or redirected. Three modules co-resident at distinct bases verified on a real boot: 7176 frames, 0 `recomp-MISS` (CLAIM-C013, re-frontier RE-09/HACK-02) |
| Input (pad) | `GameConfig` pad group | **done** — verified behaviourally: a forced DOWN moves the menu selection and changes the screen |
| Memory card | `GameConfig` card group + framework `memcard.cpp` | **done** — 128 KB image created/formatted; the card check COMPLETES and the game advances |
| Rendering (via the framework's VK path) | framework `gpu_native.cpp`, `gpu_vk.cpp` (present stage: `present_plan.h` → `build_present_image` / `show_present_image`) | **renders in BOTH legs, and the present stage is now instrumented.** The windowed black screen is fixed (swapchain present mode, C021/issue 0005 — MAILBOX is requested at claim time; a windowed run reaches `vram_writes=11324` over 4354 presents and shows the picture). The composite no longer branches on the leg: `present()` used to return above `show_composite` when headless, so headless skipped the picture stage entirely — that was the mechanism behind every "99.95% non-black headless / black in a window" reading. **Verified 2026-08-05 on a NATIVELY RENDERED frame:** the main menu at present 3900 (15-bit 512x240, the game's own render mode) presents correctly — VRAM 99.4% non-black / 1048 colours, present shot 62.1% / 1048 at a 960x720 sink, the difference being exactly the widescreen letterbox (512:240 fills 450 of 720 rows = 62.5%; 99.4 x 0.625 = 62.1). **Not verified:** anything past the blit — the swapchain image itself is still unsampled (INST-20) |
| Native frame loop / OT / packet pool | — | **missing** — that `GameConfig` group is deliberately zero; Phase 0 runs the guest's loop instead (RE-12) |
| Scheduler | — | **missing** — the SDK task model may not apply to this engine (RE-13) |
| Renderer: GTE tap → native depth / widescreen | recompiler tap in `generated/` | **wired, never executed** — 10 `gte_record_pz` sites in 6 projection fns; `records=0` over a full boot because the port never reaches 3D (RE-08) |
| Audio | `game/core/sync_native.cpp` (`vblank_advance` drives `spu_audio.frame()` once per owed field) + framework `spu_audio.cpp` / `xa_stream.cpp` | **PRODUCES AUDIO headless, NOT user-confirmed** (C022, issue 0005). The port previously had NO audio at all: `spu_audio.init()` was called and `spu_audio.frame()` was called nowhere, so the SPU mixer never advanced and nothing ever pulled `CDC_GetCDAudioSample` — the XA streamer armed at the intro movie's LBA and stopped there having decoded 0 sectors. Now 149940 pulls / 16 sectors on ATVILOGO and 160965 / 69 on LOGO, and a headless WAV capture is 115 audible buckets of 198. Census instrument: `PSXPORT_DEBUG=xa` reports pulls AND sectors at stream stop, so a silent stream says which side failed. **Unmeasured:** A/V sync — the XA streamer and the guest's video path run two independent read heads over the same file. `SpuAudio::init` still bails on `!gpu_windowed()`, which is the SDL SINK only; the mixer, the XA decode and `PSXPORT_WAV` all run headless |
| FMV / intro movies | framework `dma_irq.h`/`mem.cpp`/`hle.cpp` + `tools/recomp/emit.py`; guest libstr + `FUN_8002AA0C` (player), `FUN_8002A338` (DecDCTvlc), `0x8002B28C` (MDEC-out callback). **Windowed black screen is owned by `external/psxport/runtime/recomp/gpu_vk.cpp` (swapchain), not by anything in this row** | **decodes headless, BLACK IN A WINDOW** (RE-07 is `re-partial`; C019 falsified, re-issued scoped as C020; issue 0005). **Headless** both logo movies decode into guest VRAM: f120 = 99.95% non-black / 11395 colours (ACTIVISION), f300 = 25.70% / 8773 (Neversoft), menu 99.44%. **Windowed, same build: 0.00% / 1 colour to f2400, `vram_writes=0` over 4027 presents** — the guest never advances. Root cause is NOT in the FMV path (C021): `gpu_vk.cpp:498` never calls `SDL_SetGPUSwapchainParameters`, so the swapchain defaults to VSYNC and `SDL_WaitAndAcquireGPUSwapchainTexture` (`gpu_vk.cpp:1007`) blocks the **guest** thread — `game/core/sync_native.cpp:202` `vblank_advance() -> gpu_present()`. Proven by a zero-code control: windowed + `MESA_VK_WSI_PRESENT_MODE=immediate` -> `vram_writes=11076` and the logos appear. Framework fix IN FLIGHT 2026-08-05. The three earlier fixes are real and unaffected: DICR gate (C017), the recompiler unwinding DecDCTvlc (RE-16), per-channel DMA completions. Still not measured: audio, and frame PACING against the movie's real duration. TTSLOGO is legitimately absent from the disc |
| DMA interrupt registers (DPCR/DICR) | framework `dma_irq.h`, `mem.cpp` | **done for DMA3** — the guest's per-channel IRQ enable is honoured, so a transfer it deliberately runs silent no longer signals its callback. Hermetic gate `tests/test_dma_irq_gate.cpp` (7 cases / 30 checks). DPCR is storage only, deliberately: two of the three consuming ports never write it, so gating transfers on it would read their untouched 0 as "all channels disabled" |
| The framework itself | `external/psxport/` | **not this repo's subsystem** — a submodule with its own history and its own codemap. Changes to it are made here but land in that repo; consumers pin their own commit |
| The recompiled substrate | `generated/` | **not a subsystem** — regenerated output of `tools/ensure_recomp.py`, never committed and never hand-edited. A mistranslation is fixed in the recompiler, not in its output |

## Where is X

- **The RE'd guest addresses** → `game/core/game_config.cpp`, each cited with its instruction.
- **Why a value is zero** → `docs/re-frontier.md`, by step. Query it with
  `RE_FRONTIER_ROADMAP=docs/re-frontier.md python3 tools/re_frontier.py next` (run from the repo
  root) — **the env var is required**, or the tool silently validates nothing (INST-14). Do not use
  a `$CLAUDE_SKILLS`-relative path: that variable is unset in a plain shell, so the command
  collapses to `/re-frontier/re_frontier.py` and fails.
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
  - `PSXPORT_PRESENT_SHOT_AT=<present>,...` → `scratch/screenshots/present_<n>.ppm`. **The presented
    picture**: after letterbox, fade, native-vs-ires selection and 24bpp decode — what a player
    SEES. Works in both legs (the headless sink defaults to the window's 960x720; override with
    `PSXPORT_PRESENT_SINK=WxH`). Still blind to the swapchain hop itself — INST-20 states where.

  Prefer either over primitive counters: counters say the pipeline moved data, only an image says
  what a player sees.
- **Driving the game without a controller** → `PSXPORT_FORCE_BUTTONS=<hex active-low mask>`
  (`0040` DOWN, `4000` CROSS, `0008` START).

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

**WHAT IT DRAWS THERE IS WRONG, and it is ROOT-CAUSED (issue 0007): the palettes are empty, not the
textures.** Every CLUT the 3D scene uses is the constant `0x3333` = RGB(152,200,96), pale
yellow-green; all 1765 textured prims in a 6-frame window are CLUT-indexed (0 direct-colour) and all
carry `raw=0` (modulate), so every textured pixel is `vertex_colour x pale-green` — a smooth gouraud
gradient with no detail, 369 distinct colours in the frame. The 358 untextured prims are unaffected,
which is why a blue prop and the brown HUD box keep correct colour. Texture PAGES are rich and
correctly addressed (up to 11027 distinct words in one prim's uv box); the texture window is
irrelevant (913 prims carry a zero window and are equally flat); and an independent decode sharing
no code with the shader reproduces the same result, so the sampling arithmetic is faithful.
**The fault is UPSTREAM OF THE GPU — an asset/palette LOAD problem. Nothing in `gpu_native.cpp`,
`gpu_vk.cpp` or the shaders needs to change.** The guest re-uploads the whole CLUT strip every frame
from guest RAM that is already `0x33`-filled. Lead, not yet chased: one CLUT upload descriptor reads
`src=0x801FFD20` for 34816 bytes, ending past the 2 MB RAM end. The city-skyline screen (present
4500) additionally shows heavy blocky corruption — separate symptom, also in 0007.
