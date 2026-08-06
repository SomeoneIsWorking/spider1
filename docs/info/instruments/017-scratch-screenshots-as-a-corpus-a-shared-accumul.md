---
id: I017
kind: instrument
status: DISTRUSTED
created: 2026-08-06
distrusted_on: 2026-08-06
---

## Instrument

scratch/screenshots/ AS A CORPUS — a SHARED ACCUMULATOR written by every run; globbing it is not a read of your run [POINTER ENTRY — canonical record is docs/info/instruments.md INST-27]

## Validated by

NOT validated — registered DISTRUSTED 2026-08-06 as a METHOD, not a tool. Canonical record: docs/info/instruments.md INST-27. Pointer entry only, so info.py brief/list/check can see it.

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-06

See docs/info/instruments.md INST-27 for the full record. In short: scratch/screenshots/ is written by every run, every tool, every session, and is never cleared, so glob / sorted()[-1] / 'the newest one' silently mixes runs. MEASURED CONSEQUENCE 2026-08-05: a glob swept a STALE leftover in as 'the correct reference frame' and manufactured an entire false root cause for Spider-Man's flicker (a widescreen explanation, subsequently REFUTED). It is near-uncatchable because a stale capture is a real valid picture that passes every check we run — plausible frame, right dimensions, non-black, sensible colour count. THE RULE, two halves: (1) write to a PER-RUN directory created fresh; (2) verify every file against its own present_shot/capture log line before reading it. mtime is not proof — a run that dies before capturing leaves the previous file newest.

> Every result this instrument produced is suspect until it is re-validated.
