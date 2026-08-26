---
id: C055
kind: claim
status: holds
created: 2026-08-26
tags: render,animation,interpolation,matrix
depends: game/render/mesh_pose_contract.cpp#decodeMeshPoseInput, game/render/mesh_probe.cpp#composeAnimatedPose
---

## Claim

Spider-Man's animated pose composers consume three pre-GTE game-state records: an 8-word base transform, a 5-word secondary rotation, and a packed 24-byte authored pose; FUN_8007FD1C differs from FUN_8007FB1C by an arithmetic right-shift of the three authored translation terms by four before the final MVMVA

## Evidence

Exact SLUS_008.75 disassembly at 0x800777B8..0x800777EC and full instruction comparison of 0x8007FB1C..0x8007FF28; mesh_pose_contract_test passes opposite-answer signed-layout, near/far, changed-key, and same-frame cases in the Clang build

## What would falsify it

if a live POSE_CORPUS row has missing owner/input, an owner mismatch on a face-producing animated call, or identical input signatures yield different retail CR0..CR7
