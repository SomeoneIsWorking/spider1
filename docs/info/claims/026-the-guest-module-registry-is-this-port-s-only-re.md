---
id: C026
kind: claim
status: holds
created: 2026-08-06
tags: render,classifyScene,scene-identity,module-registry,re-13,tomba2-pattern
depends: game/core/module_loader.cpp, the retired static overlay registry, docs/re-frontier.md
reconfirmed: 2026-08-22 19:16:22
verified_at: 2026-08-22 19:16:22
---

## Claim

The guest module registry is this port's ONLY RE'd scene-identity source, and it is a near-useless scene discriminator: 94.3 percent of a gameplay-reaching run shares one constant resident set that spans the attract fly-through AND live gameplay

## Evidence

230s windowed run, PSXPORT_DEBUG=ovload,presentskip PSXPORT_FORCE_BUTTONS=4000, reaching present 13757 (scratch/logs/g8/ovload_census.log). The ovload channel logs BOTH placement (overlay_router.cpp:156) and eviction (:170), so the census has no silent half. TOTAL: 8 module events, 3 distinct names (SHELL, THUG, BLACKCAT), ALL before ~present 780 = 5.7% of the run; the remaining 94.3% holds one constant set {THUG, BLACKCAT} with zero events. That span is NOT one scene — present shots from the same run (scratch/g8_shots/, each verified against its own [present_shot] log line and mtime, not globbed): p800 73.75% non-black / 404 colours = attract 3D city fly-through; p3000 74.96% / 831 = attract rooftop with an NPC; p6000 74.26% / 595; p9500 99.82% / 1986 = LIVE 3D GAMEPLAY, Spider-Man on a New York rooftop with full HUD. Tomba!2's Render::classifyScene (game/render/render_walk.cpp:313) instead keys on 5 RE'd fields (0x801FE000 task0 state, 0x801FE00C stage ptr, 0x1F800138 task-sm, 0x801FE048 substate, 0x80109450 overlay sig); this port's equivalents are RE-12 and RE-13, both status:todo with empty evidence/where/gap/notes.

## What would falsify it

RE-13 landing a real scheduler/stage lens, OR anyone finding a guest field that separates the attract fly-through from live gameplay. Re-run the census and check whether any module event falls after present 780, and whether two visually distinct scenes ever differ in resident set.

## Re-confirmed 2026-08-22 19:16:22

Post-commit af8a3c0 bounded allocator/census A/B reaches dem1 on the healthy leg while module-loader auditing adds only first-write observation; scene registry ownership remains unchanged.
