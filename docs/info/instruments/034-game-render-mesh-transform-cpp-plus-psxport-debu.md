---
id: I034
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

game/render/mesh_transform.cpp plus PSXPORT_DEBUG=meshprobe direct-transform source validator

## Validated by

The shipping self-test, run by CTest `mesh_transform_contract` and again in band before wrappers are
installed, accepts an exact source tuple and rejects four independent other answers: +1 relative Z,
non-zero rotation, scale flag 0x0200, and an unknown return address. Live log
`scratch/logs/gate-boot-20260821-032403.log` then reported MATCH at both legal callsites under two
distinct object/camera tuples, while unrelated FUN_8007C4D8 calls were NO-CONTEXT.
The final Clang/tidy-clean binary repeated the in-band self-test and first live MATCH with zero
transform mismatches in the PID-bounded `scratch/logs/re21-transform-final.log` run.

## Known failure modes

The validator covers only the two direct `FUN_80077D64` callsites whose retail guards require zero
object rotation and clear scale flag `0x0200`. Rotated/scaled paths use other guest functions and
must not be inferred from this result. It validates source memory and the passed relative vector;
it deliberately never reads GTE registers or projected scratchpad output. Consequently it cannot
certify RTPS saturation, screen projection, the source-vertex flag `0x10` path, or any future native
producer. Missing owner/camera/relative pointers are a mismatch, not an all-zero answer.
