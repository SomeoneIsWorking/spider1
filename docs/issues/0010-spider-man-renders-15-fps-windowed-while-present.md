---
id: 10
title: Spider-Man renders ~15 fps windowed while presenting at 60: GameConfig::paceQuota=2 made every 1-field guest wait sleep 2 fields
status: resolved
symptom: windowed gameplay is half speed / choppy; PSXPORT_DEBUG=presentskip shows presents ~60/s but rebuild_geom ~15.5/s and presents/rebuild_geom ~3.85; ~91% of wall time inside gpu_pace_subframe's nanosleep; PSXPORT_NOPACE=1 makes it run 30fps
tags: pacing,perf,windowed,paceQuota,fps,gpu_pace_frame,sync_native
created: 2026-08-06
updated: 2026-08-06
---

## Symptom

Windowed, the port presents at ~60/s but only produces ~15.5 NEW rendered frames per second
(`presents/rebuild_geom` ~3.85). `PSXPORT_NOPACE=1` -- same binary, same window, same swapchain --
sustained ~30 rendered fps, which localised the throttle to the pacer rather than to rendering.

## Root cause

`GameConfig::paceQuota` was 2, chosen as "this game's 30fps base cadence". That is not what the
field means. `game_iface.h:255-271` defines it as the number of vblanks that ONE `gpu_pace_frame`
call represents, explicitly "by CALLING CADENCE, not the game's display rate", and the framework
sleeps `quota/60 s` per call.

This port has exactly two pace call sites (`game/core/sync_native.cpp:316` and `:382`), both the
same shape:

    while (<guest vblank counter> < target) { gpu_pace_frame(c); vblank_advance(c); }

`vblank_advance` rederives that counter from REAL elapsed time at the NTSC field rate, so the loop
is a real-time wait and one `gpu_pace_frame` call is its WAIT QUANTUM -- the granularity at which
the wait can notice it is finished. The counter is denominated in vblanks, so the quantum has to be
one vblank or every wait is rounded up to a multiple of it. `game_iface.h` names this exact case:
"A port that still runs the guest's own frame loop and paces once per vblank sets 1."

And the guess also mis-modelled the game: it does NOT ask for two fields at a time. Measured with the
new `PSXPORT_DEBUG=pace` channel, 7327 of 7449 field-wait entries (98.4%) are `FUN_8005E748(n=1)`
-- a request for ONE field; the remaining 122 are a single n=240 loading delay. So every one-field
wait was served with a two-field sleep, the game issues ~2 of them per rendered frame, and the frame
budget became 4 fields instead of 2.

## Fix

`game/core/game_config.cpp`: `paceQuota` 2u -> 1u, with the derivation written at the definition.
Game-side only; no framework change.

## Evidence, and the negative control

WINDOWED both legs -- headless is never paced (`gpu_pace_subframe` early-returns on
`!gpu_has_window()`), so a headless run cannot measure this at all. Two 260 s runs one build apart,
identical env and identical `PSXPORT_PAD_REPLAY=scratch/bin/drive.pad`, same instruments
(`PSXPORT_DEBUG=presentskip,pace`), log TIMESTAMPED by `scratch/g1_pace/ts.py`, same 215 s steady
window (40-255 s):

| | presents/s | rebuild_geom/s | presents/geom | pace entries/s | entries per rendered frame |
|---|---|---|---|---|---|
| quota=2 (NEGATIVE CONTROL) | 59.94 | 15.57 | 3.85 | 30.00 | 1.93 |
| quota=1 (fix)              | 59.94 | 29.66 | 2.02 | 60.00 | 2.02 |

The negative control is the same instrument in the same mode on the UNCHANGED build, and it produced
the failing numbers. A third run without the `pace` instrument (`before_quota2.log`) gave
59.87/15.4/3.87, so adding the instrument did not move the measurement.

Per-entry behaviour, the decisive discriminator: at quota=2, 578 of 599 consecutive field-wait
entries advanced the guest vblank counter by 2 against an `n=1` request, median wall spacing
33.30 ms (= 2/60 s). At quota=1, 599 of 599 advance by exactly 1, median 16.70 ms (= 1/60 s).

Pace entries per rendered frame is unchanged (1.93 -> 2.02): the game's loop structure did not move,
only the length of the quantum. The pace-entry rate is exactly 60/quota in both legs.

Logs: `scratch/g1_pace/logs/{before_quota2,before_quota2_paced,after_quota1}.log`.
Shots: `scratch/g1_pace/shots/<tag>/` (harvested from the shared accumulator by mtime window --
never globbed). See claim C024 and instrument I020.

## What this does NOT establish

- That ~30 fps is this game's INTENDED rate. The game declares no frame rate: 427,643 of its VSync
  calls are the query form `VSync(-1)` and exactly one is blocking, so there is no `VSync(2)`
  evidence either way. What is established is that the port no longer inflates the waits the game
  does ask for.
- Anything about the user-reported flicker over a whole run. 12 present shots per leg were sampled;
  all are 99.80-99.85% non-black in both legs, so no black frames at those points -- but that is 12
  of 15412 presents and it is not a flicker measurement.
