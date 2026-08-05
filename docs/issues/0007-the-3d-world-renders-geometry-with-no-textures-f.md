---
id: 7
title: the 3D world renders geometry with no textures — flat/gouraud only
status: open
symptom: gameplay 3D scene is monochrome green: character, floor, walls all flat-shaded with no texture detail; only 369 distinct colours in a full-screen 3D frame
tags: render,3d,texture,gameplay,unverified-cause
created: 2026-08-05
updated: 2026-08-05
---

FOUND 2026-08-05 by driving the port past the front-end for the first time and LOOKING at a picture.
Nobody had done that before: until this session no instrument in the project could show the presented
picture (INST-18/INST-20), and the codemap asserted the port stopped at name entry, so nothing beyond
the menu had ever been captured.

HOW TO REPRODUCE (headless, ~200 s, no window needed):

    PSXPORT_NOWINDOW=1 PSXPORT_NOAUDIO=1 PSXPORT_FORCE_BUTTONS=4000 \
    PSXPORT_SHOT_AT=10000 PSXPORT_PRESENT_SHOT_AT=10000 \
    PSXPORT_WATCHDOG=120 timeout 200 ./run.sh
    python3 ../spyro/tools/ppm_look.py scratch/screenshots/{shot,present}_10000.ppm

PSXPORT_FORCE_BUTTONS pulses CROSS (8 frames on / 24 off), which walks the front-end deterministically.

WHAT IS ON SCREEN at present 10000: a third-person 3D scene — Spider-Man in an interior, floor,
walls, ceiling fixtures, a HUD box bottom-right with an orange arrow. The GEOMETRY and the CAMERA
look right. The SHADING does not: every surface is a smooth gradient with no texture detail
anywhere, and the whole scene is mapped into green/yellow. The character is uniformly pale green
(he should be red and blue).

THE NUMBER THAT MATTERS: 369 distinct colours in a full-screen 3D frame. A textured PSX scene has
thousands (this port's own main menu at present 3900 has 1048, and its intro FMV frames 8825-11395).
369 is what pure flat/gouraud shading with NO texture sampling looks like.

IT IS NOT A GLOBAL FILTER, so 'everything is tinted green' is the wrong description: a blue object
on the right stays blue and the HUD box stays brown with an orange arrow. Some primitives carry
correct colour; the world surfaces and the character carry none.

RULED OUT — the present stage / the new present-image-sink composite (so NOT the fade, the letterbox
or the 24bpp decode). The guest-VRAM shot at the same present is THE SAME PICTURE: shot_10000
512x240 98.7% non-black / 369 colours vs present_10000 960x720 61.7% / 369 colours, the coverage gap
being exactly the widescreen letterbox (512:240 fills 450 of 720 rows = 62.5%; 98.7 x 0.625 = 61.7).
Identical colour count means the composite added and removed nothing. The defect is UPSTREAM of the
present stage, in whatever fills VRAM.

NOT YET INVESTIGATED, and deliberately not guessed at: whether this is missing texture-page uploads,
a CLUT/texture-window problem, the native raster ignoring texture bits, or the guest never issuing
textured primitives at all. RE-08 (Render: GTE tap -> native depth) is open and the codemap notes
native depth coverage is only a few percent, so the textured/3D path is known to be immature — but
no measurement here says which of those it is. Do not close this by picking one.

SECOND, SEPARATE OBSERVATION from the same run, filed here only so it is not lost: present 4500 is
the New York skyline (a 2D backdrop) with heavy blocky corruption — torn horizontal bands of
displaced pixels across the buildings. 468 colours. Different symptom, probably a different cause;
worth its own entry once someone looks at it.

VERDICT IS THE USER'S. This is a measurement, not a closed diagnosis: the port reaches 3D gameplay
and draws recognisable geometry, and the surfaces are untextured. Whether that is the expected state
of this port today or a regression is not something our own numbers can settle.
