---
id: C049
kind: claim
status: holds
created: 2026-08-22
tags: issue-0018,allocator,fmv,vlc
depends: game/core/allocator_audit.cpp#AllocatorAudit::observeStore, game/core/module_loader.cpp
---

## Claim

Issue 0018's allocator fault is downstream damage from FUN_8002A338 exceeding its first 0x25800-byte VLC output buffer

## Evidence

On clean psxport 57a17a14, scratch/logs/gate-boot-20260822-190346.log stops at the first watched free-node write: FUN_8002A338's SH at guest 0x8002A478 writes 0x0401 to 0x801664E4. FUN_8002AA0C allocated the first VLC output at 0x800FDAC4 with size 0x25800 and passed it through gp+0x6DC/index 0, so the watched address is 0x68A20 bytes from that start and the decoder had already crossed the allocation end 0x801232C4. Ghidra's saved-project query shows retail data 0x80097D84=0x00FFFFFF and exactly one xref, the bound read in FUN_8002A338; no runtime writer changes it. The earlier 0x04010401 traversal word is two consecutive copies of the caught VLC halfword.

## What would falsify it

falsified if a matched failing run shows FUN_8002A338 started with an output pointer other than gp+0x6DC[index 0], or if a store before the decoder crosses 0x801232C4 independently corrupts the allocator link
