---
id: C032
kind: claim
status: holds
created: 2026-08-06
tags: scene,classifyscene,re-23,census,identity
depends: game/render/scene_id.cpp
---

## Claim

The game's level-name lens at 0x800A568C DOES discriminate scenes at runtime — three distinct identities over one boot+attract+level run, where the module registry gave one

## Evidence

RUNTIME CENSUS 2026-08-06, the measurement C030 said to take before building a classifier on this lens. Instrument: the render seam's own scene census (game/render/render_seam.cpp censusTick + game/render/scene_id.cpp), which reads 0x800A568C at every submitFrame call and logs on CHANGE, unconditionally, at lucent::info. Run: headless, PSXPORT_NOPACE=1, PSXPORT_FORCE_BUTTONS=4000, ~200 s (scratch/re20/logs/census_200s.log), denominator 1024+ submitFrame calls / 3300 presents. OBSERVED, in order: call #1 frame 2 raw=00,00,00,00 name unset code=0xFFFFFFD0 printable=0 (the mode switch has not written a level name yet — an honest 'not set', not a failure); call #2 frame 379 name='dem1' code=0x9901 (the 'd'/'D' scheme, level 0x99 sub 1); call #9 frame 513 name='l1a1' code=0x0101, which is inside the 0x100..0x105 range FUN_80062CE0's own switch tests. It then held 'l1a1' through call 1024 / frame 2720. A SECOND run with no input at all (scratch/re20/logs/psxleg_60s.log) reached only unset -> 'dem1' (at frame 2248 rather than 379, the FMVs not being skipped), which is the expected difference and shows the lens tracks the mode rather than the clock. CONTRAST, the same question asked of the other source: C026's module-registry census gave 3 names, all events before present 780, and ONE constant resident set over the remaining 94.3 percent of a 13757-present run spanning attract AND live gameplay. The encoder is a faithful port of FUN_8005A734, disassembled at 0x8005A734..0x8005A7B8 this session.

## What would falsify it

a run showing 'l1a1' (or any single code) spanning two scenes that need DIFFERENT native producers — the attract fly-through and live gameplay in particular have NOT been shown to differ here, and would need a present-shot correlation to settle. That within-level substate gap is RE-13 and this claim does not cover it.
