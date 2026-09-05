---
id: C019
kind: claim
status: falsified
created: 2026-08-05
tags: fmv,mdec,re-07,re-16,dma
falsified_on: 2026-08-05
---

## Claim

The intro logo movies PLAY: both ATVILOGO.STR and LOGO.STR decode and reach the screen, and the menu is unaffected

## Evidence

180 s headless run on the real disc (scratch/logs/re07b_gate.log), PSXPORT_SHOT_AT=120,300,2400,4000,6000,8000, PPM histogram: f120 = 320x240 24-bit @0,256, 99.95% non-black / 11395 colours — the ACTIVISION logo, read off scratch/screenshots/re07_fmv_120.png; f300 = 25.70% / 8773 — the Neversoft eyeball (re07_fmv_300.png); menu f4000/6000/8000 = 99.44% / 1061-1105 (re07_menu_4000.png, all eight items legible). 0 abort, 0 fatal, 0 recomp-MISS; the one error line is the expected CdSearchFile miss on TTSLOGO.STR, which is not on the retail disc. Instrument shows both answers: the same histogram read 0.00% / 1 colour on every intro shot before the fixes. Counters: DecDCTin 0x80085B24 2 -> 421 calls; MDEC-out callback 0x8002B28C 0 -> 8420; 0x8002A338 4 calls / 8 ABI violations -> 423 / 0. Two framework fixes, each with a hermetic RED-first gate in the retired static implementation: internal-label demotion plus intra-function bal/jr-ra (shown red at v0=20 of 30) and per-channel DMA-completion dispatch (shown red 9/12)

## What would falsify it

if a longer run or a windowed run shows the movies stalling, tearing, or the menu regressing — the evidence covers 180 s headless with shots at six frames, and it says nothing about audio (PSXPORT_NOAUDIO=1 throughout) or about frame PACING, which was never measured against the movie's real duration

## FALSIFIED 2026-08-05

AS WRITTEN, FALSIFIED — it claims the movies 'reach the screen', and they do not reach the SCREEN. Its own falsifier named this exact observation ('if ... a windowed run shows the movies stalling'). Measured 2026-08-05 on the same build (spider1 3381fcc / psxport 3f6a1e14): WINDOWED is 0.00% non-black / 1 colour at every present index out to f2400, and PSXPORT_DEBUG=presentskip reports presents=4027 reuse_last=4027 rebuild_geom=0 rebuild_vram=0 vram_writes=0 — the guest wrote NOTHING to VRAM over 4027 presents. The headless evidence in this entry is NOT withdrawn: it was correct about headless, and it is re-issued scoped as C020. The reason the two disagree is C021 (the swapchain present mode defaults to VSYNC and SDL_WaitAndAcquireGPUSwapchainTexture blocks the GUEST thread), proven by a zero-code-change control: windowed + MESA_VK_WSI_PRESENT_MODE=immediate gives vram_writes=11076 and the movie appears (f120 17.97% / 2197, f200 50.76% / 6279, f300 36.35% / 6035). Two instruments certified the false negative: PSXPORT_SHOT_AT reads back guest VRAM and never samples the swapchain (INST-18), and the watchdog is petted from gpu_present_ex so it cannot see guest starvation (INST-19). See issue 0005.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
