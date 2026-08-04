---
id: 4
title: Intro logo FMVs never reach the screen: the libstr sector ring deadlocks (StGetNext parks on a slot the producer never marks)
status: resolved
symptom: boots to black screen, no Activision logo; MDEC decodes exactly one macroblock column per movie then stops; StGetNext returns not-ready forever
tags: fmv,str,mdec,cd,black-screen,re-07
created: 2026-08-04
updated: 2026-08-05
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

## Where it deadlocked (superseded — read the corrected root cause below)

The LAST paragraph of this section is WRONG and kept only so the wrong inference is visible: the
ring has 48 slots, so `cons=9` was never past the end. What the measurement really showed is that
slots 1..7 carried a `2` nothing legitimately put there.

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

## Root cause — CORRECTED 2026-08-05, and FIXED

**The attribution below this line replaces the earlier one, which was wrong.** It said the port
pumped sectors "from the CONSUMER side instead of delivering them through the guest's CD data-ready
path". That reads as a design fault in `cd_stream.cpp`, and it is not one: `pumpStream` invokes the
callback the guest registered through CdReadyCallback (0x800B3B18), which during a stream is
`FUN_800860B4 -> FUN_80085000` — libstr's own sector-arrival handler. The port supplies the drive's
PACING and the guest keeps every ring invariant. That division is correct and nothing in
`cd_stream.cpp` was changed to fix this.

The earlier reading also asserted "slot 9 is past the end of the ring". **The ring has 48 slots**
(`DAT_800C1520`, read at runtime — the exact falsifier C016 recorded as open). `cons=9` was well
inside it.

The real cause is one register the framework did not model. `DPCR` (0x1F8010F0) and `DICR`
(0x1F8010F4) were absent from `mem.cpp` — reads returned 0, writes fell through to the stray-I/O
path — so the runtime set `cd.dma_done_pending` on **every** DMA3 completion and the guest's DMA
callback `0x8008DB44` ran once per SECTOR. Ground truth for why that is wrong, all read out of the
image with `tools/ghidra_query.py`:

* libcd `DMACallback` at `0x8009152C`:
  `DICR = (DICR & 0x00FFFFFF) | (1 << (16+ch)) | 0x00800000` — registering a callback arms that
  channel AND the master enable. It goes through the pointer global `0x800B4384`, whose stored value
  is `0x1F8010F4`.
* libstr's DMA starter `FUN_80085948(ch, madr, ?, words, chcr, irq_enable)` read-modify-writes the
  BYTE at `DICR+2` on every transfer — `|= 1<<ch` when this transfer should raise, `&= ~(1<<ch)`
  when it should not — through the pointer global `0x800B0FD4`, stored value `0x1F8010F4`.
* `FUN_80085000` arms it for the **last chunk of an STR frame only**. A 10-chunk frame owes the
  guest exactly ONE completion.

So `0x8008DB44` — which sets `slot[frameStart] = 2` (frame ready) and advances `frameStart` to the
write index — ran ten times per frame instead of once. That is what put status 2 on slots 1..7. The
producer then found `slot[writeIdx=7] != 0` (not free) and refused to fill it, while the consumer,
having legitimately taken and freed a 9-chunk frame at index 0, sat at `cons=9` where nothing would
ever be marked. Both sides were stuck for the honest reason that the ring's state was a lie.

### The fix

`external/psxport/runtime/recomp/dma_irq.h` (NEW) carries DPCR/DICR as pure rules; `mem.cpp` stores
both registers and gates the DMA3 completion signal on `dma_irq_armed(DICR, 3)`; `hle.cpp`
acknowledges the channel flag where it dispatches the guest callback, because that dispatch stands
in for the BIOS DMA handler which would acknowledge first.

Gate: `external/psxport/tests/test_dma_irq_gate.cpp`, 7 cases / 30 checks, hermetic. Shown RED 0/7
against a transcription of the shipped ungated rule — headline failure `signalled == 1: got 20`.

### What it did and did not fix

A/B on ONE binary, same disc, same env, toggling only the gate:

* BEFORE — ring wedged at `frameStart=7 cons=9 writeIdx=7 | status: 0 2 2 2 2 2 2 2 0...`, held over
  42 decimated samples (>= 8.2M StGetNext not-ready calls). Intro presents 0.00% non-black, 1 colour.
* AFTER — the ring never wedges: ONE decimated sample, at `writeIdx=1 status: 3 0 0...` (a DMA in
  flight, normal). `PSXPORT_DEBUG=dmairq` shows 9 chunks `armed=0` then the 10th `armed=1` per frame.
  TWO full STR frames are delivered per movie instead of one. Intro presents 40.62% non-black,
  140 colours.
* Menu unaffected: f2400 = 99.44% non-black / 1120 colours, f4000 = 83.70% / 1315. No abort, no
  recomp-MISS.

**The movies still do not play.** The MDEC trace is byte-identical to before: still exactly one
DecDCTin/DecDCTout pair per movie. See the next section.

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

## RESOLVED 2026-08-05 — the movies play. Two more causes, both real, both now fixed.

Frames read off the PPMs of a 180 s headless run (`PSXPORT_SHOT_AT=120,300,2400,4000,6000,8000`):

    frame  120   320x240 24-bit @0,256   99.95% non-black  11395 colours   ACTIVISION logo
    frame  300   320x240 24-bit @0,256   25.70%             8773           Neversoft eyeball
    frame 4000   512x240                 99.44%             1061           main menu, unchanged
    frame 6000   512x240                 99.44%             1070
    frame 8000   512x240                 99.43%             1105

PNGs: `scratch/screenshots/re07_fmv_120.png`, `re07_fmv_300.png`, `re07_menu_4000.png`.
Log: `scratch/logs/re07b_gate.log` — 0 abort, 0 fatal, 0 recomp-MISS; the single `error` line is the
expected `CdSearchFile: /CINEMAS/TTSLOGO.STR;1 not found` (that file is not on the retail disc).

### Cause 2 — RE-16: the recompiler unwound the bit-stream decoder after one block

`0x8002A338` is libpress's `DecDCTvlc`, hand-written assembly that uses `jal` / `jr $ra` as an
INTERNAL coroutine inside one stack frame. `jal 0x8002A478` at `0x8002A41C` made the emitter promote
that internal block to a FUNCTION entry, splitting the body — so the routine's own loop back-edge,
the unconditional `bgez $zero, 0x8002A478` at `0x8002A43C`, was emitted as `func_8002A478(c);
return;`. The decoder ran one block and returned out of itself, skipping its epilogue.

Measured live, with a fntrace ABI check validated BOTH ways (`PSXPORT_FNTRACE_SELFTEST=1` makes the
control leaves `0x8008735C` / `0x8008710C` report a violation; without it they report 0 over 4 and 49
calls, so the checker is not blanket-reporting):

    BEFORE  0x8002A338  4 calls, 8 ABI violations — sp 807FFF00 -> 807FFEFC, ra -> 8002A424
            0x8002B430  44 calls, 12 violations   — downstream damage from the leaked 4 bytes
            DecDCTin    2 calls in a whole run
    AFTER   0x8002A338  423 calls, 0 violations; 0x8002B430 4653, 0; DecDCTin 421

The ABI lead recorded below as "untriaged, instrument suspect" was therefore REAL. So was the
attribution: `0x8002B430`'s violation is entirely explained by its callee leaving sp 4 low, so its
own epilogue reloaded ra/s0 from the wrong stack words.

The fix is `demote_internal_labels`, which already existed (psxport 77386b68), was unit-tested, and
was **never called from `emit_module`** — dead code for a week. Wiring it needed two companions: an
intra-function `jal` must emit `$ra = <ret>; goto L_<target>;` (its target is no longer a function),
and the matching `jr $ra` must become a SWITCH over this body's own link constants — the revert of
`d2d99ff7` is undone, and its router dispatch replaced by that switch, which is why the earlier
attempt's `recomp-MISS 0x03FF03FF` cannot recur. On the real executable the emitter now reports
exactly one demotion (`0x8002A478`) and exactly one computed `jr $ra` (`0x8002A460`, cases
`0x8002A424 / 0x8002A708 / 0x8002A774`).

With cause 2 alone the player loops (421 decodes) and the display finally switches into the movie's
own 24-bit 320x240 @ (0,256) mode — but the picture is still 0.00% non-black, because of:

### Cause 3 — only channel 3's DMA completion was ever announced to the guest

The player registers an MDEC-OUT completion callback at init: `0x8002B1FC` calls
`FUN_80085BC0(0x8002B28C)` -> `DMACallback(1, ...)` through the BIOS thunk `0x8008B89C`, which stores
it into libcd's per-channel table at `0x800B4388 + 4*ch`. `0x8002B28C` is what LoadImages each
decoded 16-px strip into VRAM and advances the destination x at `gp+0x6D4` — the very value the
player's `dsx` wait at `0x8002AD64` spins on. The framework signalled channel 3 only, so:

* `PSXPORT_FNTRACE=0x8002B28C` -> **NEVER CALLED**, over a whole run, while its registrar ran 4 times
  and the movie loop pumped 4653 iterations. That is the third gate, and it is why the dsx wait was
  one of the two surviving suspects: with the callback dead, `gp+0x6D4` never moves, so before RE-16
  the wait was trivially satisfied and after it the loop just spun.
* The guest's own DICR is `0x009A0000` — master plus channels 1, 3 and 4. It asked for all three.

Fixed by making the dispatch per-channel: `GameConfig::cdDmaDoneCbPtr` (one slot) becomes
`dmaCallbackTable` (the table base), `DmaDone` in `dma_irq.h` is a per-channel pending SET, every one
of `mem.cpp`'s six completion points signals through the same DICR gate, and `Hle::irqPoll` scans all
seven channels.

### Cause 4 — a deferred completion was consumed without being delivered

These callbacks CHAIN: `0x8002B28C` starts the next strip's `DecDCTout`, and that transfer finishes
while the handler is still running, so the second completion always lands with `in_irq` set. The
first version took the pending flag and *then* declined the dispatch, dropping it — the chain died
after two strips per movie and the decoder was left holding 1640 abandoned input words
(`scratch/logs/re07b_fix3c.log`: `owed ch1 -> callback 8002B28C  DEFERRED: a guest callback is
running`). A completion is now consumed only when it is delivered, and the deferred-work gate stays
armed while any channel is owed. `0x8002B28C` calls: 0 -> 2 -> **8420**.

## The SECOND blocker (historical — this is what cause 2 turned out to be)

The player is `FUN_8002AA0C` (`gp = 0x800B47F4`). Its `while(true)` body is rotated: the loop-back
target is the frame wait at `0x8002AD38`, and DecDCTin (`0x80085B24`) + DecDCTout (`0x80085BA0`) sit
at the END of the iteration, `0x8002AF20` / `0x8002AF58`. Each iteration decodes ONE 16-px-wide
strip: `words = bpp * height / 2` (`gp+0x6D8` = 16 or 24 bpp, `gp+0x6DA` = height) — 2880 for
320x240 @24bpp, 2304 for 320x192.

It runs ONE iteration per movie and then leaves via a `goto LAB_8002AF90`, **not** the normal
end-of-movie break: `PSXPORT_FNTRACE=0x8002B18C` reports that function NEVER reached over a whole
run, while `0x80085B24`, `0x80085BA0`, `0x8002B430`, `0x8002A338` all are.

Four `goto LAB_8002AF90` paths exist. Two are RULED OUT by watchpoint, each with its denominator —
every store to the address over an entire run, not a sample:

* `gp+0x68C` (0x800B4E80), the abort flag — `PSXPORT_WWATCH=0x800B4E80,0x800B4E84`: every store 0.
* `DAT_800A4ED5`, the pad-skip flag — `PSXPORT_WWATCH=0x800A4ED4,0x800A4ED8`: every store 0.

The two left are the frame-wait timeout and the `dsx` wait (`gp+0x6D4` vs `gp+0x6C2`), both counting
down from 0x800000. `FUN_8008710C(s_dsx___d, ...)` is the guest's own printf on the dsx path and is
the cheapest next discriminator — check whether it is reached with an `ra` inside 0x8002AD38..0x8002AF1C.

Note also that the intro's display area is **512x1** in this build (`shot_30..300` are 512x1), so a
decoded movie would still have nowhere to land — see issue 0002. That is a third, independent gate
between "the movie decodes" and "the user sees a logo".

## Untriaged lead, recorded rather than chased: an ABI violation IN the movie loop's callees

The same `PSXPORT_FNTRACE` run that proved `0x8002B18C` is never reached also reported:

    0x8002A338 VIOLATES THE ABI  — sp entered 807FFF00, returned 807FFEFC (an epilogue did not run)
                                   ra entered 8002B468, returned 8002A424
    0x8002B430 VIOLATES THE ABI  — s0 entered 80098034, returned 800B4ED0
                                   sp entered 807FFF18, returned 807FFF14
                                   ra entered 8002AD20, returned 80098034

`FUN_8002B430` is the ring consumer the movie loop calls, and `FUN_8002A338` is the VLC decode
inside it. If those violations are real, the loop's own locals and counters are being corrupted by
its callee — which would explain a loop that exits after one iteration for no reason visible in the
guest source, and would put the cause in the RECOMPILER, not the game.

**Not yet trusted, because the instrument is a suspect here.** fntrace uninstalls itself,
re-dispatches, and reinstalls; that dance is exactly the kind of thing that can perturb `sp`/`ra`
itself. Validate it first by tracing a function known to be well-behaved (a leaf with a plain
prologue/epilogue) and confirming it reports NO violation, then re-read these two. If it is a real
recompiler defect it is likely the same family as RE-16.

### Note (2026-08-05)
SCOPE CORRECTION 2026-08-05 — this entry's 'RESOLVED: the movies play' is TRUE HEADLESS and FALSE WINDOWED. Every number in it (f120 99.95% non-black / 11395 colours, f300 25.70% / 8773, menu 99.44%) was taken under PSXPORT_VK_HEADLESS=1. Windowed, the same build is 0.00% / 1 colour to f2400 with vram_writes=0 over 4027 presents. THE FOUR CAUSES DIAGNOSED AND FIXED HERE ARE ALL REAL AND NONE OF THEM IS RETRACTED — the DICR gate, RE-16's DecDCTvlc unwind, per-channel DMA completions, and the dropped chained completion. They are exactly what makes the movie appear the moment the guest is given CPU. A FIFTH, INDEPENDENT blocker sits downstream of all of them: gpu_vk.cpp:498 never calls SDL_SetGPUSwapchainParameters, so the swapchain defaults to VSYNC and SDL_WaitAndAcquireGPUSwapchainTexture blocks the GUEST thread (there is no I/O thread). Proven by a zero-code control — windowed + MESA_VK_WSI_PRESENT_MODE=immediate gives vram_writes=11076 and the logos appear. FULL WRITE-UP: issue 0005. Claims: C019 falsified, C020 (headless-scoped re-issue), C021 (root cause). RE-07 downgraded re-verified -> re-partial. Instruments that hid this: INST-18 / I013 (PSXPORT_SHOT_AT reads guest VRAM, never the swapchain) and INST-19 / I014 (the watchdog is petted from gpu_present_ex, so it cannot see guest starvation). This entry stays RESOLVED because the deadlock it names IS fixed; the black screen the user sees is issue 0005.
