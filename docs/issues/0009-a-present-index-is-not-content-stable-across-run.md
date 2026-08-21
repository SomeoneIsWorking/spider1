---
id: 9
title: a present INDEX is not content-stable across runs — before/after comparisons at a fixed index can compare different scenes
status: open
symptom: present 4500 showed a corrupted city skyline in one run and a pause screen in another, on builds minutes apart; 'the same frame' is not the same frame
tags: method,instrument,measurement,trap
created: 2026-08-06
updated: 2026-08-21
---

FOUND 2026-08-06 while re-checking the city-skyline corruption noted in issue 0007. I captured
present 4500 with both render fixes in, expecting to see whether the corruption survived. It showed
a PAUSE SCREEN instead — different content entirely. The corruption was neither fixed nor unfixed;
I had simply looked at a different scene.

THE CAUSE: these runs are driven with PSXPORT_FORCE_BUTTONS=4000, which PULSES cross (8 frames on,
24 off) to walk the front-end. Where that pulse train lands depends on load timings, so a run reaches
a different game state by a given present index. Two runs on this machine differed enough that
present 4500 was a skyline in one and a pause menu in another. Present index is a CLOCK, not a
BOOKMARK.

WHY IT IS WORTH AN ENTRY: "capture frame N before, capture frame N after, compare" is the shape of
almost every render check in this project, and it is INVALID whenever the two runs can diverge in
state. It fails silently and plausibly — you get two real frames and a difference, and the
difference is the scene, not the change.

WHAT THIS DOES *NOT* INVALIDATE, checked rather than assumed:
  * issue 0008's aspect gate. It measured GEOMETRY (is the picture 4:3?), which is a property of the
    PRESENTATION and not of the content. A different scene at the same index still answers it.
  * issue 0007's palette fix. Its decisive evidence was an ABLATION inside ONE run (toggle the DMA2
    drain, same binary, same run) plus a hermetic unit test — neither depends on cross-run index
    stability. The cross-run strip dumps were corroborating, not load-bearing.
Both conclusions stand. That is luck as much as design, and the next comparison may not be so lucky.

HOW TO MAKE A CROSS-RUN COMPARISON VALID — pick one, and say which you used:
  * measure something CONTENT-INDEPENDENT (geometry, present-mode, counters with denominators);
  * ablate INSIDE one run (toggle the change, same binary, same session) — the strongest form, and
    what settled 0007;
  * anchor on a STATE the log names ("free-roam reached", "[disc] opened", a stage id) instead of an
    index — Tomba2's PSXPORT_AUTO_SKIP does exactly this and is the model to copy;
  * or use a deterministic input replay (PSXPORT_PAD_REPLAY) rather than a pulse train.

STILL OPEN AND UNEXAMINED, recorded so it is not lost: (1) the original city-skyline corruption from
issue 0007 has NOT been re-checked with the fixes in — I never actually saw it again. (2) The pause
screen at present 4500 shows a large translucent spider-emblem polygon overlaying the whole scene,
and its content band measures 16:9 (960x540) rather than 4:3 in a run where the display register
reads 512x240. Either the widescreen mod was active for that frame or something else is, and I did
not establish which. Neither observation is diagnosed; both need a run anchored on state, not index.

### Note (2026-08-06)
PARTIAL FIX 2026-08-06: tools/make_pad_replay.py + PSXPORT_PAD_REPLAY. Input determinism removes the
LARGE divergence but NOT all of it, and the residual is measured rather than hand-waved.

    python3 tools/make_pad_replay.py scratch/bin/drive.pad --frames 20000 --button 4000
    PSXPORT_NOWINDOW=1 PSXPORT_PAD_REPLAY=scratch/bin/drive.pad PSXPORT_PRESENT_SHOT_AT=10000 ./run.sh

The file is one uint16 LE active-low mask per pad frame, forced back verbatim, generated with the
same pulse shape FORCE_BUTTONS uses (8 on / 24 off). --button takes the SAME hex a human writes for
PSXPORT_FORCE_BUTTONS and inverts it internally, so the two knobs cannot disagree about polarity —
getting that backwards produces a replay holding every button except the intended one, which reads
as "the game ignored my input".

MEASURED, two runs, same replay, same present index 10000, both logging "replaying 20000 frames":
    det_A: 99.8% non-black, 1712 colours
    det_B: 99.8% non-black, 1720 colours
    cmp: differ at byte 730
SO: the same SCENE both times — which is the property that was missing and the reason 0009 exists —
but NOT the same FRAME. Compare that to the failure this issue was opened for, where the same index
gave a city skyline and a pause menu.

WHAT THAT IS AND IS NOT GOOD FOR, stated so nobody over-trusts it:
  * GOOD ENOUGH for "did this defect survive the fix?" — you are looking at the same content, so a
    before/after comparison is answering the question you asked.
  * NOT good enough for byte-exact diffing or for a pixel-level regression gate. 8 colours of drift
    means the two frames are a few animation/pacing ticks apart, not identical.
  * The residual is NOT the input any more. It is whatever else the port derives from wall-clock —
    frame pacing, CD delivery timing, the host turn. Naming that is the next step if byte-exactness
    is ever needed; PSXPORT_NOPACE=1 is the obvious first thing to try, and I did not.

STILL OPEN, unchanged: the original city-skyline corruption has STILL not been re-observed, and the
pause-screen spider-emblem overlay / 16:9 band is still undiagnosed. Both now have a repeatable way
to be looked at, which they did not before.

### Note (2026-08-06)
2026-08-06 (step G6). PARTIAL PROGRESS on this entry's second open item — the pause screen's
"16:9 content band". Measured, not inferred, and it is NOT the widescreen mod.

Captured 390 consecutive presents at the PRESENT stage across three scenes and measured the lit-row
extent of every one (960x720 windowed sink):

    pause screen, resting   150/150 presents lit rows 90..629  -> 960x540 content band (1.778)
    rooftop, presents 1500+   41/120 presents lit rows 90..629  -> 960x540
                              79/120 presents lit rows 0..719   -> 960x720 (1.333, full 4:3)
    front-end/loading        87/120 presents lit rows 570..602 (a text strip), 33 fully black

So the band is real and reproducible — but it is NOT constant, and in the rooftop capture it
switches ONCE (T x41 then F x79) and never switches back. Looking at the two frames: the 960x540
ones have NO HUD and a cinematic camera; the 960x720 ones carry the health bar, ammo count and
"FOLLOW THE SPIDEY COMPASS" prompt. That is the GAME's own cinematic letterbox, drawn by the game,
not a present-stage aspect error — which also explains why issue 0008's aspect gate (which measured
the display rect) and this observation can both be right.

WHAT IS STILL NOT ESTABLISHED, so nobody reads this as closed: whether the PAUSE screen's band is
the same game-drawn cinematic letterbox (the pause was entered from a state this replay reaches, and
I did not check whether that state is a cutscene) or something else. Deciding it needs the display
register at that present, which I did not capture. The measurement above is a content-extent
measurement and inherits the standing caveat that a content bbox is not a display rect — it is
trustworthy here only because three very different scenes agreed on the same two exact extents.

Also confirmed still present, undiagnosed: the large translucent spider-emblem polygon over the whole
pause scene (scratch/shots-g6/B_rest/p003600.ppm). It is byte-identical across all 150 presents, so
whatever it is, it is stable and not flickering.

The pad replay that reaches this state is replays/bugs/pause-corruption.pad; the resting state after
the replay ends (present ~2600 onward) is the pause screen and is completely static, which makes it a
good anchor for anyone re-examining it: 150 consecutive presents were byte-identical.

### Note (2026-08-21)

The trap reproduced while trying to picture-gate HACK-03. A Native run with
`PSXPORT_NOPACE=1`, forced Cross, and `PSXPORT_PRESENT_SHOT_AT=450` captured a wholly black frame
before `dem1`; after 39 seconds the log still contained only the boot-init scene even though a
same-build bounded run without the shot request reached `dem1` and `l1a1` in seven seconds. The
capture API reported exactly what it sampled and is not distrusted, but present 450 was not a
content anchor. The black PPM was deleted and no native-vs-Gte picture claim was made. A valid
fallback picture comparison needs a scene/content-triggered capture, not another selected index.
