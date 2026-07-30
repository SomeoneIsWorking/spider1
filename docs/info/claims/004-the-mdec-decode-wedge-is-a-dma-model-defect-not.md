---
id: C004
kind: claim
status: falsified
created: 2026-07-30
tags: RE-03c
falsified_on: 2026-07-30
---

## Claim

The MDEC decode wedge is a DMA-MODEL defect, not a decoder defect: mem.cpp runs each DMA transfer synchronously and atomically on the guest's CHCR write, so DMA0(in) and DMA1(out) can never be in flight together, and the decoder blocks on a full output FIFO. Measured order on this game is DMA0-first, DMA1 only after DMA0 gives up.

## Evidence

PSXPORT_DEBUG=mdecdma: DMA0 32w ok, DMA0 32w ok, DMA0 1824w WEDGES at 6 with outfifo_has_data=1, then DMA1 2880w canread=1. The wedge diagnostic in mdec_beetle.c reports MDEC_DMACanRead() at the wedge and says BLOCKED ON OUTPUT. Reference behaviour is Beetle dma.c ChCan/RecalcHalt: both channels stay PENDING with busy set, gated per block reload on MDEC_DMACanWrite/MDEC_DMACanRead. Guest never programs DICR/DPCR (0 refs in 186880 instructions), so no completion IRQ is involved.

## What would falsify it

if a build with pending-channel DMA still wedges with outfifo_has_data=1, the block is inside the decoder rather than in channel scheduling; also re-check if any game is observed starting DMA1 before DMA0, which would make the latent zero-drain truncation live

## FALSIFIED 2026-07-30

PARTIALLY falsified -- the DMA-model root cause and the fix are CORRECT and verified (mdec errors 2 -> 0, DMA1 drains in full, selftest validated against both classes). What is falsified is the DOWNSTREAM attribution built on top of it: I claimed RE-16's coroutine fix was blocked on RE-03c because the bit-stream decoder was reading its saved continuation out of an incomplete decode. Tested directly: with RE-03c fixed (mdec:error = 0 in the same run) and the RE-16 change reapplied, the fault is UNCHANGED -- ra=0x03FF03FF, c->pc=0x8002A478, FATAL at 0x080252D4. So the garbage continuation does not come from an incomplete MDEC decode. Second wrong blocker attribution for RE-16 (RE-07 was the first).

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
