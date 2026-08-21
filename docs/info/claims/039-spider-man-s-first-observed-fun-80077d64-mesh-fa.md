---
id: C039
kind: claim
status: holds
created: 2026-08-21
tags: re-21,render,mesh-face,source-record
depends: game/render/mesh_face_format.cpp
---

## Claim

Spider-Man's first observed FUN_80077D64 mesh face is a 28-byte direct-textured quad whose source record names four vertex indices plus four UVs, CLUT and texture page

## Evidence

Executable instructions at 0x8007C570..0x8007C588 apply the scratchpad control word to the encoded header, 0x8007CC18 advances by the effective high half, 0x8007C774 branches flag 0x10 between triangle and quad, and 0x8007C950 branches flag 0x1 to the GP0(2C) path that copies source offsets +0x10/+0x14/+0x18. That path also ORs `(flags & 0x180) << 14` into the packed UV/TPAGE word at 0x8007C9B0..0x8007C9C0. In clean-pin live reference-leg log scratch/logs/gate-boot-20260821-025743.log, object 0x8018BB90 / mesh 0x8018BC38 produced raw header 0x001C1083 and indices 0x02030001 under control 0xFFFF0000: stride 28, quad indices [1,0,3,2], source vertices [(1970,-1,-78),(-1967,-1,-78),(1970,-1774,-78),(-1967,-1774,-78)], UVs [(59,253),(52,253),(59,246),(52,246)], CLUT 0x00E2 at (544,3), and stored TPAGE 0x0008 plus face flag 0x80. The executable rule combines those into effective 4-bpp TPAGE 0x0028 at (512,0), blend mode 1. All indices were in range, and the in-band format self-test passed.

## What would falsify it

Any instruction-exact decode contradicts these flag/offset meanings, or a repeated live observation of this same encoded source record under the same control word produces a different decoded stride, indices, source fields, UVs, CLUT, or texture page.
