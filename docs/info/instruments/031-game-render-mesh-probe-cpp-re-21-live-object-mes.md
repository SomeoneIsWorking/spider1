---
id: I031
kind: instrument
status: trusted
created: 2026-08-20
---

## Instrument

game/render/mesh_probe.cpp — RE-21 live object/mesh submission-boundary validator

## Validated by

Its install self-test accepts the header-derived pointer/count tuple and rejects a +4-byte
face-stream perturbation. Live log `scratch/logs/gate-boot-20260820-223032.log` then produced both
`layout=MATCH` inside `FUN_80077D64` context and `NO-CONTEXT` for other `FUN_8007C4D8` callers. The
final null-safe build repeated `SELFTEST PASS` in `scratch/logs/gate-boot-20260820-223543.log`; that
four-second replay ended before the first contextual face call.

## Known failure modes

The unique-context roster is capped at 64 object/mesh pairs, so `uniqueContexts=64` means at least
64 were observed, not that the run contained exactly 64. A `FUN_8007C4D8` call outside the dynamic
extent of the wrapped `FUN_80077D64` is deliberately reported as `NO-CONTEXT`; the probe cannot
attribute those callers. Context is tracked with process-global nesting state and therefore assumes
these guest render calls are serialized on one execution thread. A null relative-translation pointer
is reported as `(0,0,0)` rather than dereferenced.
