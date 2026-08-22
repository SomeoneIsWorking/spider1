---
id: I038
kind: instrument
status: trusted
created: 2026-08-22
---

## Instrument

game/render/face_builder_census.cpp + mesh_probe.cpp — exact SLUS_008.75 FUN_8007C4D8 caller and output census

## Validated by

The pure selftest exercises two known return addresses, an unknown return address, repeated
aggregation, contextual and non-contextual calls, and valid and invalid primitive-cursor deltas.
Ghidra xrefs found exactly 12 direct calls; `scratch/logs/gate-boot-20260822-174218.log` observed
24,576 calls with `unknownCallsite=0` and four distinct known owners, while
`scratch/logs/gate-boot-20260822-174725.log` changed dominant `FUN_80077C08` from `NO-CONTEXT` to
14,793 layout-matched animated contexts with zero mismatches. After the framework pin moved to
`ad5cf802`, `scratch/logs/gate-boot-20260822-181035.log` independently reached 16,384 calls and
again reported all four known owners, `unknownCallsite=0`, and zero invalid cursor deltas, layout
mismatches, and transform mismatches. A separate replay aborted before the instrument's boundary in
the guest allocator (issue 0018) and is not counted as census evidence.

## Known failure modes

The executable table covers Ghidra's direct-call reference set. An indirect dispatch into
`FUN_8007C4D8` would be counted under `unknown` rather than silently attributed. Primitive output is
the monotonic low-24-bit cursor delta within one call; a decreasing cursor is counted as an invalid
delta and contributes no bytes, because the instrument does not guess across a pool reset or wrap.
The bounded live corpus reached `dem1` and exercised four of twelve sites; the eight quiet sites are
statically classified but not runtime-validated in that scene.
