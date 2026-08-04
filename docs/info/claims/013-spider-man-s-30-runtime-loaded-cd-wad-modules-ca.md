---
id: C013
kind: claim
status: holds
created: 2026-08-04
tags: module-loader,RE-09,overlay,recomp-emitter
---

## Claim

Spider-Man's 30 runtime-loaded CD.WAD modules can be statically recompiled BASE-RELATIVE and placed anywhere the game's own allocator puts them

## Evidence

Implemented and run. emit.py emits a relocatable module against a LINK base and adds a per-Core delta at exactly four site classes (HI16 lui results; router constants inside the module's own range, which covers PC-relative branch targets that no relocation table names; jal/jalr/branch-link values; the recovered jump-table switch). Boot before: SIGSEGV, recomp-MISS 0x800C6684, 0 frames. Boot after: 7176 frames in 120s and 5385 in a second 90s run, 0 recomp-MISS, with L5A5LSC/LIZMAN/VENOM simultaneously live at 0x8014A6D0 / 0x801BDA30 / 0x801C6238. Two independent gates back the model: emit.py's HI16-consumer check (911 sites, ~800 uses, 0 raw high halves escaping across all 30 modules) and tools/check_reloc_model.py (8883 sites: 911 HI16 all lui, 5114 J26 all j/jal, 978 LO16 all address-low-half and all traceable to a relocated lui, 0 duplicate offsets, 30/30 streams terminating cleanly; --selftest proves each check fires).

## What would falsify it

a module whose recompiled code reads a wrong address at a delta the boot path never produced — the boot exercises 3 co-resident modules and ~8 distinct deltas, NOT all 30 modules and not gameplay; or a HI16-relocated lui whose raw high half escapes to memory or a comparison, which would make emit.py's consumer gate fail the build
