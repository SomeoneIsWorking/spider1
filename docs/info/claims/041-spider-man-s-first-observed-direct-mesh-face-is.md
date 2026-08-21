---
id: C041
kind: claim
status: holds
created: 2026-08-21
tags: render,re-21,texture,asset-lifetime
depends: game/render/mesh_probe.cpp#logDecodedSourceFace, game/render/texture_asset_probe.cpp#parsePsxAsset, game/render/texture_asset_probe.cpp#spiderman_report_texture_asset_binding, game/render/asset_upload_ledger.cpp#latestCovering
reconfirmed: 2026-08-21 12:18:22
verified_at: 2026-08-21 12:18:22
---

## Claim

Spider-Man's first observed direct mesh face is retained at Dem1_G.psx+0xB4, while its exact 4-bpp texture and CLUT were uploaded by Dem1_L.psx from transient raw offsets +0x2B30 and +0x38 that are freed and reused before face submission

## Evidence

Executable FUN_80069A60 builds and loads <name>.psx, FUN_80068BB0 parses/trims it, FUN_80081C50 uploads it, and FUN_800695D0 unloads it. Fresh Clang build run scratch/logs/re21-asset-owner-live-final.log records CD LBA 8907 -> Dem1_L at 0x801539D4 and LBA 8903 -> Dem1_G at 0x8018BB84; the first sourceFace resolves mesh 0x8018BC38 to Dem1_G+0xB4, texture target (525,246 2x8) to Dem1_L+0x2B30, and CLUT (544,3 16x1) to Dem1_L+0x38. Dem1_L shrinks 12288->48 bytes, making both sourceBytesLive=no while slot 7 is live. The next asset, henchman.psx, starts at 0x80153A0C: exactly the former CLUT source and low enough to cover the former texture source as well. Dem1_G retains 7836 bytes and is live. The same run explicitly observes both unloads. Hermetic and in-band selftests make a one-word target perturbation return MISSING.

## What would falsify it

A clean real-disc replay of the same first contextual face resolves a different authored asset/offset or lifetime, any executable instruction contradicts the load/parse/trim/unload model, or the exact-vs-perturbed owner test stops discriminating

## Re-confirmed 2026-08-21 10:41:32

Fresh Clang build replay scratch/logs/re21-asset-owner-live-final.log resolved the first face to Dem1_G+0xB4 and its exact texture/CLUT uploads to transient Dem1_L+0x2B30/+0x38; hermetic and in-band opposite-answer tests passed.

## Re-confirmed 2026-08-21 10:44:27

Post-landing Clang build and four focused CTests passed; the retained real-disc trace re21-asset-owner-live-final.log still resolves the first face to Dem1_G+0xB4 and its texture/CLUT uploads to transient Dem1_L+0x2B30/+0x38, with the exact-versus-perturbed ledger test discriminating.

## Re-confirmed 2026-08-21 11:40:38

Final repin to psxport 692b9b20: Clang rebuild and all four CTests passed. Bounded PID-owned replay scratch/logs/re21-asset-owner-live-692b9b20.log passed the exact-versus-one-word-perturbed selftest, resolved Dem1_G+0xB4 live geometry and Dem1_L+0x2B30/+0x38 transient texture/CLUT owners, observed both unloads, and contained zero recomp-MISS or missing geometry-owner marker.

## Re-confirmed 2026-08-21 12:18:22

Final Clang replay scratch/logs/re21-mesh-cook-live-final.log again resolved mesh 8018BC38 to live Dem1_G+0xB4 and the exact texture/CLUT targets to transient Dem1_L+0x2B30/+0x38 with sourceBytesLive=no; it also observed Dem1_G unload. The upload-ledger and new retained-cook opposite-answer tests passed.
