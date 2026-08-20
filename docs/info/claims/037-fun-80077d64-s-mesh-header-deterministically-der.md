---
id: C037
kind: claim
status: holds
created: 2026-08-20
tags: re-21,render,mesh-layout
depends: game/render/mesh_probe.cpp#deriveLayout
---

## Claim

FUN_80077D64's mesh header deterministically derives the FUN_8007C4D8 secondary pointer, face-stream pointer, and face count on observed live Spider-Man execution

## Evidence

Static Ghidra decompilation in scratch/logs/re21_decomp_7c4d8.txt shows +0x1C + vertexCount*8 + secondaryCount*8 pointer arithmetic. The 2026-08-20 live reference-path run scratch/logs/gate-boot-20260820-221812.log observed 27,579 contextual FUN_8007C4D8 calls, sampled at least 64 object/mesh pairs (the roster cap), and reported layoutMismatches=0; scratch/logs/gate-boot-20260820-223032.log independently exercised the current self-test and a live MATCH.

## What would falsify it

Any call to FUN_8007C4D8 nested under FUN_80077D64 reports layout=MISMATCH, or disassembly shows a path deriving those arguments by different arithmetic.
