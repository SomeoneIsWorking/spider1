---
id: 16
title: first Dem1 mesh has no renderer-time raw texture source
status: resolved
symptom: a native producer can resolve the live Dem1_G mesh but the raw bytes that uploaded its texture and CLUT no longer belong to a live allocation at face submission
tags: render,re-21,texture,asset,lifetime
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

`FUN_80069A60` builds `<name>.psx`, allocates and reads it, and calls `FUN_80068BB0`; the parser
registers palettes through `FUN_80062BB4`, sends image records through `FUN_80081C50`, and trims
the allocation through the allocator whose size header is `word[-1] >> 4`. A fresh forced-Cross
reference-leg run with `PSXPORT_DEBUG=meshprobe,assetprobe,cd` recorded all four boundaries in
`scratch/logs/re21-asset-owner-live-final.log`:

- CD.WAD LBA 8907 loaded `Dem1_L.psx` at `0x801539D4`; its parser uploaded the face's exact
  `(525,246 2x8)` texture from `+0x2B30` and `(544,3 16x1)` CLUT from `+0x38`.
- That allocation was 12,288 bytes while parsing and 48 bytes afterwards. Both source offsets are
  therefore outside the retained range when the face runs; the probe reports `sourceBytesLive=no`.
  The allocator then places `henchman.psx` at `0x80153A0C`, exactly the old CLUT source and with a
  retained range that also covers the old texture source. Those addresses already name different
  asset bytes by render time.
- CD.WAD LBA 8903 loaded `Dem1_G.psx` at `0x8018BB84`; it retained 7,836 bytes, so mesh
  `0x8018BC38` is the live authored record at offset `0xB4`.
- The first face sees both asset slots live. The same run later observes `FUN_800695D0` unload both,
  which establishes the other lifetime boundary rather than inferring it from a quiet slot.

The ledger's selftest supplies the opposite answer: an exact target selects the later owner, while
a one-word target perturbation returns MISSING; unloading changes the same slot from live to dead.

## What was tried / dead ends

Do not retain the raw `Dem1_L.psx` pointers in a renderer or rediscover them by reading VRAM. Their
lifetime ends during parsing by retail design. The correct seam for future native texture ownership
is the asset load/parse boundary, while the retained `Dem1_G` mesh remains a render-time input.

## Resolution

### Resolution (2026-08-21)
The retail FUN_80069A60 -> FUN_80068BB0 loader owns both sampled uploads: Dem1_L.psx uploads the 2x8 texture from raw offset 0x2B30 and the 16x1 CLUT from 0x38, then the parser trims its allocation from 12,288 to 48 bytes before the first face and henchman.psx reuses that released range. Therefore both raw source ranges are intentionally transient; capture texture ownership at load time. The live mesh itself is retained at Dem1_G.psx+0xB4 until the level unload.
