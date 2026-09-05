---
id: C020
kind: claim
status: holds
created: 2026-08-05
tags: fmv,mdec,re-07,headless,scoped
depends: external/psxport/runtime/psx/dma_irq.h, external/psxport/runtime/psx/mem.cpp, external/psxport/runtime/psx/gpu_vk.cpp, game/core/cd_stream.cpp
---

## Claim

HEADLESS ONLY: under PSXPORT_VK_HEADLESS=1 both intro logo movies decode and land in guest VRAM, and the menu is unaffected. This says NOTHING about what a window shows

## Evidence

This is C019 re-issued with the scope its evidence actually supports; C019 as written claimed the movies 'reach the screen' and was falsified by a windowed run (issue 0005). MEASURED: 180 s headless run on the real disc (scratch/logs/re07b_gate.log), PSXPORT_SHOT_AT=120,300,2400,4000,6000,8000, PPM histogram: f120 = 320x240 24-bit @0,256, 99.95% non-black / 11395 colours (ACTIVISION logo, scratch/screenshots/re07_fmv_120.png); f300 = 25.70% / 8773 (Neversoft eyeball); menu f4000/6000/8000 = 99.44% / 1061-1105. 0 abort, 0 fatal, 0 recomp-MISS. Negative control present: the same histogram read 0.00% / 1 colour on every intro shot before the DICR/RE-16/per-channel-DMA fixes. Counters DecDCTin 0x80085B24 2 -> 421; MDEC-out callback 0x8002B28C 0 -> 8420; 0x8002A338 4 calls / 8 ABI violations -> 423 / 0. CORROBORATED WINDOWED, which is what makes the decode claim independent of the sink: windowed + MESA_VK_WSI_PRESENT_MODE=immediate (zero code changed) gives f120 17.97% / 2197, f200 50.76% / 6279, f300 36.35% / 6035 (scratch/logs/win_immediate.log). SCOPE, stated because the instrument forces it: PSXPORT_SHOT_AT reads back the guest VRAM texture s_vram_tex and never samples the swapchain (INST-18), so this claim is about VRAM CONTENT, not about pixels on a display

## What would falsify it

if a headless run shows the movies stalling or the menu regressing; or if a swapchain-sampling shot instrument is built and disagrees with the VRAM readback on the SAME frame — the current instrument cannot tell 'the guest drew nothing' from 'the window shows nothing', so this claim can only ever be about VRAM. It says nothing about audio (PSXPORT_NOAUDIO=1 throughout, and audio is additionally gated on !gpu_windowed() at spu_audio.cpp:94 so a headless run CANNOT measure it) and nothing about frame PACING against the movie's real duration
