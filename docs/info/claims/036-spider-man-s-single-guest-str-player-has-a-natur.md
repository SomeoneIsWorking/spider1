---
id: C036
kind: claim
status: holds
created: 2026-08-14
tags: fmv,input,skip,static-re,runtime
depends: game/recomp_seeds.json,game/core/str_skip_oracle.cpp
---

## Claim

Spider-Man's single guest STR player has a natural edge-triggered Start/Cross exit through its full teardown, and all statically identified boot and table-driven cinematic call sites use that player

## Evidence

Ghidra decompilation and the generated substrate establish the control-flow foundation. `FUN_8006B514` converts the stock libpad packet to active-high button state and calls `FUN_8006B208(&DAT_800A4ED4, state & 0x800)`. `FUN_8006B208` raises byte +1 only on an up-to-down transition, making `DAT_800A4ED5` the Start pressed edge. `FUN_8002AA0C` clears stale Start/Cross edges on entry, polls once per decoded frame, clears both during its first 30 ticks, and sends either later edge to `LAB_8002AF90`, the common full teardown. Its five static calls are accounted for: boot owner `FUN_8006BF9C` calls IDs 0 and 1, queued owner `FUN_8006BE28` calls `DAT_800B4E98`, and overlay dispatcher `FUN_80010080` maps event cases to IDs 3..23. Boot suppresses ID1 when Start remains held after ID0.

Bounded runtime evidence from the current source exercises the shipping player and pad path. In `scratch/logs/str_skip_start_current.log`, Start produced a guest edge after the guard at tick 31 for boot ID0 and the call reached `LAB_8002AF90` with active state cleared (1/1); the retained Start-held byte then made the retail boot owner omit ID1. In `scratch/logs/str_skip_cross_current.log`, Cross reached boot IDs 0 and 1 (2/2), but only ID0 produced the post-guard edge (1/2). The `0x8002AEF8` checkpoint is reported as `guard_arm` because it precedes the two clearing stores. Queued calls are **MISSING CORPUS: 0 invocations; no verdict**.

This evidence does not establish ID1 responsiveness, queued-movie responsiveness, menu arrival, timing, A/V sync, or skipping of non-STR in-engine sequences.

## What would falsify it

A retail-code call to a different STR player; a movie descriptor or call path that bypasses `FUN_8002AA0C`; runtime evidence that Start does not produce `DAT_800A4ED5` at the player; or teardown state after a Start exit differing from natural `LAB_8002AF90` completion.
