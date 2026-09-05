---
id: 8
title: the presented picture is stretched 1.6x wide — framebuffer aspect is used as display aspect
status: resolved
symptom: everything on screen is too wide and too short; a 512x240 game presents as a 2.133:1 letterboxed strip with big top/bottom bars instead of filling a 4:3 window
tags: render,present,aspect,letterbox,framework,all-ports
created: 2026-08-05
updated: 2026-08-06
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
    runtime/psx/present_plan.h:
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

### Note (2026-08-05)
BLAST RADIUS CONFIRMED FROM EACH PORT'S OWN DATA, and the bug has a visible signature nobody named.

spyro's own logs (grep over spyro/scratch/logs): it runs BOTH
    disp 320x240   and   display depth -> 15-bit (GP1(08)=08000002, 512x240)
So spyro is affected, confirmed from its data rather than inferred from the framework comment.

THE SIGNATURE, which is a testable prediction and not a story: because the formula is
(4/3) x (disp_w / 320), a game that SWITCHES horizontal mode changes PRESENTED ASPECT mid-run.
    320-wide mode -> pane_letterbox(320,240,960,720) -> 960x720, FILLS a 4:3 sink, no bars
    512-wide mode -> pane_letterbox(512,240,960,720) -> 960x450, 1.6x too wide, BIG TOP/BOTTOM BARS
Both spider1 and spyro switch between these modes (spider1: 24-bit 320x240 for FMV, 15-bit 512x240
for the game; spyro likewise). So on both ports the picture VISIBLY CHANGES SHAPE at every FMV ->
gameplay transition, and the black bars appear and disappear with it.

ALREADY CORROBORATED BY CAPTURES TAKEN EARLIER TODAY, before the bug was known: the Neversoft intro
logo (320x240 24-bit FMV) filled the whole 960x720 present shot edge to edge, while the pause screen
and menu (512x240) sit in a 960x450 band with heavy bars above and below. Those two screenshots are
the same bug photographed twice; nobody read the bars as a defect because a letterbox looks
deliberate.

WHY THAT MATTERS BEYOND THE ASPECT ITSELF: 'the picture has black bars' is the kind of thing an
observer explains away as intended letterboxing. It is the sort of defect that survives review by
looking like a feature — which is why it needed a USER to ask "are these stretched wide?" rather
than any of the numeric checks run against these frames all day. Coverage percentages, colour counts
and non-black ratios are ALL invariant under this bug; not one of the instruments used today could
have caught it, and none of them was wrong. The missing check was geometric, and there was no
instrument for shape at all.

### Note (2026-08-05)
INSTRUMENT BUILT, 2026-08-05: tools/present_geometry.py — the missing SHAPE check.

The reason this bug survived a full session of numeric checking is that every instrument in the repo
is INVARIANT UNDER IT. A uniform rescale changes no coverage percentage, no distinct-colour count,
no mean brightness, no per-tile richness. ppm_look.py reported "real frame, 62.0% non-black, 528
colours" on a frame stretched 1.6x, and every one of those numbers was correct. The gap was not a
weak instrument — it was a MISSING DIMENSION.

    python3 tools/present_geometry.py <present shot.ppm> [--expect 16:9] [--tol 0.02]

It finds the content band inside the sink, reports its aspect against the expected one, names the
stretch factor, and reports the bars explicitly (because "big black bars" is what this bug looks
like to a human, and a reader tends to assume bars are deliberate letterboxing).

VALIDATED IN ALL FOUR DIRECTIONS, which is more than "it printed the number I wanted":
  * the real defect frame          -> STRETCHED 1.600x WIDE, rc=1
  * a synthetic correct 4:3 frame  -> OK, rc=0, "bars none (fills the sink)"
  * a synthetic 16:9 band          -> STRETCHED 1.333x under the 4:3 default, OK under --expect 16:9
  * an entirely black frame        -> REFUSES with rc=2 and says so, because a 0x0 band would
                                      otherwise compare unequal to any expected aspect and print a
                                      confident "STRETCHED" for a frame with no picture in it.

ITS OWN LIMITS ARE IN ITS HEADER: it measures the non-black bounding box, not the intended picture
rectangle, so a fade or a genuinely dark scene can measure smaller than the real picture (it prints
a CAUTION when the band covers <5% of the sink); and it cannot tell a correctly-4:3 picture from a
square picture of a square thing.

A DEFECT IN THE TOOL, FOUND AND FIXED IN ITS FIRST MINUTE, recorded because it is the failure mode
this file exists for: the first argument parser filtered on a leading "--", which left the VALUE of
--expect ("16:9") in the positional list, and the tool tried to open it as a PPM. It crashed loudly,
which is the only reason it was caught immediately — an arg parser that mistakes a flag's value for
an input is otherwise exactly how a tool measures the wrong file and reports it with confidence.
Unknown options now refuse (rc=2) rather than being ignored.

### Note (2026-08-06)
FIX DESIGNED AND HERMETICALLY VERIFIED 2026-08-06 — developed in scratch/aspect/ because the shared
psxport tree was held by another agent's build. Ready to apply; artifacts staged at
coord/patches/present-aspect-0008.NEW.{present_plan.h,test_present_plan.cpp}.

THE CHANGE: PresentInputs gains `native_w` (the game's own 4:3 framebuffer width), and the letterbox
becomes
    pane_letterbox(4 * disp_w, 3 * native_w, sink_w, sink_h)
i.e. 4:3 scaled by how much wider than ITS OWN NATIVE WIDTH the framebuffer is. native_w <= 0
degrades to 4:3 (every PSX mode is 4:3, so that is the only safe default, and it cannot divide by
zero).

RED, seen and pasted, against the shipped rule with the new field present but unused:
    test native_width_sets_the_aspect_not_the_framebuffer_width
        FAIL: plan(b).viewport.h == 720: got 450 want 720      <- 512-wide native, the bug
    test unknown_native_width_degrades_to_4_3
        FAIL: p.viewport.h == 720: got 450 want 720
    9/11 tests passed, 157 checks run, 2 failed
GREEN after: 12/12, 1703 checks. The suite's built-in legacy control
(-DPSXPORT_TEST_LEGACY_PRESENT_PLAN) still fails 10/11, so the older invariants are still pinned.

**THE FIX IS A PROVABLE NO-OP FOR TOMBA2, AND THAT IS ASSERTED, NOT ARGUED.** The old rule was
disp_w : 240; the new one is 4*disp_w : 3*native_w; at native_w == 320 that is 4*disp_w : 960 ==
disp_w : 240 — algebraically identical. So a 320-wide game cannot change in ANY mode at ANY
framebuffer width. test_a_320_wide_game_is_bit_identical_to_the_old_rule compares the new viewport
against a literal transcription of the old rule for every disp_w in 256..640 (385 widths, x/y/w/h
each) and asserts the denominator. This matters because Tomba2Engine is the consumer I cannot test
from here, and "it should be identical" is exactly the sort of claim this project keeps getting
burned by.

The existing widescreen case is likewise untouched: a 320-native game widened to 512 still gives
1280x600 in a 1280x960 sink, before and after.

STILL TO DO when the tree frees: plumb native_w at the call site in gpu_vk.cpp's present_inputs()
(the game's native width is gpu.s_disp_w, the same source wide_native_w already scales from), then
re-run the real-data gate with tools/present_geometry.py as the acceptance check — a 512x240 frame
must report OK / "bars none (fills the sink)" instead of STRETCHED 1.600x.

### Note (2026-08-06)
FIXED AND VERIFIED ON REAL DATA 2026-08-06.

CALL SITE: present_inputs() now sets in.native_w from the DISPLAY REGISTER (g.game->gpu.s_disp_w via
present()'s pre-widening `w`), not from disp_w — disp_w has already been widened when the widescreen
mod is active, so passing it would have restored the stretch. Same source wide_native_w already
scales from, so both sides of the widescreen mechanism now agree on what "native" means.

GATE, on the real game, with the instrument built for exactly this (INST-25):
  BEFORE  content x 0..959 (960 wide), y 135..584 (450 tall), aspect 2.133:1
          verdict STRETCHED 1.600x WIDE, bars top/bottom 135+135 px
  AFTER   content x 0..959 (960 wide), y 0..719 (720 tall), aspect 1.333:1
          verdict OK, bars none (fills the sink)
Same instrument, same headless mode, both sides — and it produced the failing answer before.

Framework suite 17/17. test_present_plan 12/12, 1703 checks; its legacy negative control still fails
10/11, so the older invariants stay pinned rather than loosened to let the new rule through.

THE PICTURE, same present index, with the GP0(0xC0) readback fix also in the tree:
    369 colours, uniformly pale green, stretched  ->  1581 colours, 99.7% non-black, correct 4:3
Spider-Man renders in red and blue on a New York rooftop with a correct HUD. TWO INDEPENDENT DEFECTS
were stacked on the same frame — a missing framework feature destroying the palettes, and a wrong
aspect constant — and NEITHER WAS VISIBLE WHILE THE OTHER STOOD. The green hid the stretch (a flat
pale field has no shape to judge) and the stretch hid nothing about the green. That is worth
remembering: "fix the top defect and re-look" is the only way to find the second one.

TREE STATE, stated because it matters for landing: spider1/external/psxport now carries FOUR
uncommitted areas from this session — present-image-sink, gpu-diagnostics-0007, gpu-vram-readback,
and this. A plain git diff of that tree is all four together;
coord/patches/COMBINED-psxport-alltrees.diff is that combined diff (9 files, +683/-101) and is NOT
four applyable patches. The operator should split it or land it as one reviewed unit.

### Resolution (2026-08-06)
CLOSED 2026-08-06 — fixed, gated, and provably inert for the port that was already correct.

FIX: PresentInputs gains native_w (the game's own 4:3 width, from the display register), and the
letterbox becomes pane_letterbox(4*disp_w, 3*native_w, ...) — 4:3 scaled by how much wider than its
OWN native width the framebuffer is. native_w<=0 degrades to 4:3, which is both the right default
for every PSX mode and divide-by-zero-proof.

GATE (INST-25, same instrument and mode both sides, and it produced the failing answer before):
  BEFORE  y 135..584 (450 tall), 2.133:1, STRETCHED 1.600x WIDE, bars 135+135 px
  AFTER   y 0..719  (720 tall), 1.333:1, OK, bars none (fills the sink)
Framework suite 17/17. test_present_plan 12/12 / 1703 checks, legacy control still red 10/11.

TOMBA2 CANNOT MOVE, and that is asserted rather than argued: old rule disp_w:240; new rule
4*disp_w:3*native_w; at native_w==320 they are algebraically identical. The suite compares the new
viewport against a literal transcription of the old rule across all 385 widths in 256..640 (x/y/w/h
each, denominator asserted). That matters because Tomba2Engine is the consumer this repo cannot run.

WHY IT SURVIVED SO LONG, worth keeping: every other instrument here is INVARIANT under it. Coverage
%, colour counts, brightness, tile richness — a uniform rescale changes none of them, and none of
them was wrong. It took a human asking "are these stretched wide?". INST-25 (tools/present_geometry.py)
now exists so the shape question has an instrument at all.

NOT VERIFIED: spyro. It is 512x240 and therefore was affected identically, but I did not run it —
the fix is in the shared framework and its tree does not carry the patch yet.

### Note (2026-08-06)
CROSS-REFERENCED INTO spyro 2026-08-06. This entry's own closing line said "NOT VERIFIED: spyro", and spyro's catalog (46 entries at the time) had NO entry for this defect — a spyro session searching "letterbox"/"1.6x" got "(no matches)" and would have re-derived the whole thing. It is now spyro docs/issues/0047, status OPEN there, cross-referenced back to here as the canonical record.

CONFIRMED IN SPYRO'S OWN TREE, not inferred from this one: spyro's external/psxport is dbc5a5e1 (CLEAN) and still carries the pre-refactor form in its GPU presentation code: `letterbox(disp_w, 240, sw, sh)`. That tree has no `present_plan.h` and no PresentInputs.native_w — the fix does not exist there, so spyro is UNFIXED and will only get it via a framework pin bump.

ONE NEW PIECE OF EVIDENCE FOR THE "visibly changes shape at every FMV<->gameplay transition" prediction, found while mirroring: the SAME FILE already contains the correct form. gpu_vk_present_image() (the RGBA still-image path) uses `letterbox(4, 3, sw, sh)` at gpu_vk.cpp:1128, while show_composite() (the game picture) uses `letterbox(disp_w, 240, ...)` at :1047. The two present paths in one file disagree about what the display aspect is, which makes the shape change checkable from any two captures either side of such a transition rather than only from the display-mode log.

CAVEAT ON THE SPYRO NUMBER, recorded here because this entry's evidence is what a reader will reuse: no spyro capture has been through a shape check, and THIS REPO'S copy of present_geometry.py reports spyro's stretch as 1.714x where the real present stretch is 1.600x — it measures the non-black CONTENT BBOX, not the display rect. The discrepancy is exact and names the cause: 1.714/1.600 == 240/224, and spyro's guest draws 224 of its 240 display lines, so the band measurement charges GUEST-DRAWN black to the letterbox. A REPAIRED copy already exists at spyro/tools/present_geometry.py (spyro instrument I042, --selftest 16/16): it REFUSES with rc=3 on that ambiguity and resolves the same frame to 1.600x when given --active 512x224 --display 512x240. spider1's copy is the stale one; remedy is `cp spyro/tools/present_geometry.py spider1/tools/`, not done here (docs-only step). Check which copy you have with `md5sum */tools/present_geometry.py` from ~/repo/psx before quoting any number. See INST-25's 2026-08-06 amendment and spyro I042.
