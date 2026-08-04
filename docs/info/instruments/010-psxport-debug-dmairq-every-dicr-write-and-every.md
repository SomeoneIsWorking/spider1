---
id: I010
kind: instrument
status: trusted
created: 2026-08-05
---

## Instrument

PSXPORT_DEBUG=dmairq — every DICR write and every DMA3 completion, with whether channel 3 was ARMED

## Validated by

Shown to produce BOTH answers on ONE run: 'armed=0' for the 9 non-final chunks of an STR frame and 'armed=1' for the 10th, plus the DICR value each write produced (scratch/logs/re07_after.log). It is what made the once-per-sector vs once-per-frame distinction visible at all. Denominator is explicit — one line per DMA3 completion, none suppressed, none decimated.

## Known failure modes

(none recorded yet)
