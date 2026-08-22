---
id: C048
kind: claim
status: holds
created: 2026-08-22
tags: allocator,meshprobe,issue-0018
depends: game/render/mesh_probe.cpp, game/render/face_builder_census.cpp, game/core/allocator_audit.cpp, game/core/irq_poll_audit.cpp
reconfirmed: 2026-08-22 19:11:09
verified_at: 2026-08-22 19:11:09
---

## Claim

Issue 0018 is not a deterministic side effect of installing Spider-Man's observe-only face census

## Evidence

Matched ad5cf802 bounded legs with PSXPORT_DEBUG=allocaudit reached the suspect post-TTSLOGO CD IRQ
without meshprobe (scratch/logs/gate-boot-20260822-183038.log), while the meshprobe plus full-R3000
poll-audit leg crossed it and reached dem1/frame 1901 and 3,320+ face calls
(scratch/logs/gate-boot-20260822-184214.log) with no invalid free-list edge, external free-node write,
or deferred-work register difference. Static inspection shows the wrappers read guest state, update
host counters, and super-call generated bodies.

## What would falsify it

a matched deterministic schedule produces the invalid-link fault only with meshprobe installed, or a wrapper is found writing guest state outside its generated super-call

## Re-confirmed 2026-08-22 19:11:09

The clean 57a17a14 recurrence in scratch/logs/gate-boot-20260822-190346.log ran with meshprobe enabled but failed before any live face call. The allocator watch identified the store owner as retail FUN_8002A338:0x8002A478, not a mesh wrapper: decoded 0x0401 was written to live free node 0x801664E4 after the decoder exceeded its gp+0x6DC[0] output allocation. This strengthens the owner result without claiming logging cannot perturb timing.
