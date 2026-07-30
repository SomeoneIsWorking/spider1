---
id: C008
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

0x8002A5F4 is NEVER CALLED during the Spider-Man boot, so demoting it cannot be the RE-16 blocker. The FATAL at 0x080252D4 that survives attempt 13's flat emission has a different, unknown cause.

## Evidence

PSXPORT_DEBUG=coroentry with super-calling probes installed on both 0x8002A338 and 0x8002A5F4 (game/core/diag_overrides.cpp:238, same channel, unconditional ARM line so silence is meaningful): 0x8002A338 reports three fresh-start entries, 0x8002A5F4 reports zero.

## What would falsify it

if a later game phase (field, cutscene) enters 0x8002A5F4, rule 2 becomes live for correctness there -- re-run the probe past the menu before assuming it is dead code
