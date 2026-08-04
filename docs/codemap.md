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
| Rendering (via the framework's VK path) | framework `gpu_native.cpp`, `gpu_vk.cpp` | **done for Phase 0 HEADLESS; the WINDOW is black** — the guest's own draw path fills guest VRAM and the menu reads 99.4% coverage there, but that number comes from `PSXPORT_SHOT_AT`, which reads back `s_vram_tex` and **never samples the swapchain** (INST-18). Windowed the same build reaches 0.00% / 1 colour with `vram_writes=0`; root cause is the swapchain present mode (C021, issue 0005), fix in flight |
| Native frame loop / OT / packet pool | — | **missing** — that `GameConfig` group is deliberately zero; Phase 0 runs the guest's loop instead (RE-12) |
| Scheduler | — | **missing** — the SDK task model may not apply to this engine (RE-13) |
| Renderer: GTE tap → native depth / widescreen | recompiler tap in `generated/` | **wired, never executed** — 10 `gte_record_pz` sites in 6 projection fns; `records=0` over a full boot because the port never reaches 3D (RE-08) |
| Audio | — | framework SPU is up; **unverified** — every run so far has used `PSXPORT_NOAUDIO=1`. Additionally, `SpuAudio::init` bails on `!gpu_windowed()` (`external/psxport/runtime/recomp/spu_audio.cpp:94`), so **a headless run can never measure audio** — and windowed runs currently starve the guest (C021, issue 0005). Both have to be cleared before any audio number means anything |
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
- **A picture of what the port actually draws** → `PSXPORT_SHOT_AT=<present>,...` writes
  `scratch/screenshots/shot_<n>.ppm`. Prefer this over primitive counters: counters say the pipeline
  moved data, only an image says what a player sees.
- **Driving the game without a controller** → `PSXPORT_FORCE_BUTTONS=<hex active-low mask>`
  (`0040` DOWN, `4000` CROSS, `0008` START).

## Current state in one line

The port provisions, recompiles, builds, boots, loads and rotates its 30 runtime code modules,
renders, and responds to input: **main menu → memory-card check → new game → name entry**, all drawn
correctly and driven by pad input. It stops on an unmapped read at `0x80800004` when advancing
further — one address past the mirrored 8 MB window, walking upward off the stack top (RE-16).
