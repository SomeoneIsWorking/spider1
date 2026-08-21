---
id: C043
kind: claim
status: holds
created: 2026-08-21
tags: render,re-21,mesh,asset-lifetime
depends: game/render/mesh_asset_cook.cpp#MeshAssetCookLedger, game/render/mesh_face_format.cpp#deriveMeshLayout, game/render/texture_asset_probe.cpp#parsePsxAsset, game/render/mesh_probe.cpp#logDecodedSourceFace
reconfirmed: 2026-08-21 12:19:30
verified_at: 2026-08-21 12:19:30
---

## Claim

Spider-Man's retail load-time mesh cook embeds the first Dem1 direct face's final 28-byte UV/CLUT/TPAGE record in retained Dem1_G memory, and FUN_8007C4D8 later consumes that record byte-for-byte until asset unload

## Evidence

Exact disassembly of FUN_80068BB0 -> FUN_80074C98 proves the in-place face walk and structural mutations; direct-texture faces receive their descriptor-derived UV/CLUT/TPAGE words. Captured-PID run scratch/logs/re21-mesh-cook-live-final.log recorded Dem1_G raw=2/2, cooked=2/2, structuralExact=2, refused=0 and later MESH_COOK face=8018BC7C structuralCook=EXACT retainedSource=MATCH for all 28 bytes; it also observed Dem1_G unload. mesh_asset_cook_test and the in-band selftest make a one-word perturbation MISMATCH, an absent face MISSING, and unload UNLOADED.

## What would falsify it

a clean retail replay changes any of the first face cook words or its later source identity, unload fails to invalidate it, static instructions contradict the documented cook, or any opposite-answer case stops discriminating

## Re-confirmed 2026-08-21 12:11:23

Final Clang build and all five normal CTests passed. Captured-PID real-disc replay scratch/logs/re21-mesh-cook-live-final.log recorded Dem1_G raw=2/2, cooked=2/2, structuralExact=2, refused=0, followed by the later 28-byte retained source MATCH; the in-band and hermetic opposite-answer cases still discriminated.

## Re-confirmed 2026-08-21 12:15:26

Final Clang build and all five normal CTests passed. PSXPORT_NOPACE=1 affected only pacing; captured-PID real-disc replay scratch/logs/re21-mesh-cook-live-final.log reached the same Dem1 source in five seconds, recorded raw=2/2 cooked=2/2 structuralExact=2 refused=0, and matched all 28 later source bytes. The in-band and hermetic MISMATCH/MISSING/UNLOADED controls remained green.

## Re-confirmed 2026-08-21 12:18:22

Final DRY integration moved the shared mesh layout formula into mesh_face_format.cpp. All five normal CTests and Clang policy passed; captured-PID real-disc replay scratch/logs/re21-mesh-cook-live-final.log repeated raw=2/2 cooked=2/2 structuralExact=2 refused=0 and matched all 28 later source bytes, while opposite answers remained green.

## Re-confirmed 2026-08-21 12:19:30

Final naming cleanup changed no semantics. Clang rebuilt the port, all five normal CTests including all-TU format/tidy passed, and the captured-PID NOPACE real-disc replay again reached Dem1 in five seconds with raw=2/2 cooked=2/2 structuralExact=2 refused=0 plus all-28-byte retained source MATCH and explicit unload.
