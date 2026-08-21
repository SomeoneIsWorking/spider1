---
id: C044
kind: claim
status: holds
created: 2026-08-21
tags: render,re-21,hack-03
depends: game/render/guest_frame_fallback.cpp#decideGuestFrameFallback, game/render/render_seam.cpp#RenderSeam::submitFrame, game/render/render_seam.cpp#RenderSeam::seamPass
reconfirmed: 2026-08-21 14:15:27
verified_at: 2026-08-21 14:15:27
---

## Claim

Spider-Man HACK-03 submits an unported named scene as one mutually-exclusive, non-interpolated retail guest frame: no native producer runs in that frame, and the guest body executes under pure Gte mode

## Evidence

Structural: render_seam decides before FrameEnvelope::produce; FallbackGuest skips the native branch, enters GuestFrameFallbackModeScope(RenderPath::Gte), and super-calls unmodified FUN_80061308. Production decision refuses nativeSubmissionStarted and fps60.active. Unit test exercises SUBMIT, DISABLED, NATIVE_PRODUCER_READY, NATIVE_OVERLAP_FORBIDDEN, INTERPOLATION_FORBIDDEN plus Gte-mode restoration. Retail-disc Native run scratch/logs/re21-guest-fallback-positive.log reached dem1 then l1a1 and logged SELECTED/SUBMITTED with nativeSubmitted=0 interpolation=0. Identical-boundary controls logged DISABLED and INTERPOLATION_FORBIDDEN in scratch/logs/re21-guest-fallback-disabled.log and re21-guest-fallback-fps60-refusal.log.

## What would falsify it

Falsified if a fallback frame is observed after any native producer submitted, if PSXPORT_FPS60=1 submits rather than refusing, if the guest submit body runs under a path other than Gte, or if FUN_80061308 stops being the retail whole-frame ResetGraph/PutDispEnv/PutDrawEnv/DrawOTag body.

## Re-confirmed 2026-08-21 12:37:53

Final Clang rebuild and bounded captured-PID retail-disc replay scratch/logs/re21-guest-fallback-positive-final.log: Native submitted three dem1 guest frames and then five l1a1 guest frames; every selected/submitted line says nativeSubmitted=0 and interpolation=0, and no FATAL/STUCK/FAULT appeared before the exact-PID termination. Forced-off and FPS60 refusal logs remain the opposite answers.

## Re-confirmed 2026-08-21 12:42:52

Final post-documentation Clang 22 rebuild and all six normal CTests passed; the only source changes after the captured-PID retail replay were comment corrections describing the mutually-exclusive ownership. The final live semantic evidence remains scratch/logs/re21-guest-fallback-positive-final.log plus the DISABLED and INTERPOLATION_FORBIDDEN controls.

## Re-confirmed 2026-08-21 12:45:07

Final post-ownership-gate captured-PID retail replay scratch/logs/re21-guest-fallback-ownership-final.log submitted three dem1 and five l1a1 guest frames. Every line records nativeEnvelopeDelta=0, nativeSubmitted=0 and interpolation=0; the shipped seam now mechanically aborts if FrameEnvelope advanced during a fallback-selected call.

## Re-confirmed 2026-08-21 12:52:52

Post-landing guest_frame_fallback test passed; live Native-path evidence retained nativeSubmitted=0, nativeEnvelopeDelta=0, interpolation=0, while disabled and FPS60 controls refused with the named opposite answers.

## Re-confirmed 2026-08-21 14:15:27

Final psxport 3418a79b Clang rebuild passed guest_frame_fallback and all five other CTests. Bounded retail Native log scratch/logs/re21-guest-fallback-3418a79b.log submitted six dem1 and two l1a1 guest frames; every SELECTED/SUBMITTED record says nativeSubmitted=0, nativeEnvelopeDelta=0, and interpolation=0. The run crossed the repaired FIFO 70 + controller-zero 434 DMA boundary and only later reached the separately catalogued FPS60 queue overflow; DISABLED, INTERPOLATION_FORBIDDEN, and NATIVE_OVERLAP_FORBIDDEN remain the opposite-answer gates.
