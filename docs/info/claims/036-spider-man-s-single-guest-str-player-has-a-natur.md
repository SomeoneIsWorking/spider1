---
id: C036
kind: claim
status: holds
created: 2026-08-14
tags: fmv,input,skip,static-re
depends: game/recomp_seeds.json
---

## Claim

Spider-Man's single guest STR player has a natural edge-triggered Start/Cross exit through its full teardown, and all statically identified boot and table-driven cinematic call sites use that player

## Evidence

STATIC RE ONLY, 2026-08-14; no runtime input/skipping run was performed. Ghidra decompilation and the generated substrate agree: FUN_8006B514 converts the stock libpad packet to active-high button state and calls FUN_8006B208(&DAT_800A4ED4, state & 0x800); FUN_8006B208 raises byte +1 only on up-to-down, making DAT_800A4ED5 the Start pressed edge (raw PSX Start is 0x0008 before the packet-byte ordering). FUN_8002AA0C clears DAT_800A4ED5/DAT_800A4E25 on entry, calls FUN_8006B514 per decoded frame, ignores/clears both during its first 30 ticks, then branches on either edge to LAB_8002AF90, the same teardown reached by natural completion/error: stop both audio channels, tear down STR/MDEC state and buffers, restore display state, return. FUN_8006BF9C calls IDs 0 and 1 for boot and suppresses ID 1 if Start remains held after ID 0. FUN_8006BE28 calls the queued movie ID at DAT_800B4E98. Overlay dispatcher FUN_80010080 maps event cases to movie IDs 3..23. Ghidra xrefs reports 5 call instructions to FUN_8002AA0C, all accounted for by those three owners (two boot calls, one queued call, two dispatcher calls). This proves guest control-flow ownership, not runtime responsiveness, timing, A/V sync, or in-engine non-STR sequence skipping.

## What would falsify it

a retail-code call to a different STR player, a movie descriptor/call path that bypasses FUN_8002AA0C, runtime evidence that Start does not produce DAT_800A4ED5 at the player, or teardown state after a Start exit differing from natural LAB_8002AF90 completion
