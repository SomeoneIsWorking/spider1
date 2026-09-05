---
id: C013
kind: claim
status: holds
created: 2026-08-04
tags: module-loader,RE-09,overlay,jit
depends: docs/issues/0001-recomp-miss-0x800c6684-three-cd-wad-modules-live.md,docs/re-frontier.md#RE-09
---

## Claim

Spider-Man's 30 runtime-loaded CD.WAD modules require image-aware guest placement: multiple modules
can be live at distinct allocator-chosen bases, and their code identity must follow load, unload, and
replacement events.

## Evidence

A bounded real boot falsified the former one-slot residency premise. L5A5LSC, LIZMAN, and VENOM were
simultaneously live at 0x8014A6D0, 0x801BDA30, and 0x801C6238. Independently relocated bytes for
SHELL matched 112,912 bytes of the guest-loaded RAM image, establishing that the observed images are
the game's CD.WAD code modules rather than unrelated allocations. The implementation that produced
the old run has been removed; this claim retains only the guest image-lifecycle fact that the dynamic
runtime must preserve through authenticated image identity and code-cache invalidation.

## What would falsify it

A runtime census that accounts for every module load and eviction and proves only one code module is
ever live, or authenticated image bytes showing that the three observed allocations were not the
named CD.WAD modules.
