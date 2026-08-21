---
id: I031
kind: instrument
status: trusted
created: 2026-08-20
---

## Instrument

game/render/mesh_probe.cpp — RE-21 live object/mesh submission-boundary validator

## Validated by

Its install self-test accepts the header-derived pointer/count tuple, rejects a +4-byte face-stream
perturbation, and calls the first-face sampler with null/empty inputs to prove it does not read them.
Live log `scratch/logs/gate-boot-20260821-010831.log` on the final Clang/tidy-clean build produced
`layout=MATCH` inside `FUN_80077D64` context and `NO-CONTEXT` for other `FUN_8007C4D8` callers. The
long denominator remains `scratch/logs/gate-boot-20260820-221812.log`: 27,579 contextual calls and
zero mismatches.
The extended source-record path first decodes the executable-derived stride and only reads fields
that fit inside it. On the final Clang/tidy-clean build,
`scratch/logs/gate-boot-20260821-025743.log` reported a 28-byte direct-textured quad, four in-range
source vertices, UVs, CLUT, stored TPAGE `0x0008`, and effective TPAGE `0x0028` / blend mode 1 for
the first contextual mesh face. A second contextual mesh independently decoded a 32-byte record
with valid vertices and material fields. The installed self-test perturbs the scratchpad control
word and checks the resulting effective flags and TPAGE blend bits.
The transform extension captures the actual current object from `s3` at `FUN_80077D64`, separately
from `FUN_80076480`'s outer list head. In `scratch/logs/gate-boot-20260821-032403.log` it reported
`transform=MATCH` at both legal direct-path return addresses with distinct owners, relative vectors,
and camera matrices. The first line corrected the old attribution: list head `0x8018BB90` is not the
owner; owner `0x8018BBB4` has flags `0x0000`.
The final eight-second PID-bounded replay, `scratch/logs/re21-transform-final.log`, repeated the
in-band self-test and first contextual transform MATCH with zero transform mismatches; unrelated
face-builder calls now label both transform and layout `NO-CONTEXT` instead of implying a mismatch.

## Known failure modes

The unique-context roster is capped at 64 object/mesh pairs, so `uniqueContexts=64` means at least
64 were observed, not that the run contained exactly 64. A `FUN_8007C4D8` call outside the dynamic
extent of the wrapped `FUN_80077D64` is deliberately reported as `NO-CONTEXT`; the probe cannot
attribute those callers. Context is tracked with process-global nesting state and therefore assumes
these guest render calls are serialized on one execution thread. A null relative-translation pointer
is reported as `(0,0,0)` rather than dereferenced.
The direct-transform result applies only inside the dynamic extent of wrapped `FUN_80077D64` and
only to its two statically proven zero-rotation/unscaled callsites. It does not read live GTE
registers and therefore cannot validate later GTE saturation/projection output. The source-vertex
flag `0x10` branch remains a separate, unproved transform variant.
An empty or null face stream reports zero sample words rather than being dereferenced.
The source-record line is emitted only for a new contextual object/mesh pair whose derived layout
matches. It therefore proves that sampled record, not every face format or every caller. Fields that
do not fit the decoded stride remain zero and `textureValid=false`; those zeroes are not source data.
