---
id: C004
kind: claim
status: holds
created: 2026-07-30
tags: RE-03c
---

## Claim

The MDEC decode wedge is a DMA-MODEL defect, not a decoder defect: mem.cpp runs each DMA transfer synchronously and atomically on the guest's CHCR write, so DMA0(in) and DMA1(out) can never be in flight together, and the decoder blocks on a full output FIFO. Measured order on this game is DMA0-first, DMA1 only after DMA0 gives up.

## Evidence

PSXPORT_DEBUG=mdecdma: DMA0 32w ok, DMA0 32w ok, DMA0 1824w WEDGES at 6 with outfifo_has_data=1, then DMA1 2880w canread=1. The wedge diagnostic in mdec_beetle.c reports MDEC_DMACanRead() at the wedge and says BLOCKED ON OUTPUT. Reference behaviour is Beetle dma.c ChCan/RecalcHalt: both channels stay PENDING with busy set, gated per block reload on MDEC_DMACanWrite/MDEC_DMACanRead. Guest never programs DICR/DPCR (0 refs in 186880 instructions), so no completion IRQ is involved.

## What would falsify it

if a build with pending-channel DMA still wedges with outfifo_has_data=1, the block is inside the decoder rather than in channel scheduling; also re-check if any game is observed starting DMA1 before DMA0, which would make the latent zero-drain truncation live
