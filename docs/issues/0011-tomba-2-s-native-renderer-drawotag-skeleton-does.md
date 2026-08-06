---
id: 11
title: Tomba!2's native-renderer drawOTag skeleton does not transfer to spider1: the hook is unreachable and there is no scene identity to classify on
status: dead-end
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

    external/psxport/runtime/recomp/native_boot.cpp:173   c->hooks->drawOTag(c, envp + 0x1ffcu);
    external/psxport/runtime/recomp/native_boot.cpp:194   c->hooks->drawOTag(c, envp + 0x1ffcu);   // dualview only

Both are inside `native_step_frame`, which is only reached from the frame loop in `game_main`
(`native_boot.cpp:452`). And this port never gets there:

    native_boot_run -> native_crt0 -> crt0_setup + game_main
                     -> game_init  -> hooks->bootInit
                                   -> spiderman_bootInit: rec_dispatch(c, cfg->gameMain)   <-- never returns

`spiderman_bootInit` (game/core/game_hooks.cpp) dispatches the guest's own `main()` at 0x8002C354,
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
