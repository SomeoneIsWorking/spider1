---
id: C042
kind: claim
status: holds
created: 2026-08-21
tags: recomp,bios,interrupt
depends: game/recomp_seeds.json
---

## Claim

Spider-Man's HookEntryInt continuation is CdInit's mid-function PC 0x8008B990, which must be a main_reentry discovery root for psxport's modeled BIOS exception exit

## Evidence

The retail CdInit instruction stream calls setjmp at 0x8008B988 and resumes at 0x8008B990; the live HookEntryInt buffer at 0x800B28BC independently stores ra=0x8008B990. A 9f1 run without that root fail-fast reported recomp-MISS 0x8008B990. Adding only main_reentry regenerated 738 roots into 1672 resident fragments with wrapper, body, and dispatch case. The final pinned `692b9b20` Clang build passes all four CTests and scratch/logs/re21-asset-owner-live-692b9b20.log reaches the exact Dem1 geometry/texture/CLUT binding and unload evidence with zero recomp-MISS. psxport's hermetic emitter test also proves the opposite answer: the same interior PC is absent when not listed.

## What would falsify it

The exact retail disassembly or live jmp_buf names a different continuation, removing the root still lets a clean regenerated 9f1+ substrate execute the modeled custom exit without a dispatch miss, or the seeded continuation falls through instead of returning through B0:17
