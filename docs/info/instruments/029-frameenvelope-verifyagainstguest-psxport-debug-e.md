---
id: I029
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

FrameEnvelope::verifyAgainstGuest (PSXPORT_DEBUG=envcheck) — the port's envelope words vs libgpu's own DR_ENV packet, every frame

## Validated by

WHAT IT ANSWERS: does the port's ported PutDrawEnv arithmetic produce the SAME GPU words libgpu produces from the same DRAWENV. Runs only on the reference leg, after the super-call, because that is the only place and time the guest's own packet for THIS frame exists. BOTH CLASSES OBSERVED 2026-08-06: NEGATIVE-CAPABLE proven by perturbation — one bit changed in DrawEnv::drawOffsetWord and mismatch tracked checked exactly (512/512, 1024/1024, 1536/1536), with the per-word grid naming E5 as the differing word; POSITIVE on the clean build, checked=2560 mismatch=0. It also caught its own ORDERING BUG: placed before the super-call it reported 6/1024 mismatches against a two-frames-stale packet. THE NEGATIVE IS DESIGNED: the periodic line always carries checked= next to mismatch=, so 'mismatch=0 checked=0' (never ran) cannot be read as 'mismatch=0 checked=2560' (verified), and the guest packet's own word-count byte is compared so a length disagreement is as loud as a value disagreement. BLIND SPOTS: it compares only the DRAWENV half — the GP1 words PutDispEnv emits have NO guest-side artefact in RAM to compare against, so displayStartWord/displayModeWord/verticalRangeWord are unverified by this instrument (GP1(08) has independent corroboration from the framework's own [gpu] display line; GP1(05) and GP1(07) have none). It also cannot see a word the port emits and the guest does not, or vice versa, beyond the count check.

## Known failure modes

(none recorded yet)
