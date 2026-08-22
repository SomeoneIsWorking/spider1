---
id: C045
kind: claim
status: holds
created: 2026-08-22
tags: 
depends: game/render/guest_frame_commit.cpp#commitCapturedGuestFrame, game/render/render_seam.cpp#RenderSeam::submitFrame, tools/gate.py#FAIL_PATTERNS
---

## Claim

Spider-Man's game-owned submitFrame fence prevents unified RenderQueue captures from crossing frames

## Evidence

At recorded psxport pin 3418a79b, pre-fix headless logs scratch/logs/gate-boot-20260822-122332.log and -122525.log abort at Fps60::rq_capture 65291/65301 + 312 > 65536 after 20-28 s. With game/render/guest_frame_commit.cpp called after retail FUN_80061308, final scratch/logs/frame-fence-final.log reaches 1024 submissions through dem1 -> l1a1; tools/gate.py check-log PASS; present 2600 reports 686985/691200 non-black; no OVERFLOW/FAULT/FATAL/STUCK. Explicit Native scratch/logs/fence-native.log reaches 512 fallback submissions while commit remains under Gte. Full Clang build and 6/6 CTests pass.

The paced headless control `scratch/logs/frame-fence-paced-final.log` (no `PSXPORT_NOPACE`) reached
`dem1 -> l1a1`, frame 2,799 / 512 submissions in 60 seconds with no overflow or fault. Those counts
clear the run gate's 60-second rate floors (frame 1,225 / submissions 492), so the added commit did
not turn pacing into a below-baseline crawl in the headless path. A windowed cadence remains outside
this agent's permitted measurement scope.

## What would falsify it

Falsified if any bounded headless Gte or Native-fallback run reaches Fps60::rq_capture OVERFLOW, if FUN_80061308 ceases to be one complete game-frame submission, or if commitCapturedGuestFrame no longer runs once after each guest submission.
