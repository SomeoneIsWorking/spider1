---
id: 31
title: A CD-read overlay is jumped into with no published code image, so the run stopped at 0x8014D5AC
status: resolved
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

## Fix

The one owner is psxport `runtime/psx/core/guest_code_module.*`: `psx::code_module::publishStockReadLanding`
publishes each whole stock CdRead as an image named by the SHA-256 of its bytes, one new generation per
read, so an overlay replaced in the same arena stops resolving as the old one. Spyro's copy
(`stock_read_publication.*`, `image_publication.*`) moved there and Spyro 2/3 and `ArchiveTransfer`
consume it. The older window form (`publishGuestCodeModuleLanding`, C-12) keeps its first-landing identity
and now shares the digest and activation.

The arena comes from the guest's own loader. `FUN_8001B990` allocates `NAME.bin` with `FUN_800651C8` from
heap 1, reads it with CdRead through `FUN_80064DA4` (state 2, `FUN_80089ECC`) and relocates `NAME.rel`
(`FUN_8001BF58`). Heap 1 is set up by `FUN_8006BF9C` from the descriptor at `0x8009C5B8`: heap 0 is
`[0x800C65D4, 0x800C65E4)`, heap 1 runs from `0x800C65E4` to the stack guard `0x801FE000`.
`Spider1Runtime::stockCdReadLanded` passes that arena (`overlayHeapArena`). Both measured reads (LBA 8696 to
`0x8014C788`, LBA 10582 to `0x8014D5AC`) lie inside it.

Tests: psxport `test_guest_code_module_publication` (inside, outside, straddling, repeated, replaced by
different bytes, empty arena, unpublishable landing); Spyro's `stock_read_publication` still passes on the
shared owner.

## Projection abort (S008)

`Spider1Widescreen::publishProjection` read the viewport record from the cell `0x800B5918`, but
`FUN_80075D0C` stores its `$a1` argument into that cell itself, so the first call finds it zero. It is not a
widescreen defect: the 4:3 run (`aspect=0`) aborted identically. The record is now taken from `$a1`;
test `the_first_publication_takes_the_record_from_its_argument` covers both aspects with the cell zero (the
old fixture pre-wrote the cell and hid the defect).

## Result

The run passes the overlay, presents the legal screen (512x240, 100%), the menu and a 3D attract demo at
4:3 and 16:9 with no abort. Coverage is in issue 0025.

## Landed alongside the diagnosis

A full turn budget with no VSync or stream boundary now delivers one display field
(`Spider1FrameDriver::deliverBootstrapWaitField`); without it the post-logo wait
(`FUN_8006bf9c`, 300 field-callback ticks) and the overlay mode loops never advance the counter at
`0x800B5468`.
