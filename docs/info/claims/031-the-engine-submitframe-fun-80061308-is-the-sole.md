---
id: C031
kind: claim
status: holds
created: 2026-08-06
tags: render,re-20,break-first,drawotag,seam
depends: game/render/render_seam.cpp
---

## Claim

The engine submitFrame FUN_80061308 IS the sole source of this port's presented picture: suppressing it FREEZES the picture completely, measured break-first

## Evidence

MEASURED 2026-08-06 with a THROWAWAY build (the shipped tree has no such switch): the render seam's native leg made to return without super-calling and without aborting, so the guest submit never runs and the run continues. Headless, PSXPORT_NOPACE=1, PSXPORT_FORCE_BUTTONS=4000, PSXPORT_PRESENT_BURST=1500:20 -> scratch/re20/shots_break, log scratch/re20/logs/break_first.log. RESULT: 1 distinct picture across 20 presents, 0 of 19 consecutive pairs differ, 0 differing pixels out of 13,132,800 pixel comparisons; the frozen image is the last FMV/boot frame (37.67 percent non-black, 5444 colours) because nothing new is ever drawn. presentskip corroborates: rebuild_geom FROZE at 11 and vram_writes at 4090. NEGATIVE CONTROL, same instrument, same mode, same capture window, the SHIPPED build on the reference leg (scratch/re20/logs/ref_burst.log, scratch/re20/shots_ref): 3 distinct pictures across 20 presents, 2 of 19 pairs differ, max 226377 differing px = 32.75 percent of the frame; rebuild_geom reached 202 by present 1125. The tool itself was validated the same session: tools/present_flicker.py --selftest 'PASSED - it can print both answers'. METHOD CORRECTION: re-frontier RE-20 prescribed DISTINCT COLOUR COUNT for this test and that metric gives the WRONG SIGN here - the suppressed leg reads 5444 colours against the reference leg's 628, because the frozen FMV photo is more colourful than the live flat-shaded 3D scene. Use FRAME IDENTITY / consecutive per-pixel diff.

## What would falsify it

a run in which the guest submit is suppressed and the presented picture still CHANGES between consecutive presents (nonzero per-pixel diff over a window with the submit provably not running), or the discovery of a second game-side DrawOTag caller
