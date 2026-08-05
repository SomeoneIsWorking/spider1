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

### Note (2026-08-05)
FORK SPLIT 2026-08-05: the texture atlas IS uploaded and IS full of real texture data. So "the atlas
was never uploaded for this scene" is REFUTED, and the remaining cause is ADDRESSING or SAMPLING.

MEASURED. Full CPU-side VRAM dumped during the 3D scene and tiled 64x64, scored by distinct 16-bit
values (tools/vram_tiles.py, written for this so the analysis stops being re-derived by hand — issue
0006 did the same thing manually):

    PSXPORT_NOWINDOW=1 PSXPORT_NOAUDIO=1 PSXPORT_FORCE_BUTTONS=4000 \
    PSXPORT_VRAMDUMP="11900:scratch/raw/vram3d.bin" PSXPORT_WATCHDOG=120 timeout 220 ./run.sh
    python3 tools/vram_tiles.py scratch/raw/vram3d.bin

    rich tiles (>16 distinct): 60 of 128 scanned
      x<512  (display half): 0
      x>=512 (atlas half)  : 60      <- 700..2786 distinct values per tile

WHY THAT SETTLES IT: GpuVkState::present() is handed the CPU-side s_vram as `src` and uploads it as
BOTH the render target seed and the texture-source snapshot (upload_vram -> s_vram_tex + s_vram_snap,
which is what render_geom binds as the sampler). So the very buffer whose atlas half is provably rich
IS the texture source the native raster samples from. The data is there and the sampler can reach it.

Combined with the previous note (969 textured prims per frame ARE submitted and rasterised), the
cause is now bounded to: the texture PAGE / CLUT coordinates the prims carry, the texture window, or
the sampling arithmetic in the shader — i.e. the prims are addressing the atlas wrongly, not missing
it. Still not distinguished between those, and still not to be closed by picking one.

DENOMINATOR AND BLIND SPOT, stated: the display half of that dump reads BLANK (1-2 distinct values
per tile), and that is EXPECTED, not a finding — with the VK backend on, the composite is built in
the GPU texture and never written back to CPU s_vram (issue 0006, INST-18). A CPU-side VRAM dump can
speak about the ATLAS half only. tools/vram_tiles.py prints that caveat itself so the next reader
cannot mistake the blank half for "the port renders nothing" — which is exactly the false conclusion
issue 0006 was opened to record.

NEXT MEASUREMENT (not done): compare the texture-page / CLUT coordinates the submitted prims actually
carry against where the rich atlas tiles physically are. PSXPORT_PRIMDUMP exists (gpu_native.cpp
prim_dump_close_if_done) and is the likely instrument. If the prims point outside the rich region, it
is addressing; if they point into it, it is the sampling/CLUT decode.

### Note (2026-08-05)
ROOT CAUSE FOUND 2026-08-05. The textures are fine. THE PALETTES ARE NOT: every CLUT the scene uses
is the constant 0x3333, and 0x3333 in PSX 1555 is RGB(152,200,96) — pale yellow-green. That single
value IS the symptom.

    python3 -c "v=0x3333; print((v&31)<<3, ((v>>5)&31)<<3, ((v>>10)&31)<<3)"   ->  152 200 96

THE CHAIN, each link measured:
 1. ALL 1765 textured prims in a 6-frame window are CLUT-INDEXED — 1230 4bpp, 535 8bpp, ZERO
    15bpp-direct. So every textured surface's colour comes through a palette lookup, with no
    direct-colour path to mask the fault.
 2. ALL 1765 point their CLUT into VRAM x=512..767, y=0..79.
 3. That rectangle is 100.0% the single value 0x3333. Independently confirmed on FOUR separate VRAM
    dumps (three by an agent, one by me: 20480/20480 halfwords, 1 distinct value). 0x3333 occupies
    21919 of 524288 VRAM words (4.2%) and is confined to exactly that rectangle.
 4. ALL 1765 prims carry raw=0, i.e. MODULATE. So the shader computes
    vertex_colour x RGB(152,200,96) for every textured pixel in the scene — a smooth gouraud
    gradient tinted pale green, with no texture detail. That is precisely the reported picture, and
    it is why the frame holds only 369 distinct colours.
 5. AND IT EXPLAINS THE ANOMALY THAT DID NOT FIT ANY EARLIER STORY: the blue prop and the brown HUD
    box keep correct colour because they are among the 358 UNTEXTURED prims in the same window.
    Untextured prims never touch a CLUT, so they are the only things on screen unaffected.

WHY THE ADDRESSING WAS NEVER THE PROBLEM (all three original candidates are now dead):
 (A) tpage/CLUT coords — 0 of 1765 prims have texpage x<512; their addressed texels are RICH (up to
     11027 distinct words in one prim's uv box; 1543/1765 address >16). The coordinates land on real
     texture data AND on the strip the guest itself re-uploads. They are correct.
 (B) texture window — 913 of 1765 prims carry a ZERO window (no mask, no offset) and are just as
     flat as the 852 that carry a real one. A pass-through window cannot cause a defect.
 (C) sampling/CLUT-decode arithmetic — an INDEPENDENT Python decode sharing no code with
     tritex.frag, fed the same prims and the same VRAM, reproduces the same impoverished result
     (30 distinct colours over 2.69M texels sampled). The port's arithmetic is faithfully decoding
     what its VRAM says. Separately, a static read found the 4bpp/8bpp unpack and the window formula
     character-identical to the nocash spec, and NO vertex-colour fallback: tritex.frag does
     `if (texel == 0u) discard;`.

DECISIVE POSITIVE CONTROL, which is what makes this a root cause and not a correlation: feeding the
SAME 597-prim set through the SAME decode against a VRAM dump whose CLUT strip holds REAL palettes
yields median 6 / max 11 distinct texels per prim and only 14.6% flat, versus 597/597 flat on the
defect dump. Same coordinates, same code, different palette data, different answer.

WHO WRITES 0x3333 — THE GUEST DOES, from guest RAM that is already 0x33-filled.
PSXPORT_TEXWATCH="512,0,768,80" (18923 hits in an agent's run, 29419 in mine) shows the guest
re-uploading the whole CLUT strip every drawing frame via GP0(0xA0), and the SOURCE BYTES are
already wrong in RAM:
    [texwatch] f133 A0 dest=(640,0) 16x1 src=0x801461A4 srcbytes: 33 44 44 44 44 33 33 33 00 33 33 33
    [texwatch] f15427 A0 dest=(512,0) 256x12 src=0x801E4060 srcbytes: 33 33 33 33 33 33 33 33 ...
No FILL and no 0x80 VRAM->VRAM copy touches the strip — and that negative is now worth something,
because TEXWATCH previously watched only A0 and 80copy and was BLIND to both GP0(0x02) fills and the
native upload path. Those two coverage holes were closed before this negative was taken; without
that, "nothing else wrote here" would have been a confident silent lie.

SO THE FAULT IS UPSTREAM OF THE GPU ENTIRELY — it is an asset/palette LOAD problem, not a renderer
problem. The renderer is faithfully drawing the data it was given. Nothing in gpu_native.cpp,
gpu_vk.cpp or the shaders needs to change for this.

STRONGEST LEAD FOR THAT UPSTREAM BUG, observed but NOT chased: one CLUT upload descriptor reads
src=0x801FFD20 for 256x68 halfwords = 34816 bytes, ending at 0x80208520 — PAST THE 2 MB RAM END
(core.h: uint8_t ram[0x200000], i.e. valid through 0x801FFFFF). That upload cannot be reading a
valid source from where it points. Whether the descriptor is wrong, or the palette DMA/decompression
that should have filled that buffer never ran, is the next question. Start there.

NOT FIXED, NOT CLOSED. This is a root-cause identification backed by measurement; the fix is
upstream and unwritten, and per PROTOCOL the user closes the bug, not our own numbers.
