---
id: C045
kind: claim
status: holds
created: 2026-08-22
tags: []
depends: titles/spiderman1/spider1_frame_driver.cpp#Spider1FrameDriver::stepFrame, titles/spiderman1/spider1_mode_driver.cpp#Spider1ModeDriver::step, game/render/render_seam.cpp#RenderSeam::submitFrame, tools/gate.py#FAIL_PATTERNS
reconfirmed: 2026-08-22 19:16:22
verified_at: 2026-08-22 19:16:22
---

## Claim

Spider-Man's game-owned submitFrame fence prevents unified RenderQueue captures from crossing frames

## Evidence

At recorded psxport pin 3418a79b, pre-fix headless logs scratch/logs/gate-boot-20260822-122332.log and -122525.log abort at Fps60::rq_capture 65291/65301 + 312 > 65536 after 20-28 s. The original `game/render/guest_frame_commit.cpp` implementation then proved the FUN_80061308 fence in live runs: final scratch/logs/frame-fence-final.log reaches 1024 submissions through dem1 -> l1a1; tools/gate.py check-log PASS; present 2600 reports 686985/691200 non-black; no OVERFLOW/FAULT/FATAL/STUCK. On 2026-08-27 the same boundary moved to `Spider1FrameDriver::stepFrame`, which dispatches FUN_80061308 and commits once immediately after it; the render seam no longer presents independently.

The paced headless control `scratch/logs/frame-fence-paced-final.log` (no `PSXPORT_NOPACE`) reached
`dem1 -> l1a1`, frame 2,799 / 512 submissions in 60 seconds with no overflow or fault. Those counts
clear the run gate's 60-second rate floors (frame 1,225 / submissions 492), so the added commit did
not turn pacing into a below-baseline crawl in the headless path. The 2026-08-27 finite-mode
migration moves all primary/transition/menu/alternate submit fences through `Spider1ModeDriver` and
`Spider1FrameDriver`; that expanded route is statically built but is not reconfirmed by a product run
in the no-launch task. A windowed cadence remains outside this evidence.

## What would falsify it

Falsified if any bounded headless Gte or Native-fallback run reaches Fps60::rq_capture OVERFLOW, if FUN_80061308 ceases to be one complete game-frame submission, or if the title drivers no longer commit once immediately after every retail mode submission.

## Re-confirmed 2026-08-22 19:16:22

Post-commit af8a3c0 gate selftest passes 19/19 including allocator and memory error verdicts; authoritative Clang CTest passes 8/8.
