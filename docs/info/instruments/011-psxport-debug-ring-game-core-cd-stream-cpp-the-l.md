---
id: I011
kind: instrument
status: trusted
created: 2026-08-05
---

## Instrument

PSXPORT_DEBUG=ring (game/core/cd_stream.cpp) — the libstr sector ring's state, now sized by the guest's own slot count

## Validated by

Produces both answers: a WEDGED ring reads 'slots=48 frameStart=7 cons=9 writeIdx=7 | status: 0 2 2 2 2 2 2 2 0...' repeated over 42 decimated samples, a HEALTHY one reads 'slots=48 ... writeIdx=1 | status: 3 0 0...' once and then falls silent because StGetNext stops finding nothing ready. CORRECTED THIS SESSION: it printed a fixed 12 slots and never read DAT_800c1520, so 'the consumer has run past the end of the ring' could only be INFERRED — and that inference was WRONG (the ring has 48 slots). It now reads the count and prints the whole ring. Treat any earlier reading of this line that reasoned about the ring's end as unsupported.

## Known failure modes

(none recorded yet)
