---
id: C047
kind: claim
status: holds
created: 2026-08-22
tags: render,re21,spider
depends: game/render/face_builder_census.cpp#kCallsites, game/render/mesh_probe.cpp#buildFaces
---

## Claim

In the bounded dem1 corpus, Spider-Man retail face production is dominated by the animated FUN_80077C08 family, and the first direct 28-byte face is clipped or expanded rather than emitted as one quad

## Evidence

Ghidra xrefs enumerated 12 direct calls to FUN_8007C4D8. scratch/logs/gate-boot-20260822-174218.log classified all 24,576 live calls: FUN_80077C08=18,355 calls/306,027 faces, FUN_8002EED4=5,308/5,308, FUN_80077D64=593/593, FUN_80077A48=320/2,310, unknown=0. The first direct face advanced the primitive cursor 0x0CEA50->0x0CEC30, 480 bytes for one 28-byte source record. The post-extension run scratch/logs/gate-boot-20260822-174725.log gave FUN_80077C08 14,793 contextual calls with zero layout mismatches at the 20,480-call checkpoint.

The independent replay after pinning clean psxport `ad5cf802`,
`scratch/logs/gate-boot-20260822-181035.log`, reached 16,384 calls: `FUN_80077C08` still dominated at
11,432 calls / 192,419 faces, with zero unknown callsites, invalid cursor deltas, layout mismatches,
and transform mismatches. A separate replay aborted in the guest allocator before `dem1` (issue
0018); it neither supports nor falsifies this face-boundary claim.

## What would falsify it

A rerun of SLUS_008.75 reaches dem1 but maps any FUN_8007C4D8 return address outside the 12-site executable table, the dominant site is not FUN_80077C08 in the same bounded corpus, or the first direct face advances by exactly one un-clipped FT4 record instead of 480 bytes.
