---
id: 4
title: Intro logo FMVs never reach the screen: the libstr sector ring deadlocks (StGetNext parks on a slot the producer never marks)
status: investigating
symptom: boots to black screen, no Activision logo; MDEC decodes exactly one macroblock column per movie then stops; StGetNext returns not-ready forever
tags: fmv,str,mdec,cd,black-screen,re-07
created: 2026-08-04
updated: 2026-08-04
---

Measured on 5cd7b43 / psxport 86dad4f9 (the build that no longer aborts on recomp-MISS).

## What the guest actually does

Boot sequence, from `PSXPORT_DEBUG=cdcmd,mdecdma,ring` (scratch/logs/cd_probe.log,
mdec_probe.log, ring_probe.log):

1. `CdSearchFile /CINEMAS/TTSLOGO.STR;1` -> not found (expected; not on the retail disc).
2. `CdlSetloc 28:32:54` -> LBA 128304. `discdump list` puts **CINEMAS/ATVILOGO.STR at LBA
   128304**. Exact match. Then `CdlSetmode 0x80`, `CdlSetmode 0xE0` (double speed + XA-ADPCM +
   whole sector), `CdlReadS`, `CdlDemute`. This is textbook STR streaming start-up.
3. MDEC: two 32-word DMA0 table uploads, then `DMA0(in) 1824 words @800FDAC8`.
   Ground truth from the disc: ATVILOGO.STR frame 1 has BS header word count **1824**
   (`bs[0]=0x0720`, magic `0x3800`, q=1, ver=2), 320x240, 10 chunks. **The guest fed MDEC exactly
   the right bitstream length**, so the CD -> ring -> frame-assembly path is byte-correct.
4. `DMA1(out) 2880 words` completes. 2880 words at 24bpp = 15 macroblocks = ONE 16-px-wide
   column of a 240-high frame. Then nothing: the guest never issues a second DecDCTout, and at
   the next attempt force-stops DMA0, abandoning 1548 of the 1824 input words.
5. Same again for `CdlSetloc 62:15:25` -> LBA 280000 = **CINEMAS/LOGO.STR** exactly.
   `DMA0(in) 1440 words` (LOGO.STR's BS header says 1440), `DMA1(out) 2304 words` = 12 MBs =
   one column of a 192-high frame. LOGO.STR is 320x192. Then stops.
6. **Over a 20337-frame headless run there are exactly TWO MDEC decode attempts, both at frames
   3 and 5, and no MDEC traffic ever again.** (Denominator: `grep -c mdecdma` = 24 lines total.)

No large VRAM upload happens anywhere near frames 3-5 (`[gpu]` shows 10 gp0words/frame), so no
decoded video ever reaches VRAM.

## Where it deadlocks

`PSXPORT_DEBUG=ring` (game/core/cd_stream.cpp's diagnostic) after the second movie, held for
millions of StGetNext calls:

    base=0x80139AD4 prod=7 cons=9 d1514=7 | slots: 0 2 2 2 2 2 2 2 0 0 0 0

RE (Ghidra, scratch/ghidra project built from tools/redump_ram.py + tools/ghidra_import.sh):

* `StGetNext` = `FUN_80086b10`. It reads the slot header at `DAT_800c1510 + DAT_800c151c*0x20`.
  Status 1 = wrap marker (resets cons to 0 and re-reads); status 2 = ready (takes it, marks 4,
  returns 0); anything else returns non-zero.
* `StFreeRing` = `FUN_800872ac`. It zeroes `slotHdr[3]` (= chunks-in-frame) slots starting at the
  freed index and sets `DAT_800c151c = index + count`.
* The producer is `FUN_80085000` (the CD sector-arrival handler; checks `*hdr == 0x160`, the STR
  magic). It is the only writer of the wrap marker (status 1).

LOGO.STR frame 1 spans **9 chunks**, so after StFreeRing cons = 0 + 9 = **9**. Slot 9 holds 0,
not 1 and not 2, so StGetNext returns not-ready forever. The producer meanwhile refilled slots
1..7 (status 2) and its index sits at 7 - BEHIND the consumer. The wrap marker that would reset
cons to 0 is never written at slot 9.

## Root cause (named, not yet fixed)

The port pumps STR sectors from inside the CONSUMER (`game/core/cd_stream.cpp` overrides
StGetNext at 0x80086B10 and calls `cd.pumpStream()` when the guest finds nothing ready), instead
of delivering them through the guest's CD data-ready path, which is what runs the PRODUCER
(FUN_80085000) and therefore what maintains the ring's wrap marker and producer index. Pumping
on demand from the consumer side fills slots at a different phase than the producer's own
bookkeeping expects, and the two indices desynchronise. The comment in cd_stream.cpp anticipated
exactly this discriminator ("if they hold 2 the producer is fine and the consumer index is
wrong") - the measurement says slots hold 2, so it is the consumer/producer INDEX relationship
that is broken, not sector delivery.

This is RE-07 (Intro FMV / front-end). The proper fix is to drive the guest's own sector-arrival
handler at drive pace so the producer keeps the ring's invariants, not to patch StGetNext.
DO NOT paper over it by forcing cons to wrap - that is a hack and would corrupt frame assembly.

## Not the cause (ruled out, with evidence)

* Not the renderer. The main menu renders correctly at frame 2400 (99.44% non-black, 1048
  colours) and the game keeps running to 20337+ frames.
* Not sector delivery / CD seeking. Both movies' LBAs match the ISO exactly and both frames'
  bitstream lengths match their BS headers exactly.
* Not `PSXPORT_NO_FMV` / headless FMV skipping. With `PSXPORT_NO_FMV=0` the log says
  "no boot FMV configured (GameConfig::bootFmv is empty) - nothing to play" and the MDEC trace is
  byte-identical (same madr, same 1824/2880 and 1440/2304 word counts). The framework FMV player
  is not in this port's path at all; the A/B is a provable no-op.
* Not the 512x2 display area - see issue 0002.
