---
id: C018
kind: claim
status: falsified
created: 2026-08-05
tags: fmv,str,mdec,re-07
falsified_on: 2026-08-05
---

## Claim

Fixing the ring deadlock does NOT make the intro movies play — a SECOND, independent blocker follows it in the guest's movie loop FUN_8002AA0C

## Evidence

With the DICR gate in place the ring is healthy and two full STR frames per movie are delivered, but the MDEC trace is UNCHANGED: still exactly one DecDCTin/DecDCTout pair per movie (1824 in / 2880 out, then 1440 in / 2304 out), then 'no decode command in flight' and 1548/1218 input words abandoned. The player loop is FUN_8002AA0C (gp=0x800B47F4); its body at 0x8002AF20/0x8002AF58 issues DecDCTin(0x80085B24)+DecDCTout(0x80085BA0) once per iteration and branches back to the frame wait at 0x8002AD38. It exits via a goto to LAB_8002AF90, NOT the normal break (fntrace: 0x8002B18C NEVER reached over a whole run). Two of the four goto paths are ruled out by watchpoint, each with its denominator: PSXPORT_WWATCH=0x800B4E80,0x800B4E84 (gp+0x68c, the abort flag) — every store over the run was 0; PSXPORT_WWATCH=0x800A4ED4,0x800A4ED8 (the pad-skip flag DAT_800A4ED5) — every store over the run was 0. The two remaining candidates are the frame-wait timeout and the dsx-wait timeout, both counting down from 0x800000.

## What would falsify it

if a run is found where the movie loop performs more than one DecDCTin per movie, this is not a single hard blocker but a timing-dependent one

## FALSIFIED 2026-08-05

PARTLY. Its headline was right — the ring fix alone does not play the movies — but its narrowing was wrong in a way worth keeping. The 'one DecDCTin per movie' was a RECOMPILER defect (RE-16): 0x8002A338, libpress DecDCTvlc, uses jal/jr-$ra as an internal coroutine, its internal block 0x8002A478 was promoted to a function entry, and the routine's own loop back-edge (bgez $zero at 0x8002A43C) was emitted as call+return, so it unwound after one block with sp 4 low. Fixed -> DecDCTin 2 -> 421 per run, 0 ABI violations. The claim listed that lead as 'untriaged, instrument suspect'; the instrument was fine (validated both ways, INST-017) and the lead was the answer. Of its two named suspects the DSX WAIT was the exit, but not for a timing reason: the MDEC-out DMA callback 0x8002B28C that advances gp+0x6D4 was NEVER dispatched, because the framework signalled DMA completions on channel 3 only. Both fixed; both movies now render (issue 0004).

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
