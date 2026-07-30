---
id: C008
kind: claim
status: falsified
created: 2026-07-30
tags: RE-16
falsified_on: 2026-07-30
---

## Claim

0x8002A5F4 is NEVER CALLED during the Spider-Man boot, so demoting it cannot be the RE-16 blocker. The FATAL at 0x080252D4 that survives attempt 13's flat emission has a different, unknown cause.

## Evidence

PSXPORT_DEBUG=coroentry with super-calling probes installed on both 0x8002A338 and 0x8002A5F4 (game/core/diag_overrides.cpp:238, same channel, unconditional ARM line so silence is meaningful): 0x8002A338 reports three fresh-start entries, 0x8002A5F4 reports zero.

## What would falsify it

if a later game phase (field, cutscene) enters 0x8002A5F4, rule 2 becomes live for correctness there -- re-run the probe past the menu before assuming it is dead code

## FALSIFIED 2026-07-30

SCOPE ERROR, not a bad measurement. The probe result (zero entries) was real, but it was taken on the HEALTHY / pre-fix build, where the RE-16 leak makes 0x8002A338 bail out via call+return after roughly one block -- so the decode loop never runs and the four 'jal 0x8002A5F4' sites at 0x8002A774/77C/784/78C are never REACHED. The claim's conclusion ('so demoting it cannot be the RE-16 blocker') therefore does not transfer to a build where the leak is fixed. Attempt 19 fixed the leak, the decode loop ran, and 0x080252D4 reappeared byte-identically -- exactly what attempt 13 predicted before this claim retired the prediction. Superseded by C010.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
