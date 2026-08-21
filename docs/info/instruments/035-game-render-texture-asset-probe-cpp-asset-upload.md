---
id: I035
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

game/render/texture_asset_probe.cpp + asset_upload_ledger.cpp — RE-21 authored texture/CLUT owner and lifetime tracer

## Validated by

asset_upload_ledger_test and the in-band install selftest select the latest exact upload owner, make a one-word target perturbation return MISSING, resolve a containing geometry allocation, and change live=yes to live=no on unload. Fresh real-disc run scratch/logs/re21-asset-owner-live-final.log then showed both answers in-band: exact texture/CLUT targets resolved to Dem1_L.psx while the transient raw sources were dead, the Dem1_G mesh resolved live, and both later produced explicit UNLOAD lines.

## Known failure modes

(none recorded yet)
