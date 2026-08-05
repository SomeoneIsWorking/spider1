---
id: 9
title: a present INDEX is not content-stable across runs — before/after comparisons at a fixed index can compare different scenes
status: open
symptom: present 4500 showed a corrupted city skyline in one run and a pause screen in another, on builds minutes apart; 'the same frame' is not the same frame
tags: method,instrument,measurement,trap
created: 2026-08-06
updated: 2026-08-06
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
