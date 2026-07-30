---
id: C005
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

RE-16's stack-leak diagnosis is CONFIRMED LIVE on the healthy (RE-16-shelved) build: 0x8002A338 leaks exactly 4 bytes of guest stack per call. Separately, $ra is a VALID code address at entry (0x8002B468), so the routine inherits a good $ra and is not the victim of an upstream clobber.

## Evidence

PSXPORT_DEBUG=coroentry diagnostic native override on 0x8002A338 (game/core/diag_overrides.cpp, super-calls so the run is unchanged), armed with an unconditional ARM line so silence would be meaningful. Three entries during boot, all fresh-start (a0 != 0): sp=807FFF00, 807FFEFC, 807FFEF8 -- monotonic -4 per call. ra=8002B468 on all three.

## What would falsify it

if a longer run or a different game phase shows sp NOT decreasing per call, the leak is path-dependent rather than unconditional; and if $ra is ever seen non-code at entry, the upstream-clobber theory reopens
