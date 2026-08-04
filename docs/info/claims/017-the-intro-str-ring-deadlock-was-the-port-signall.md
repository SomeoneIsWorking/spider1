---
id: C017
kind: claim
status: holds
created: 2026-08-05
tags: fmv,str,cd,dma,re-07
---

## Claim

The intro STR ring deadlock was the port signalling the guest's DMA-completion callback on every DMA3 transfer; DICR is the gate and it was unmodelled

## Evidence

RE (ground truth from the image, tools/ghidra_query.py): libcd DMACallback at 0x8009152C does DICR = (DICR & 0x00FFFFFF) | (1<<(16+ch)) | 0x00800000 through the pointer global 0x800B4384, whose stored value is 0x1F8010F4; libstr's DMA starter FUN_80085948 read-modify-writes the BYTE at DICR+2 per transfer through the pointer global 0x800B0FD4 (stored value 0x1F8010F4), setting bit ch when this transfer should raise and clearing it otherwise; FUN_80085000 arms it for the LAST chunk of an STR frame only. mem.cpp modelled neither DPCR nor DICR (reads 0, writes dropped) and set cd.dma_done_pending on every DMA3 completion. A/B on ONE binary, same disc, same env, toggling only the gate: BEFORE the ring wedges at frameStart=7 cons=9 writeIdx=7 status '0 2 2 2 2 2 2 2 0...' over 42 decimated samples (>=8.2M StGetNext not-ready calls) and intro presents are 0.00% non-black / 1 colour; AFTER the ring never wedges (1 decimated sample, at writeIdx=1 status '3 0 0...'), PSXPORT_DEBUG=dmairq shows 9 chunks armed=0 then the 10th armed=1 per frame — exactly one completion per frame — and TWO full STR frames are delivered per movie instead of one. Hermetic gate: external/psxport/tests/test_dma_irq_gate.cpp, 7 cases 30 checks, shown RED 0/7 against a transcription of the shipped ungated rule (headline: 'signalled == 1: got 20'). Logs scratch/logs/re07_before.log, re07_after.log.

## What would falsify it

if a run shows a DMA3 completion with DICR channel-3 armed that the guest did NOT want announced, or an STR frame whose last chunk arrives with armed=0 — either would mean the per-transfer DICR byte write is not the discriminator
