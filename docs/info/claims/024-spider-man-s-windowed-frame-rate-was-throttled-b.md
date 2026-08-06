---
id: C024
kind: claim
status: holds
created: 2026-08-06
tags: pacing,perf,windowed,paceQuota
depends: game/core/game_config.cpp, game/core/sync_native.cpp
---

## Claim

Spider-Man's windowed frame rate was throttled by GameConfig::paceQuota=2, not by rendering: one gpu_pace_frame call is this port's WAIT QUANTUM, and the guest asks for 1-field waits, so quota=2 served every 1-field wait with a 2-field sleep and halved the frame rate. paceQuota=1 is the derived value and gives ~30 rendered fps.

## Evidence

MEASURED WINDOWED (headless is never paced: gpu_pace_subframe early-returns on !gpu_has_window, so headless cannot measure this at all). Two 260 s runs one build apart, identical env, identical PSXPORT_PAD_REPLAY=scratch/bin/drive.pad, same instruments (PSXPORT_DEBUG=presentskip,pace), log lines TIMESTAMPED by scratch/g1_pace/ts.py so every rate is over measured wall time. Same 215 s steady window (40-255 s) in both.
NEGATIVE CONTROL (unchanged build, paceQuota=2, scratch/g1_pace/logs/before_quota2_paced.log): presents 59.94/s, rebuild_geom 15.57/s, presents/rebuild_geom 3.85, pace entries 30.00/s. A third run with no pace instrument at all (before_quota2.log) gave the same: 59.87/15.4/3.87. So the instrument DID show the failing answer, in this mode, before the change.
AFTER (paceQuota=1, after_quota1.log): presents 59.94/s, rebuild_geom 29.66/s, presents/rebuild_geom 2.02, pace entries 60.00/s.
MECHANISM, measured not inferred, from the game-side PSXPORT_DEBUG=pace channel: 7327 of 7449 field-wait entries (98.4%) serve FUN_8005E748(n=1) -- a request for ONE field. At quota=2, 578 of 599 consecutive entries advanced the guest vblank counter by 2 against that n=1 request, median wall spacing 33.30 ms (= 2/60 s). At quota=1, 599 of 599 advance by exactly 1, median 16.70 ms (= 1/60 s).
The game issues ~1.93 pace entries per rendered frame BEFORE and 2.02 AFTER -- unchanged -- so the loop structure did not move, only the quantum length. Pace-entry rate is exactly 60/quota in both legs.
DERIVATION (game_iface.h:255-271 + game/core/sync_native.cpp:316,382): paceQuota is 'the vblanks ONE gpu_pace_frame call represents', semantics 'by CALLING CADENCE, not the game's display rate'. Both of this port's call sites are 'while (guest_vblank_counter < target) { gpu_pace_frame(c); vblank_advance(c); }', and vblank_advance rederives that counter from real elapsed time -- so one pace call is the wait quantum and the counter is denominated in vblanks. game_iface.h names this exact case: 'A port that still runs the guest's own frame loop and paces once per vblank sets 1.'
NOT VERIFIED: whether ~30 rendered fps is this game's intended rate (no VSync(2) evidence exists -- 427,643 of the game's VSync calls are the query form VSync(-1) and exactly one is blocking), and nothing here measures the user's reported flicker over a whole run.

## What would falsify it

if a windowed run with paceQuota=1 measures rebuild_geom materially below ~30/s in steady state, or if the pace-entry rate stops tracking 60/quota, the mechanism is wrong -- the pacer would no longer be the throttle
