---
id: I036
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

game/render/mesh_asset_cook.cpp + PSXPORT_DEBUG=assetprobe,meshprobe retained face-cook comparator

## Validated by

mesh_asset_cook_test and the in-band selftest produce MATCH, one-word MISMATCH, MISSING, and UNLOADED. Captured-PID real-disc run scratch/logs/re21-mesh-cook-live-final.log recorded Dem1_G raw=2/2, cooked=2/2, structurally exact=2, refused=0, then matched all 28 later source bytes at face 0x8018BC7C.

## Known failure modes

This is deliberately a first-face instrument, not a whole-mesh decoder: it copies only the first
face of each mesh, refuses records above 32 bytes, and has a 512-record lifetime capacity. Every
refusal is counted in the asset's LOAD line; a nonzero count forbids generalising from that asset.
It installs only with `PSXPORT_DEBUG=assetprobe`; the later identity check additionally needs
`meshprobe`. It proves source ownership and lifetime, not projection, culling, lighting, colour, or
pixels.
