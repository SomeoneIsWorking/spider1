---
id: C027
kind: claim
status: holds
created: 2026-08-06
tags: pacing,temporal,flicker,windowed
depends: game/core/sync_native.cpp#vblank_advance
---

## Claim

Spider-Man's windowed present cadence has a 16.68 s BEAT: gpu_pace_subframe paces at 60.000 Hz while vblank_advance counts fields at 59.940 Hz, and for ~1.8 s out of every ~16.7 s presents arrive as a 1 ms pair + 32 ms gap, dropping new pictures reaching the screen from 29.0/s to 19.7/s

## Evidence

Three legs, same replay (replays/bugs/pause-corruption.pad), same env, same 10-95 s steady window, PSXPORT_DEBUG=presentclock. A) as shipped: 5096 presents, 59.95/s, median gap 16.666 ms, 243 gaps <4 ms (4.77%), 253 >25 ms (4.97%), 5 periodic bursts at 15.09/16.67/16.87/16.59 s spacing. B) PSXPORT_NOPACE=1 control: 5095 presents, median 16.683 ms, 10 <4 ms (0.20%), 0 bursts. C) ABLATION keeping the pacer and changing ONLY vblank_advance's divisor 1001->1000 (i.e. 59.94->60.000 Hz), then reverted: 5100 presents, 13 <4 ms (0.25%), 0 periodic bursts. Predicted beat 1/(60-60000/1001)=16.683 s with no fitted parameter; measured burst-centre intervals 16.58/16.86/16.62/16.78/16.59/16.55 s (run1) and 16.67/16.87/16.59/16.90/16.48/16.88 s (run2). Cost: joining presentclock with presentskip over 5995 presents, 27.60 rebuild_geom/s, 221 paired submissions of which 70 (31.7%) discard a newly-rendered picture against a 46.7% baseline; inside burst seconds new pictures reaching the screen are 19.7/s vs 29.0/s outside.

## What would falsify it

If gpu_pace_subframe stops using a literal 60.0 (gpu_native.cpp:1568) or vblank_advance stops using 60000/1001, the two clocks may agree and the beat is gone — recheck. Also falsified if the MAILBOX-discard step is wrong: that the EARLIER member of a 1 ms pair never reaches the screen is INFERRED from the swapchain present mode, not measured, because scanout is not observable from inside the process. A compositor that presented both would make the frame-loss half of this claim false while leaving the cadence half intact.
