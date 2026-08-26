---
id: C054
kind: claim
status: holds
created: 2026-08-26
tags: render,re-21,animated-vertex
depends: game/render/mesh_animated_vertex.cpp#decodeAnimatedVertexRecord, game/render/mesh_probe.cpp#submitAnimatedMesh
---

## Claim

Spider-Man animated vertex flag 0x0002 reinterprets word 0 as a shared transformed-vertex cache reuse key, while near and far modes preserve opposite divide-by-16 ordering around RTPS

## Evidence

Exact SLUS_008.75 disassembly: FUN_8007B798 0x8007B81C..0x8007B848 and 0x8007B860..0x8007B9C8; FUN_8007B9CC 0x8007BA48..0x8007BA70 and 0x8007BB9C..0x8007BBD0; dispatcher/gate FUN_80077C08 0x80077C08..0x80077CA8. Existing bounded dem1 logs contain live animated source records with flags 0x0002. mesh_animated_vertex_test accepts projected, retained, near, and exact reuse-address cases and rejects their opposite modes; the Clang integrated target and cpp_policy pass.

## What would falsify it

Exact disassembly contradicts the reuse-address or shift placement, a live flagged record executes the ordinary projection path, or an opposite-answer selftest stops discriminating
