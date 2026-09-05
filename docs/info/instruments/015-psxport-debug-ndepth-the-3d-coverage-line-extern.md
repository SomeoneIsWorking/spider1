---
id: I015
kind: instrument
status: DISTRUSTED
created: 2026-08-06
distrusted_on: 2026-08-06
---

## Instrument

PSXPORT_DEBUG=ndepth — the '3D%=' coverage line (external/psxport/runtime/psx/gpu_native.cpp:1646) [POINTER ENTRY — canonical record is docs/info/instruments.md INST-26]

## Validated by

NOT validated — registered already DISTRUSTED 2026-08-06. This repo keeps TWO instrument registries: this directory (what info.py brief/list/check read) and docs/info/instruments.md (what a human reads, and what CLAUDE.md points at). INST-26 there is the canonical record; this entry exists so info.py can SEE it. Keep it a pointer — do not grow a second copy of the text here, or the two will diverge.

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-06

See docs/info/instruments.md INST-26 for the full record. In short: the report is gated `s_frame % 60 == 0` (gpu_native.cpp:1646) while the counters it reports are zeroed on EVERY present (:1673, and :1660 for the projprim line), so it prints a ONE-FRAME sample with no denominator; and the percentage expression `(nd3d+nd2d) ? 100.0*nd3d/(nd3d+nd2d) : 0.0` renders NO DATA as `3D%=0.0`, identical to a genuine 0%. MEASURED: Spider-Man draws on alternate fields and 60 is even, so every sample landed on a non-drawing field and the channel printed `3D%=0.0` for a whole run while measuring nothing. Re-check C003, which cites this channel's projprim records=0/hit=0/miss=0. To trust again: carry the denominator (frame count + raw totals; a no-data sample must print n/a, not a number), and sample by DRAW EVENT rather than frame parity.

> Every result this instrument produced is suspect until it is re-validated.
