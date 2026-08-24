---
id: 19
title: native depth lookups miss ABSENT for prims whose vertices are stored as spill-mediated halfwords
status: open
symptom: depth coverage % below expectation; ABSENT misses dominate; vertices stored via sh after stack spill get no depth record; 39% of prims fall to the 2D band
tags: RE-08,depth,ndepth,halfword,spill,gte-store,render
created: 2026-08-24
updated: 2026-08-24
---

MEASURED 2026-08-24 (RE-08): over a 240s headless run, 60.91% of prims carry real per-vertex depth
(1354377/2223704) with 84.60% lookup hit; misses split 86% ABSENT / 14% STALE. Root shape of the
ABSENT class, read from disassembly (disasm.py spot-check after Ghidra,
FUN_80028304..8002831C in FUN_80028030): the guest reads projected XY via mfc2, SPILLS it as a stack
word (sw t0,0x90(sp)), reloads it (lw v1,0x90(sp)), and stores HALFWORDS into the packet
(sh v1,(t7) / sh v0,-4(s0)). Neither emitter tap form sees this: emit.py _track_value collects sw
only and does not track through memory; a direct mfc2->sh corpus scan finds ZERO pairs, so a naive
sh extension closes nothing. DESIGNED FIX (not built; owned by the framework recomp-emitter area):
one-hop spill-through tracking + sh targets + gte_record_pz keying the containing aligned word
(addr&~3) - safe for existing word-aligned sites, order-independent for halfword pairs because the
second store snapshots the completed word as guard. The current 9c2e3f1c pin still lacks that
tracking. Until then these prims sort by draw order, which is correct PSX behaviour but not native
depth.
