---
id: C019
kind: claim
status: holds
created: 2026-08-05
tags: fmv,mdec,re-07,re-16,dma
---

## Claim

The intro logo movies PLAY: both ATVILOGO.STR and LOGO.STR decode and reach the screen, and the menu is unaffected

## Evidence

180 s headless run on the real disc (scratch/logs/re07b_gate.log), PSXPORT_SHOT_AT=120,300,2400,4000,6000,8000, PPM histogram: f120 = 320x240 24-bit @0,256, 99.95% non-black / 11395 colours — the ACTIVISION logo, read off scratch/screenshots/re07_fmv_120.png; f300 = 25.70% / 8773 — the Neversoft eyeball (re07_fmv_300.png); menu f4000/6000/8000 = 99.44% / 1061-1105 (re07_menu_4000.png, all eight items legible). 0 abort, 0 fatal, 0 recomp-MISS; the one error line is the expected CdSearchFile miss on TTSLOGO.STR, which is not on the retail disc. Instrument shows both answers: the same histogram read 0.00% / 1 colour on every intro shot before the fixes. Counters: DecDCTin 0x80085B24 2 -> 421 calls; MDEC-out callback 0x8002B28C 0 -> 8420; 0x8002A338 4 calls / 8 ABI violations -> 423 / 0. Two framework fixes, each with a hermetic RED-first gate: emit.py demote_internal_labels wiring + intra-function bal/jr-ra (test_emit.py, shown red at v0=20 of 30) and per-channel DMA-completion dispatch (test_dma_irq_gate.cpp, shown red 9/12)

## What would falsify it

if a longer run or a windowed run shows the movies stalling, tearing, or the menu regressing — the evidence covers 180 s headless with shots at six frames, and it says nothing about audio (PSXPORT_NOAUDIO=1 throughout) or about frame PACING, which was never measured against the movie's real duration
