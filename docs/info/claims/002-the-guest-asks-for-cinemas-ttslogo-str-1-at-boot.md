---
id: C002
kind: claim
status: falsified
created: 2026-07-30
tags: RE-07
falsified_on: 2026-07-30
---

## Claim

The guest asks for \CINEMAS\TTSLOGO.STR;1 at boot and that file is NOT on the retail USA disc; the executable names three logo movies and the disc ships two (ATVILOGO.STR, LOGO.STR). Real PSX hardware also fails this lookup, so the game has a working not-found path that the port is not taking.

## Evidence

strings scratch/bin/SLUS_008.75 | grep -i 'logo\|cinemas' -> 24 \CINEMAS\*.STR;1 literals including TTSLOGO. PSXPORT_DISC=<disc> external/psxport/build/tools/discdump list -> CINEMAS/ contains ATVILOGO.STR, LOGO.STR and L1M1..L8M5, no TTSLOGO.STR. The framework HLE is faithful: cd_searchfile_native returns V0=0 on a miss (cd_override.cpp:383).

## What would falsify it

if a different disc revision of the USA release ships CINEMAS/TTSLOGO.STR, or if the TTSLOGO literal turns out to be reached only by a code path the boot never takes (check the caller before relying on this)

## FALSIFIED 2026-07-30

The DISC FACT stands (TTSLOGO.STR is a literal in the exe and is not on the retail disc), but the INFERENCE drawn from it -- 'the game has a working not-found path that the port is not taking' -- is false. Disassembly of the caller at 0x8002A884 shows a boot-time pre-scan over all 24 FMV table entries (stride 0x18, base 0x80097DEC): jal CdSearchFile at 0x8002A8B4, then beqz $v0 -> 0x8002A8D4 'sw $zero, ($s0)'. A miss is EXPECTED and HANDLED -- the entry's LBA field is simply set to 0. The port's cd_searchfile_native returns V0=0, so the guest takes exactly that path correctly. The framework logging the miss at ERROR level is what made it look causal. The MDEC garbage decode is a SEPARATE defect and must be re-attributed.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
