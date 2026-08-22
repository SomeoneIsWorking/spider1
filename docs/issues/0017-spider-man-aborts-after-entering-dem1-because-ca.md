---
id: 17
title: Spider-Man aborts after entering dem1 because captured render queues never reach a game-frame fence
status: resolved
symptom: headless/default render reaches dem1 then aborts with Fps60::rq_capture OVERFLOW near 65536 captured items; native menu/world cannot remain runnable
tags: render,queue,frame-fence,fps60,guest-loop,runtime
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

psxport's unified RenderQueue captures every retail `DrawOTag` flush until
`Fps60::frame_commit` rotates it. Spider-Man runs its own recompiled guest loop rather than
`native_step_frame`, and supplied no game-frame fence. `mNCur` therefore accumulated across real
frames until the fail-fast reported `65291 + 312 > 65536`.

## What was tried / dead ends

- Raising `FPS60_RQ_MAX` was rejected: the accumulation was unbounded, so a larger cap would only
  delay the same abort.
- Disabling `ires` and removing `face_order=0` in isolated scratch settings reproduced the same
  overflow. Those recently changed settings were not the cause.
- Calling `frame_commit` at the retail submit boundary was tested before being structured. It ran
  for 240 seconds through 6,144 submissions and multiple named scenes without overflow, establishing
  that `FUN_80061308` is the missing fence rather than merely reducing queue growth.

## Resolution

`game/render/guest_frame_commit.cpp` owns the derived per-game runtime responsibility and commits
exactly once after the binary-proven complete `FUN_80061308` retail body. HACK-03 keeps its temporary
Gte scope active through commit/present, so queue delivery cannot re-enable native enhancements.

Evidence:

- Before: `scratch/logs/gate-boot-20260822-122332.log` and `-122525.log` abort in 20–28 seconds.
- Final Gte: `scratch/logs/frame-fence-final.log` reaches 1,024 submissions through
  `dem1 -> l1a1`; present 2600 is 686,985/691,200 non-black (99.39%); gate `check-log` passes.
- Final explicit Native: `scratch/logs/fence-native.log` reaches 512 fallback submissions with
  `interpolation=0`, `nativeSubmitted=0`, and no overflow.
- Final paced headless control (no `PSXPORT_NOPACE`):
  `scratch/logs/frame-fence-paced-final.log` reaches `dem1 -> l1a1`, frame 2,799 / 512 submissions
  in 60 seconds, above the run gate's scaled rate floors, with no overflow/fault. Windowed cadence
  was not measured because agent runs are headless-only.
- Full Clang build and all 6 CTests pass; `cpp_policy` checks format 41/41 and clang-tidy 26/26.
- `tools/gate.py` now matches the exact overflow line and its selftest feeds that opposite answer.
