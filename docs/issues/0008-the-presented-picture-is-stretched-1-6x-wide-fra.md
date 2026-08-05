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

### Note (2026-08-05)
CORRECTED 2026-08-05 by the USER: "Tomba! 2 has proper widescreen so it's not stretched." Right, and
my "correct by accident" framing was wrong and unfair to that port's widescreen work. The formula is
correct for Tomba2 in BOTH its modes, for a real reason — and seeing why names the true defect.

THE ALGEBRA. The present letterboxes to disp_w : 240, and
    disp_w / 240  ==  (4/3) x (disp_w / 320)
so the expression has 320 HARDCODED as the game's native 4:3 width. Therefore:
    Tomba2 native      disp_w=320, N=320  -> 4:3            CORRECT
    Tomba2 widescreen  disp_w=428, N=320  -> 1.783 ~ 16:9   CORRECT
    spider1 native     disp_w=512, N=512  -> 2.133          WRONG (should be 4:3)
    spyro   native     disp_w=512, N=512  -> 2.133          WRONG
Tomba2 is not accidentally right: for a 320-wide game the hardcoded constant IS its native width, so
both its modes land exactly where they should. The bug only appears when a game's native 4:3 width
is not 320 — i.e. it reads a HIGHER HORIZONTAL RESOLUTION as A WIDER PICTURE.

THE GENERAL RULE, which fixes all four rows above:
    present aspect = 4:3  x  (disp_w / N)      where N = the game's OWN native 4:3 width
N is available: gpu.s_disp_w is the game's native display width, and wide_native_w(game) already
scales the widescreen target from it.

AND THIS IS THE SAME BUG, IN A SECOND PLACE, THAT psxport ALREADY FIXED ONCE. gpu_vk.cpp's widescreen
accessor carries this comment verbatim:
    "THE WIDE WIDTH SCALES FROM THE GAME'S OWN 4:3 WIDTH, not from a hardcoded 320. The fixed returns
     here were 320-based, which silently assumed every PSX game renders a 320-wide framebuffer.
     Plenty do not: Spyro the Dragon runs 512x240, so asking for 16:9 returned 428 — NARROWER than
     the game's own 4:3 frame. Widescreen would have cropped the picture instead of widening it, and
     the failure would have looked like a broken renderer rather than a wrong constant."
That is this defect, diagnosed, with the correct principle stated — and the fix was applied to the
WIDE accessor only. The PRESENT letterbox kept the identical assumption, spelled 240 rather than 320,
so nobody recognised it as the same constant. The lesson generalises past this bug: when a wrong
assumption is fixed, GREP FOR ITS OTHER SPELLINGS. Here 320 and 240 are the same assumption wearing
different clothes.

REVISED TEST MATRIX for tests/test_present_plan.cpp (the RED is the two WRONG rows):
    N=320 disp_w=320 sink 960x720 -> 960x720 full
    N=320 disp_w=428 sink 960x720 -> 16:9 band
    N=512 disp_w=512 sink 960x720 -> 960x720 full   <- currently 960x450, RED
    N=512 disp_w=683 sink 960x720 -> 16:9 band      <- currently ~2.85:1, RED
This needs the game's native width plumbed into PresentInputs, which it currently is not — that
plumbing is the substance of the fix, not the arithmetic.
