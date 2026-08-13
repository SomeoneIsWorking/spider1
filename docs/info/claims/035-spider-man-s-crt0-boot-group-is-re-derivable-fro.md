---
id: C035
kind: claim
status: holds
created: 2026-08-12
tags: boot,crt0,heap,re-01
depends: game/core/game_config.cpp,external/psxport/runtime/recomp/crt0_boot.h
---

## Claim

Spider-Man's crt0 boot group is re-derivable from SLUS_008.75's own instruction stream — 8 of 8 fields, stackBias -8, and both heap globals present — matching what game_config.cpp shipped by hand

## Evidence

Measured 2026-08-12: `$PSX/psxport/build/tools/crt0_extract scratch/bin/SLUS_008.75` -> exit 0, '35 instruction(s) decoded, stopped on "jal (libcInit)", 3 zero word(s) in the window, prologue COMPLETE (reached the jal)', '8 of 8 field(s) resolved'. Derived: bssZeroLo 0x800B5994, bssZeroHi 0x800C65D4, stackTopBase 0x800B3E70, stackBias -8, stackTopBase2 0x800B3E6C, heapBase 0x800C65D4, gp 0x800B47F4, libcInit 0x8008DC98, heapSizePtr 0x800B1240, heapBasePtr 0x800B123C, plus 'libcInit is the A(39h) InitHeap BIOS thunk: YES . a1 live at the call: YES . delay slot is addi a0,a0,4: YES'. Every address equals what game/core/game_config.cpp:37-45,454 already shipped from a hand disassembly whose instruction addresses are in the comment block at lines 25-32 — two independent derivations agreeing, so the extractor is not echoing this file (it does not read it). The same crt0_scan now runs inside every port boot and refuses a confirmed disagreement. Runtime verified on the pinned psxport 553f0929: `python3 tools/gate.py boot` reported the audit at guest crt0 0x8008739C as `10 field(s) AGREE, 0 DISAGREE, 0 unresolved`, including the InitHeap/thunk, live-a1 and delay-slot checks; the same run advanced to frame 6778 and 2048 submitFrame calls.

## What would falsify it

crt0_extract resolving fewer than 8 fields, reporting a prologue that is not COMPLETE, or disagreeing with game_config.cpp on the same SLUS_008.75; or crt0_audit refusing a boot, which would mean the static derivation and the running guest disagree. The claim does NOT pin the executable by hash, so a re-extraction from a different disc region would falsify it silently.
