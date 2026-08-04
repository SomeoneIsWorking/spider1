---
id: C015
kind: claim
status: holds
created: 2026-08-04
tags: gpu,gp1,black-screen
---

## Claim

The 512x2 display area during boot is the GUEST blanking the screen while it loads, not a port decode bug — and it hides nothing

## Evidence

PSXPORT_DEBUG=gp1: the guest writes GP1(07)=040900 (y0=256,y1=258) itself and writes no further GP1(07) for the phase; GP1(07)=040010 in the same run decodes correctly to 240 lines. PSXPORT_VRAMDUMP=400 shows VRAM 5.1% non-zero with BOTH framebuffer regions (x0..512 at y0 and y256) entirely zero; the only content is texture/CLUT at x512..624 and x768..808. Over the 550 presents of that phase, 0 prims on every one.

## What would falsify it

if a run is found where the 512x2 phase coincides with non-zero framebuffer VRAM, the window IS hiding a picture and the decode must be re-examined
