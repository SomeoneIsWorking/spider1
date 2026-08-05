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

### Note (2026-08-05)
NARROWED 2026-08-05, and one reading taken during the follow-up was WRONG — recorded because the way
it was wrong is a reusable trap.

THE FALSE NEGATIVE FIRST. PSXPORT_GPU_TRACE prints `batch tri= tex= semi=` and it read 0/0/0 at
EVERY sampled present in the 3D scene. Read naively that says "no primitive reaches the native
rasterizer" — dramatic, and wrong. `batch` is the LIVE accumulator (s_tri_n/s_tex_n/s_semi_n)
sampled at the TOP of GpuVkState::present(): AFTER the previous frame_end reset it (gpu_vk.cpp:1569)
and BEFORE this frame's drawing. On a game that presents at the top of its frame loop it is
LEGITIMATELY 0 every time. The trace line now also prints `drawn tri= tex= semi=` (s_dbg_*_c — what
render_geom actually rasterised, the counters gpu_vk_stats and the debug server already report), so
that sample point can no longer produce the lie.

WHAT THE CORRECTED INSTRUMENT SAYS: textured primitives DO reach the native rasterizer and ARE drawn.
Sampled present 11600:

    batch tri=0 tex=969 semi=204 | drawn tri=0 tex=969 semi=204

969 textured + 204 semi-transparent, and tri=0 — essentially EVERYTHING the native raster draws in
this scene is submitted as TEXTURED. Corroborated at the GP0 level: PSXPORT_DEBUG=gpu reports 924
prims / 10114 gp0words per drawing frame (~11 words/prim = textured shaded polys), alternating with
0-prim frames (the game draws at 30 Hz).

REFUTED, so nobody re-derives them:
  * "the guest never issues textured primitives" — it issues ~924/frame and they are textured.
  * "the primitives never reach the native rasterizer" — 969 tex + 204 semi drawn in one present.
  * "the native raster is not running in the 3D scene" — it is.

WHICH LEAVES, still not distinguished: texture SAMPLING produces no detail. Narrowed to texture-page
/ CLUT content or addressing — the atlas not uploaded for this scene, texture page / CLUT
coordinates resolving wrong, or the texture window applied wrong. 369 distinct colours over a
full-screen frame that submits 969 textured prims is consistent with sampling a flat or empty region.

CAVEAT ON THE DENOMINATOR, stated rather than glossed: GPU_TRACE samples every 200th present, and
only one of six sampled presents in the tail showed non-zero draw counts. 200 is even, so the sample
lands on a fixed parity against a 30 Hz draw cadence — the sampling is biased and the true
per-present distribution is NOT established. Do not read "1 of 6" as a duty cycle.
