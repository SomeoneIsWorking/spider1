---
id: C014
kind: claim
status: falsified
created: 2026-08-04
tags: render,black-screen
falsified_on: 2026-08-05
---

## Claim

The port's renderer is not the cause of the boot black screen: the front-end menu renders correctly and the game keeps rendering for tens of thousands of frames

## Evidence

5cd7b43/psxport 86dad4f9, headless, PSXPORT_SHOT_AT: shot_2400.ppm and shot_4000.ppm are 512x240 with 99.44% non-black and 1048/1060 distinct colours, and are a legible main menu. A 20337-frame run shows 859 prims at f10657. scratch/screenshots/shot_2400.png, scratch/logs/blackscreen.log

## What would falsify it

if a WINDOWED run (not measured here — every number is headless) shows a different picture, or if a later psxport bump regresses gpu_vk present

## FALSIFIED 2026-08-05

PARTLY, and by its OWN falsifier verbatim: 'if a WINDOWED run (not measured here — every number is headless) shows a different picture'. It does. MEASURED 2026-08-05 on spider1 3381fcc / psxport 3f6a1e14: windowed is 0.00% non-black / 1 colour to f2400 with vram_writes=0 over 4027 presents, against 99.44% / 1048 colours headless. WHAT SURVIVES: the guest's GEOMETRY and rasterisation path is still not the cause — that part was correctly measured and is re-established by the control in C021, where windowed + MESA_VK_WSI_PRESENT_MODE=immediate (zero code changed) renders the movie. WHAT IS FALSE: the headline 'the renderer is not the cause of the boot black screen'. The renderer IS the cause, in the one part this claim never looked at — the SWAPCHAIN. gpu_vk.cpp:498 never calls SDL_SetGPUSwapchainParameters, so the default VSYNC present mode blocks the guest thread (C021). The claim's own instrument could not have found this: PSXPORT_SHOT_AT reads back guest VRAM and never samples the swapchain (INST-18). Superseded by C020 (headless decode, scoped) and C021 (the windowed root cause). See issue 0005

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
