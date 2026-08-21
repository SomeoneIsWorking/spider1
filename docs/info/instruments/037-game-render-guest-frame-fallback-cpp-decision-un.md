---
id: I037
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

game/render/guest_frame_fallback.cpp decision + unconditional [guestfallback] live submission evidence

## Validated by

TWO-CLASS and safety-refusal validation, 2026-08-21. The production decision test returns SUBMIT_GUEST_FRAME for enabled/no-native/no-interpolation and produces distinct DISABLED, NATIVE_OVERLAP_FORBIDDEN, and INTERPOLATION_FORBIDDEN answers. On the retail disc, enabled Native emitted SELECTED then SUBMITTED and advanced dem1 -> l1a1. Final `scratch/logs/re21-guest-fallback-ownership-final.log` carries `nativeEnvelopeDelta=0` on eight consecutive named-scene submissions; render_seam aborts before the super-call if the only current native producer's counter did advance. The same executable with the CVar forced off aborted at dem1 with DISABLED; PSXPORT_FPS60=1 aborted at dem1 with INTERPOLATION_FORBIDDEN. Counters increment outside logging and early lines state nativeSubmitted=0/interpolation=0, so silence is not counted as success.

## Known failure modes

(none recorded yet)
