---
id: C014
kind: claim
status: holds
created: 2026-08-04
tags: render,black-screen
---

## Claim

The port's renderer is not the cause of the boot black screen: the front-end menu renders correctly and the game keeps rendering for tens of thousands of frames

## Evidence

5cd7b43/psxport 86dad4f9, headless, PSXPORT_SHOT_AT: shot_2400.ppm and shot_4000.ppm are 512x240 with 99.44% non-black and 1048/1060 distinct colours, and are a legible main menu. A 20337-frame run shows 859 prims at f10657. scratch/screenshots/shot_2400.png, scratch/logs/blackscreen.log

## What would falsify it

if a WINDOWED run (not measured here — every number is headless) shows a different picture, or if a later psxport bump regresses gpu_vk present
