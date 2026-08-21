---
id: C040
kind: claim
status: holds
created: 2026-08-21
tags: re-21,render,mesh-transform
depends: game/render/mesh_transform.cpp#inspectMeshDirectTransform, game/render/mesh_probe.cpp#submitMesh
reconfirmed: 2026-08-21 12:11:35
verified_at: 2026-08-21 12:11:35
---

## Claim

Spider-Man's FUN_80077D64 direct mesh path transforms object-local vertices with the camera matrix and an exact object-minus-camera relative translation

## Evidence

Retail instructions 0x800767B0..0x800767E4 load `camera+0x74` into the GTE rotation matrix and zero GTE translation; 0x800767E8..0x80076824 compute `(objectPosition20p12 sra 12)-cameraPosition`; callsite guards 0x80076DCC..0x80076DF8 require zero XYZ rotation and clear object flag 0x0200. `FUN_80077D64` passes that relative vector and `mesh+0x1C` to `FUN_8007C2AC`. Instructions 0x8007C33C..0x8007C374 add the relative components to each ordinary source vertex and execute RTPS under the already-loaded camera matrix. The source-boundary log `scratch/logs/gate-boot-20260821-032403.log` observed both legal returns 0x80076E78/0x80076E88 with `passedRel=expectedRel` under distinct camera matrices. First owner 0x8018BBB4 had flags 0x0000; 0x8018BB90 was only the outer list head.

## What would falsify it

Any instruction-exact decode changes these field/guard meanings, or any live FUN_80077D64 call from either direct return address has valid owner/camera/relative sources but passedRelative differs from (objectPosition20p12 sra 12)-cameraPosition or uses a matrix other than camera+0x74.

## Re-confirmed 2026-08-21 12:11:35

The only mesh_probe change adds a retained-cook identity report after the existing decoded-face path and does not change transform capture or validation. Final Clang replay scratch/logs/re21-mesh-cook-live-final.log repeated the direct-transform selftest PASS and logged the first contextual face transform=MATCH before the retained-source MATCH.
