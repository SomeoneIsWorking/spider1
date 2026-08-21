---
id: C037
kind: claim
status: holds
created: 2026-08-20
tags: re-21,render,mesh-layout
depends: game/render/mesh_face_format.cpp#deriveMeshLayout, game/render/mesh_probe.cpp#buildFaces
reconfirmed: 2026-08-21 12:52:51
verified_at: 2026-08-21 12:52:51
---

## Claim

FUN_80077D64's mesh header deterministically derives the FUN_8007C4D8 secondary pointer, face-stream pointer, and face count on observed live Spider-Man execution

## Evidence

Static Ghidra decompilation in scratch/logs/re21_decomp_7c4d8.txt shows +0x1C + vertexCount*8 + secondaryCount*8 pointer arithmetic. The 2026-08-20 live reference-path run scratch/logs/gate-boot-20260820-221812.log observed 27,579 contextual FUN_8007C4D8 calls, sampled at least 64 object/mesh pairs (the roster cap), and reported layoutMismatches=0; scratch/logs/gate-boot-20260820-223032.log independently exercised the current self-test and a live MATCH.

## What would falsify it

Any call to FUN_8007C4D8 nested under FUN_80077D64 reports layout=MISMATCH, or disassembly shows a path deriving those arguments by different arithmetic.

## Re-confirmed 2026-08-21 01:02:42

2026-08-21: rebuilt spiderman_port with Clang after the typed MeshCounts and null/empty face-stream guard; scratch/logs/gate-boot-20260821-010144.log reports SELFTEST PASS and live layout=MATCH tuples at frames 382 and 777 with no mismatch line.

## Re-confirmed 2026-08-21 02:58:33

2026-08-21 clean-framework verification at psxport 2b5ef7b5: Clang rebuilt spiderman_port; cpp_policy passed 16/16 first-party C++ TUs; scratch/logs/gate-boot-20260821-025743.log reports meshprobe SELFTEST PASS plus live layout=MATCH at frames 487 and 904, with no layout=MISMATCH line.

## Re-confirmed 2026-08-21 12:18:22

The duplicated layout arithmetic was moved without semantic change into mesh_face_format.cpp#deriveMeshLayout. Final Clang replay scratch/logs/re21-mesh-cook-live-final.log repeated the in-band exact-versus-+4 selftest and live headerCounts 4/1/1 -> derived 8018BC54/8018BC74/8018BC7C with layout=MATCH.

## Re-confirmed 2026-08-21 12:52:51

Post-landing Clang CTest passed 6/6; the shared mesh layout derivation still produced the exact first-face layout and the live cook replay reported transform/layout MATCH with zero mismatches.
