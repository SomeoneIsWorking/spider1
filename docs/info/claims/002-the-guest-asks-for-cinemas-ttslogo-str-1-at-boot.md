---
id: C002
kind: claim
status: holds
created: 2026-07-30
tags: RE-07
---

## Claim

The guest asks for \CINEMAS\TTSLOGO.STR;1 at boot and that file is NOT on the retail USA disc; the executable names three logo movies and the disc ships two (ATVILOGO.STR, LOGO.STR). Real PSX hardware also fails this lookup, so the game has a working not-found path that the port is not taking.

## Evidence

strings scratch/bin/SLUS_008.75 | grep -i 'logo\|cinemas' -> 24 \CINEMAS\*.STR;1 literals including TTSLOGO. PSXPORT_DISC=<disc> external/psxport/build/tools/discdump list -> CINEMAS/ contains ATVILOGO.STR, LOGO.STR and L1M1..L8M5, no TTSLOGO.STR. The framework HLE is faithful: cd_searchfile_native returns V0=0 on a miss (cd_override.cpp:383).

## What would falsify it

if a different disc revision of the USA release ships CINEMAS/TTSLOGO.STR, or if the TTSLOGO literal turns out to be reached only by a code path the boot never takes (check the caller before relying on this)
