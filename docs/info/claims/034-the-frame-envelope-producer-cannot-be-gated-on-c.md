---
id: C034
kind: claim
status: holds
created: 2026-08-06
tags: render,producer,gate,negative-result
depends: game/render/frame_envelope.cpp, game/render/render_seam.cpp
---

## Claim

The frame-envelope producer CANNOT be gated on changed pixels in its own scene window — measured 0 of 524288, because both framebuffer pages are entirely black there

## Evidence

MEASURED 2026-08-06. A/B over the SAME source bar one line (SPIDERMAN_FRAME_ENVELOPE_PRODUCER 1 vs 0 in game/render/frame_envelope.cpp), native leg (PSXPORT_RENDER_PSX=0), headless, PSXPORT_NOPACE=1. Capture: the whole 1024x512 CPU VRAM dumped at the abort, which is the LAST instant of the producer's active window (scratch/prod1/gate/enabled.ppm, disabled.ppm; comparator scratch/prod1/ppmdiff.py, which refuses on a missing or differently-sized file rather than returning 0). THE PRODUCER DEMONSTRABLY RAN, in band and distinguishably: enabled logs 'frame envelope produced=2 clears=2', disabled logs 'SUPPRESSED build ... clears=0' and 'produced=2 clears=0'. RESULT: 0 of 524288 pixels differ. WHY, measured rather than guessed: at the abort BOTH framebuffer pages are entirely black in CPU VRAM (x0-511 y0-239 = 0 non-black, x0-511 y256-495 = 0 non-black) and all 115001 non-black halfwords lie in the texture area x>=512. So the clear had nothing to clear and the GP1(05) page flip moved between two identical black pages. The GP1(08) display mode is also NOT a discriminator here: the boot FMV path programs it directly, so both legs log the same '[gpu] display depth -> 15-bit (GP1(08)=08000002, 512x240)'. This is a property of the ONLY scene this producer fully owns — '....' is a 2-frame boot window — not evidence that the producer is inert.

## What would falsify it

the first display-list producer landing for 'dem1': the native leg then survives into frames with real content, the clear has something to remove, and this A/B must be re-run there and must show a non-zero delta. If it still shows zero THEN, the producer really is inert and C033's word-equivalence is measuring something that never reaches the screen.
