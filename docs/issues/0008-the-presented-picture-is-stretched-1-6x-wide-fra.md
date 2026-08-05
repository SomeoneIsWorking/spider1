---
id: 8
title: the presented picture is stretched 1.6x wide — framebuffer aspect is used as display aspect
status: open
symptom: everything on screen is too wide and too short; a 512x240 game presents as a 2.133:1 letterboxed strip with big top/bottom bars instead of filling a 4:3 window
tags: render,present,aspect,letterbox,framework,all-ports
created: 2026-08-05
updated: 2026-08-05
---

FOUND 2026-08-05 by the USER, looking at a screenshot I sent: "are these stretched wide?". Yes.
I had looked at several of these frames and not seen it.

MEASURED on the presented picture (scratch/c0work/shots/after_present_10000.ppm, a 960x720 sink):
    picture band: x 0..959 (960 wide), y 135..584 (450 tall)
    presented aspect  = 2.133:1
    VRAM 512x240      = 2.133:1   <- what we are showing
    correct PSX 4:3   = 1.333:1
    horizontal stretch vs 4:3 = 1.600
Display mode confirmed from the run log: GP1(08)=08000002, 512x240, 15-bit.

ROOT CAUSE — the present plan uses the FRAMEBUFFER's dimensions as the DISPLAY ASPECT:
    runtime/recomp/present_plan.h:
      p.viewport = pane_letterbox(in.disp_w, PRESENT_DISPLAY_ASPECT_H /*240*/, in.sink_w, in.sink_h);
That is a faithful port of the older gpu_vk.cpp `letterbox(disp_w, 240, sw, sh)`, so the defect
predates the present-image-sink refactor — but I reproduced it unquestioned and wrote a comment
explaining why it was right, which made it harder to see, not easier.

WHY IT IS WRONG: on PSX the horizontal framebuffer width (256/320/368/512/640) selects the
horizontal SAMPLING RATE, not the shape of the picture. Every one of those modes scans into the same
4:3 screen area, so pixels are NON-SQUARE — in 512-wide mode they are 0.625:1. Presenting a 512x240
frame at its literal 512:240 pixel aspect therefore stretches the image 1.6x horizontally (equally:
squashes it to 62.5% of its correct height) and adds top/bottom bars that should not exist. A
correct 4:3 present of this frame fills the whole 960x720 sink.

WHY NOBODY CAUGHT IT: Tomba2Engine — the reference consumer, and the port with the most eyes on its
picture — runs 320x240, and 320:240 IS 4:3. It is the single mode where the wrong formula returns
the right answer. spider1 and spyro both run 512x240 and are both affected.

THE FIX IS NOT JUST "use 4:3", because one caller genuinely wants a wider picture:
gpu_vk_wide_engine() widens disp_w deliberately so the engine renders a wider FOV for the widescreen
mod. There the wide present IS intended. So the plan must distinguish:
  * NATIVE presentation -> target aspect is 4:3 REGARDLESS of framebuffer width;
  * WIDESCREEN MOD active -> target aspect is the mod's chosen aspect (16:9 etc).
Conflating "the framebuffer got wider" with "the picture should be wider" is the actual bug, and it
is exactly the same conflation in both cases.

TEST TO WRITE FIRST (tests/test_present_plan.cpp already exists and is the right home): a 512x240
native frame in a 960x720 sink must present as 960x720 (full, no bars), not 960x450; a 320x240 frame
must still present as 960x720; and a widescreen-mod frame must still present wide. The current code
fails the first case, which is the RED.

BLAST RADIUS: all three ports, every windowed and headless present, and EVERY present-stage capture
taken so far — including the ones in INST-20 and in issue 0007's evidence. Those numbers are not
invalidated (coverage percentages are unaffected by a uniform rescale of where the picture sits), but
any judgement about SHAPE made from them was made on a stretched image.
