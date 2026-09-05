---
id: 11
title: Tomba!2's native-renderer drawOTag skeleton does not transfer to spider1: the hook is unreachable and there is no scene identity to classify on
status: resolved
symptom: GameHooks::drawOTag is never called in this port, so a fail-fast native-renderer skeleton implemented there is dead code; and classifyScene() has no RE'd scene-identity state to key on
tags: render,native-renderer,drawOTag,classifyScene,pc_render,re-12,re-13,phase-0,dead-end,tomba2-pattern
created: 2026-08-06
updated: 2026-08-06
---

## What was asked, and why it cannot be built yet

The plan was to port Tomba!2's native-renderer skeleton to this repo: implement
`GameHooks::drawOTag` with two branches (`psxRender()` -> `gpu_dma2_linked_list` + `rq.flush`;
otherwise `renderScene()`), plus `classifyScene()` and an `abortUnimplemented()` that names the
scene it could not draw. The intended gate: **with native rendering selected the port aborts on
frame 1 naming the scene.**

That gate is unmeetable in this port today, for two independent measured reasons. Both trace to the
same root: **Tomba!2's pattern presupposes the port OWNS the frame loop, and this port does not
(Phase 0 — the guest's own `main()` runs on the substrate).**

## 1. `GameHooks::drawOTag` is UNREACHABLE here — implementing it is dead code

`hooks->drawOTag` has exactly **two** call sites in the whole framework
(`grep -rn drawOTag external/psxport` — 16 hits, 14 of them comments):

    external/psxport/runtime/psx/native_boot.cpp:173   c->hooks->drawOTag(c, envp + 0x1ffcu);
    external/psxport/runtime/psx/native_boot.cpp:194   c->hooks->drawOTag(c, envp + 0x1ffcu);   // dualview only

Both are inside `native_step_frame`, which is only reached from the frame loop in `game_main`
(`native_boot.cpp:452`). And this port never gets there:

    native_boot_run -> native_crt0 -> crt0_setup + game_main
                     -> game_init  -> runtime->bootInit
                                   -> SpiderRuntime::bootInit: rec_dispatch(c, gameMain)   <-- never returns

`SpiderRuntime::bootInit` (game/core/spider_runtime.cpp) dispatches the guest's own `main()` at 0x8002C354,
which is the game's own infinite loop. Control never returns to `game_init`, so `game_main`'s frame
loop below it never starts.

**MEASURED, with a both-classes discriminator.** The instrument is the unconditional
`lucent::info("native_boot", "entering native frame loop ({})", ...)` at `native_boot.cpp:295` —
always emitted, no channel gate.

* NEGATIVE (this port): 0 occurrences in a 230 s windowed run reaching **present 13757**
  (`scratch/logs/g8/ovload_census.log`). The sibling `lucent::info` from the same file 65 lines
  earlier, `"entering native crt0 (PC-driven)"`, DID print (line 30 of
  `scratch/logs/g8/base_psx.log`), and so did this port's own
  `[boot] Phase 0: dispatching guest main() 0x8002C354` — so the sink is live and the absence
  localizes to exactly one thing: `bootInit` did not return.
* POSITIVE (the other class): the SAME line is present in Tomba2Engine runs, which do enter the
  loop — `grep -h "entering native frame loop" ../Tomba2Engine/scratch/logs/*.log` ->
  `[native_boot] entering native frame loop (interactive (until window close))`.

So a `drawOTag` implemented here would never execute. The abort would never fire, the negative
control would be trivially "unchanged" because nothing changed, and the codemap would acquire a
second **"wired, never executed"** row next to RE-08's.

## 2. There is nothing to CLASSIFY on — `classifyScene()` cannot name a scene here

Tomba!2's `Render::classifyScene` (game/render/render_walk.cpp:313) keys on five RE'd guest fields:
task0 scheduler state `0x801FE000`, the resident stage pointer `0x801FE00C`, the task-sm pointer
`0x1F800138`, the front-end substate selector `0x801FE048`, and an overlay signature `0x80109450`.
Every one of those comes from having RE'd the cooperative scheduler and the stage machine.

In this port **RE-12 (per-frame OT / packet-pool layout)** and **RE-13 (scheduler task layout)** are
both `status: todo` with empty `evidence`/`where`/`gap`/`notes`, and docs/codemap.md records
"the SDK task model may not apply to this engine". There is no known stage pointer and no known
substate selector.

The ONE genuinely RE'd scene-identity source this port has is the guest's own module registry
(the `$gp + 0x5C8` descriptor list lensed in `game/core/module_loader.cpp`, RE-09/HACK-02
re-verified). **It was measured as a scene discriminator and it is nearly useless as one.**

230 s windowed run, `PSXPORT_DEBUG=ovload,presentskip PSXPORT_FORCE_BUTTONS=4000`, reaching present
13757 (`scratch/logs/g8/ovload_census.log`). The `ovload` channel logs BOTH placement and eviction
(`overlay_router.cpp:156` and `:170`), so this census has no silent half:

    log line 184  SHELL     live at 0x8014D5AC..0x801695AC
    log line 429  SHELL     evicted
    log line 432  THUG      live at 0x8018DA28..0x80199228
    log line 443  THUG      evicted
    log line 565  SHELL     live at 0x8014D5AC..0x801695AC
    log line 607  SHELL     evicted
    log line 771  THUG      live at 0x80156C30..0x80162430
    log line 772  BLACKCAT  live at 0x80162438..0x80164438
    ---- and then NOTHING, for the remaining ~13000 presents ----

**8 module events, 3 distinct names, ALL of them before ~present 780 (5.7% of the run). The
remaining 94.3% of the run shares ONE constant resident set, `{THUG, BLACKCAT}`.**

And that constant-identity span is not one scene. Present shots from the same run
(`PSXPORT_PRESENT_SHOT_AT=800,3000,6000,9500`, copies kept in `scratch/g8_shots/`, each verified
against its own `[present_shot] wrote ...` log line and its mtime rather than globbed out of the
shared accumulator):

    present  800   73.75% non-black,  404 colours — attract: 3D city fly-through, camera inside geometry
    present 3000   74.96% non-black,  831 colours — attract: rooftop scene with an NPC
    present 6000   74.26% non-black,  595 colours
    present 9500   99.82% non-black, 1986 colours — LIVE 3D GAMEPLAY: Spider-Man on a New York
                                                    rooftop, full HUD (health bar, web cartridge
                                                    x01, mission marker, compass)

A `classifyScene()` built on the only identity this port has would return **the same value for the
attract fly-through and for live gameplay with the HUD up.** The crash sequence would therefore be a
backlog of length one, not "the porting backlog in dependency order" — which is the entire value of
the fail-fast pattern.

## Verdict

**The mechanism transfers; the identity data does not — and the call site does not exist yet.**
(Compare RE-17's note, which found exactly the first half of that for the projection constants:
"Tomba!2's pattern TRANSFERS as a mechanism but NOT as data".)

Building the skeleton now would produce dead code with a classifier that cannot classify, and a gate
that reports green because nothing ran. **The blocker is RE-13 (scheduler task layout) — the scene
identity — and RE-12 (per-frame OT / packet-pool layout) — the frame loop that would call
`drawOTag`.** Both are listed `RE-ready` by `re_frontier.py next`. Do those first.

If a reachable fail-fast is wanted BEFORE the native frame loop exists, the only game-side seam is a
native override on this game's libgpu `DrawOTag` (the shape `module_loader.cpp` already uses via
`engine_set_override_main`), which needs that guest address RE'd first — it is not in `GameConfig`
and is not recorded anywhere in this repo. But note that it buys a reachable abort with nothing to
put in it until RE-13 lands.

## What was NOT checked

* No attempt was made to RE this game's libgpu `DrawOTag` address.
* No attempt was made to find a scene-identity field other than the module registry; the claim
  "there is no other RE'd source" is a claim about what this REPO records (RE-12/RE-13 empty,
  codemap "missing"), not a claim that the game has no such field. It certainly has one — nobody has
  found it.
* The runs above were NOT serialized under `coord/window-lock.sh`, so the present COUNTS are not
  valid as a frame-rate figure. Nothing above depends on them as a rate: the module census, the
  scene identities and the reachability result are all contention-invariant.
* Nothing in this port was changed, so there is no before/after to compare.

### Note (2026-08-06)
SUPERSEDED 2026-08-06 by RE-19/RE-20/RE-23. The VERDICT of this entry ('do RE-13 and RE-12 first') is WRONG and is corrected here rather than left standing; the two measurements it rests on are NOT withdrawn.

WHAT STILL HOLDS. At the time measured, GameHooks::drawOTag had only the generic native_step_frame call sites and Spider-Man's non-returning guest main never reached them. C025 is now falsified because Spider1FrameDriver returns from a finite prefix and owns the framework loop, but that title driver still does not use the generic drawOTag hook: the measured FUN_80061308 seam remains the owner. The module-registry census still found 8 load/evict events / 3 names over 13757 presents with 94.3% of the run on one constant set (C026).

WHAT WAS WRONG. (1) 'The hook is unreachable, therefore the native-renderer pattern has no call site here' — the second half does not follow. The MECHANISM does not need GameHooks::drawOTag: the game has its own OT-submit function, FUN_80061308, which is the ONLY game-side caller of libgpu DrawOTag in the whole image (xrefs 0x80081ED0 -> 2 callers, one library-internal), which runs once per engine-rendered frame, and which the recomp override table reaches today because it is a MAIN-module address. MEASURED headless, 100 s, no input, PSXPORT_FNTRACE (scratch/re12/logs/fntrace1.log): 1761 calls, first at frame 2, 0 ABI violations — and fntrace installs a REAL override at each site, so those hits ARE the proof an override there executes. Two of the eight traced sites reported NEVER CALLED in the same run, so the instrument could produce the other answer. (C029.)
(2) 'There is nothing to classify on' — true of the module registry, false of the game. FUN_80062CE0 (called twice per frame from the render walk) switches on FUN_8005A734(), which encodes the current LEVEL NAME string at 0x800A568C into (level<<8)|sub — exactly the 0x201/0x302/0x704-shaped constants in its own switch. That is a scene identity this port had all along and nobody looked for. STATIC ONLY: no run has yet read 0x800A568C, so a runtime census is required before building on it. (C030.)

CORRECTED ORDER: RE-20 (override 0x80061308; prove it BREAK-FIRST — suppress the submit and the presented picture must collapse, measured by distinct-colour count, not non-black %) -> RE-23 census -> RE-21 (the display-object producers, which is where the real work is) -> RE-18. RE-12's OT/packet-pool layout is now RE-verified (DB pair 0x8009A6E4/0x8009A75C, ot +0x70, pool +0x74, both heap-allocated) and RE-22 (owning the frame loop) is NOT a prerequisite for any of the above.

### Note (2026-08-06)
LANDED 2026-08-06 — the thing this entry said could not be built now EXISTS and its gate is met. game/render/render_seam.cpp installs a recomp override on the guest's own submitFrame FUN_80061308 (not the unused generic GameHooks::drawOTag route) with the two-branch shape: psx_render super-calls the recompiled body, pc_render runs no gen body and dispatches renderScene(). game/render/scene_id.cpp ports the engine's own scene-identity encoder FUN_8005A734 over the level name at 0x800A568C. MEASURED: override REACHED at call #1 / frame 2 / ra=80061218, matching RE-19's independent fntrace measurement digit-for-digit; PSXPORT_RENDER_PSX=0 aborts on the first engine frame printing the scene name+code, the DB lens and the projection state. BREAK-FIRST negative control taken with a throwaway build (C031): suppressing the submit FREEZES the presented picture — 1 distinct picture over 20 presents, 0 of 13,132,800 pixel comparisons changed — while the same instrument on the reference leg changes up to 32.75 percent of a frame. RUNTIME CENSUS taken (C032): the level-name lens gives unset -> 'dem1' (0x9901) -> 'l1a1' (0x0101) over one run, against the module registry's effectively one value. WHAT THIS ENTRY GOT RIGHT AND WHAT IT GOT WRONG is already written up in the previous note and is unchanged: both its measurements stand, its VERDICT ('do RE-13 and RE-12 first') was wrong. What remains is RE-21 — no display-object type is decoded, so the abort names the SCENE but not the unhandled TYPE, and the crash list is still a backlog of length one.
