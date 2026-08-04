---
id: C016
kind: claim
status: falsified
created: 2026-08-04
tags: fmv,str,mdec,cd,re-07
falsified_on: 2026-08-05
---

## Claim

The guest's STR sector delivery is byte-correct; the intro FMV fails INSIDE the guest's libstr ring bookkeeping

## Evidence

CdlSetloc 28:32:54 -> LBA 128304 == CINEMAS/ATVILOGO.STR and 62:15:25 -> LBA 280000 == CINEMAS/LOGO.STR, both exact per discdump list. The guest's MDEC DMA0 word counts (1824, 1440) match the two files' BS-header word counts read straight off the disc (ATVILOGO 320x240 bs=1824, LOGO 320x192 bs=1440) exactly. The failure is later: exactly one DecDCTout per movie (2880 and 2304 words = one 16px macroblock column at 24bpp for a 240- and a 192-high frame), then StGetNext parks with cons=9 > prod=7 and slot[9]=0.

## What would falsify it

if the ring state is found to differ across runs in a way that makes cons=9 legitimate — the ring slot COUNT (DAT_800c1520) was never read, so 'past the end' is inferred from cons>prod and slot[9]!=1, not from the ring size

## FALSIFIED 2026-08-05

PARTIALLY WRONG, corrected 2026-08-05. The sector-delivery half HOLDS and is re-verified. The RING half is wrong: its own falsifier fired. DAT_800c1520 (the ring slot count) has now been READ at runtime — it is 48, so cons=9 was NEVER past the end of the ring and 'StGetNext parks on a slot past the end' was a bad inference from cons>prod. The real defect: the runtime signalled the guest's DMA-completion callback (0x8008DB44) on EVERY DMA3 transfer because DICR (0x1F8010F4) was unmodelled, so libstr's frame-ready marker was written once per SECTOR instead of once per FRAME; slots 1..7 carried spurious status 2 and the producer then refused to overwrite slot 7 (not free), which is what wedged it. Superseded by C017.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
