---
id: 31
title: A CD-read overlay is jumped into with no published code image, so the run stops at 0x8014D5AC
status: open
symptom: after the third logo movie the guest loads the overlay at 0x8014C788/0x8014D5AC through stock CdRead and calls into it; the executor refuses the dispatch (claimed by no code image) and the session ends
state_items: S002,S008,S018,S021
tags: overlay,image,cd,dem1,s018
created: 2026-10-09
updated: 2026-10-09
---

## Observed

`[native-dispatch:error] guest address 0x8014D5AC resolves to zero or multiple active code images`,
block began at `0x80064E70`, `ra=0x8001BB20`, `a0=0x80149D34`. The guest had just read 3 sectors from
LBA 8696 to `0x8014C788` and 56 from LBA 10582 to `0x8014D5AC`.

## Cause

`Spider1Runtime` does not implement `GameRuntime::stockCdReadLanded`, so no CD-read landing is ever
published as a code image and Lightrec has no image for the overlay addresses.

## Proper fix

Publish each landed read as an image from `Spider1Runtime::stockCdReadLanded`, as Spyro does
(`spyro/game/core/stock_read_publication.cpp`, `image_publication.*`). That code is title-neutral and
lives in the Spyro repo; Spider-Man must not carry a second copy, so it moves to psxport first and Spyro
and Spider-Man both consume it.

## Measured with a throwaway publisher (not landed)

Publishing every read above `0x80100000` with a placeholder identity let the run continue: the legal
screen (512x240, 100% non-black) presented, the level data loaded (189 sectors from LBA 8714 and more),
and the run then aborted in `Spider1Widescreen::publishProjection` ("published no viewport record at
0x800B5918") on the first guest projection call from `FUN_8002bd5c` (`0x8002BE0C`). That abort is the
next blocker after this one and belongs to S008.

## Landed alongside the diagnosis

A full turn budget with no VSync or stream boundary now delivers one display field
(`Spider1FrameDriver::serviceBootstrapBudgetField`); without it the post-logo wait
(`FUN_8006bf9c`, 300 field-callback ticks) and the overlay mode loops never advance the counter at
`0x800B5468`.
