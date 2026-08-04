---
id: 2
title: Boot is black for the first ~1200 frames (was "every presented frame is pure black"; that is no longer true)
status: investigating
symptom: black screen at boot, no Activision logo visible, display area collapses to 512x2 for hundreds of frames
tags: render,gpu,fmv,black-screen
created: 2026-08-04
updated: 2026-08-04
---

## CORRECTION — the original measurement is OBSOLETE

This issue was written against the build that aborted on `recomp-MISS 0x800C6684`. On
5cd7b43 / psxport 86dad4f9 the port **renders correctly**. Do not work from the original text.

Re-measured, headless, `PSXPORT_SHOT_AT=30,120,300,600,1200,2400,4000`, PPM histogram:

    frame   30   320x240 @0,256    0.00% non-black,    1 colour
    frame  120   320x240 @0,256    0.00%,              1
    frame  300   512x2   @0,0      0.00%,              1
    frame  400   512x2   @0,0      0.00%,              1
    frame  600   512x2   @0,0      0.00%,              1
    frame 1200   512x240 @0,0      1.42%,            250
    frame 2400   512x240 @0,0     99.44%,           1048   <- MAIN MENU, correct
    frame 4000   512x240 @0,0     99.44%,           1060

Frame 2400 is the front-end menu drawn properly (CONTINUE / NEW GAME / MEMORY CARD / OPTIONS,
TRAINING / RECORDS / SPECIAL / GALLERY, Spider-Man in a lit ring, SELECT prompt). A 20337-frame
run keeps rendering (859 prims/frame at f10657). **The renderer is not the problem.**

## The black window, with its phases (measured, `PSXPORT_DEBUG=gpu`, 7436 parsed frame lines)

| frames    | display        | what the guest is doing |
|-----------|----------------|-------------------------|
| 0..~173   | 320x240 @0,256 | boot; 0 prims, ~0 gp0 words. The two intro-FMV attempts happen at f3 and f5 and produce nothing (issue #4). |
| ~174..744 | **512x2**      | front-end asset load. Three VRAM upload bursts (f195/f496/f740, 202521 gp0 words and 2304 anon DMA writes in total across 550 presents), no geometry at all. |
| 745..~1500| 512x240        | menu builds/fades in: 59-60 prims per drawn field. |
| ~1500+    | 512x240        | menu fully visible. |

## The 512x2 display area is a SYMPTOM, not the cause — settled

`PSXPORT_DEBUG=gp1` shows the guest itself writing `GP1(07)=040900` (y0=256, y1=258 -> 2
scanlines) at the frame the phase starts, and then writing no further GP1(07) for the whole
phase. Our decode of that register is correct (`GP1(07)=040010` -> 240 lines is decoded right in
the same run).

More decisively: `PSXPORT_VRAMDUMP=400:...` at frame 400 shows VRAM **5.1% non-zero, with both
framebuffer regions (x 0..512, y 0 and y 256) completely empty**; the only content is
texture/CLUT data at x 512..624 and x 768..808. So the degenerate display window is not hiding a
picture — during that phase the guest has genuinely drawn nothing. Blanking the display while
loading is what the guest chose to do, and reproducing it is faithful.

Denominator for the negative: 550 consecutive presents with a 512x2 area, 0 prims on every one
of them, and 0 non-zero pixels anywhere in either framebuffer at the one frame sampled.

## What is left

The real user-visible defect in this window is the **missing Activision / Whoopee logo movies** —
tracked as issue #4, root-caused to the libstr sector-ring consumer index. Everything else in the
window is loading time.
