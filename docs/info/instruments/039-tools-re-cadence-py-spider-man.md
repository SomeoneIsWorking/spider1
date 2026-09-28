---
id: I039
kind: instrument
status: trusted
created: 2026-09-28
---

## Instrument

tools/re_cadence.py — Spider-Man 1's per-frame field count

## Validated by

SHA-1-bound measurement of SLUS_008.75 (scanned 186,880 words). Establishes that the game does not
pace through VSync at all: its field wait is `FUN_8005E748(n)`, spinning on `[gp+0x0C74]`, which
has **exactly one writer** adding exactly `+1`; the VBlank handler at `0x8008C2E8` adds 1 to
`0x800B397C` per field, which makes the unit a field by the game's own construction; all 15
`jal FUN_8005E748` sites pass the literal 1. Establishes the `n >= 2` rule on this image (VSync at
`0x80084C38`, found by shape: 23 `bgez $a0` candidates, matched 1). Establishes the VSync call
census **including its zero**: 0 direct `jal` sites, 0 `lui`/`addiu` materialisations and 0 stored
pointers naming `0x80084C38` anywhere in the file. And establishes the LIMIT: one of the frame
body's two wait sites is inside a back-edge loop whose trip count a `DrawSync(1)` GPU fence
decides, so the per-frame field count is not a compile-time constant. Selftest 7/7, including a
**discriminator**: the same tool on the same bytes with the loop removed reports a definite 3
fields/frame, so the refusal is a finding about the bytes and not a broken counter. Registered as
CTest `spider1_cadence_selftest` and `spider1_cadence`, both observed green.

## Known failure modes

This instrument's ANSWER is a refusal, which is the failure mode it exists to avoid shipping. The
discriminator case is what keeps the refusal honest: without it, "not established" is
indistinguishable from a counter that cannot count.

Two defects were found and fixed IN this instrument, both of which made a loop disappear and yield
a confident, smaller, wrong total:

- the inner-loop scan looked only at `j`/`jal`, so the **conditional** back-edge
  `bnez $v0,0x8002C284` read as straight-line code;
- the branch target was computed as `((disp & 0xFFFF) << 2)` with **no sign extension** and then
  OR-ed under `0xF0000000` instead of added to `PC+4`, so `0x1440FFF9` became `0xFFFFFFE4` —
  outside the image — and no backward branch was found at all. The formula now delegates to
  `tools/probe_mips_image.py`, which owns it.

The frame loop's HEAD is not its entry: `0x8002C174` runs a prologue and an init block, and the
per-frame body is a back-edge to `0x8002C1E0`. Treating the entry as the head counts the init
block as frame work.

`jalr`'s jump register is in `rs`, not `rd`. Filtering `jalr` sites on `rd == 25` (`$t9`) matches
nothing and reports a clean, uniform `0` — the shape of a scan that has silently found nothing.
This image uses `jalr $ra,$vN` through function pointers and has **zero** `$t9` sites.

Coverage: **0 overlay images are provisioned**, and the image's 346 `jalr` sites are function-
pointer calls that cannot be resolved statically, so "the guest never calls VSync" is a count
over the resident image and not a proof of absence. The stated falsifier is the live measurement
in `docs/issues/0030`.
