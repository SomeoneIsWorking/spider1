---
id: I008
kind: instrument
status: trusted
created: 2026-08-04
---

## Instrument

PSXPORT_DEBUG=ring (game/core/cd_stream.cpp) — STR sector-ring state at StGetNext

## Validated by

Trustworthy AND discriminating, validated on both classes: at f3 it printed 'prod=0 cons=0 d1514=1 | slots: 3 0 0 ...' (a working start) and after the second movie 'prod=7 cons=9 d1514=7 | slots: 0 2 2 2 2 2 2 2 0 0 0 0' (the deadlock). It shows the two OPPOSITE faults distinguishably, exactly as its own comment claims — slots holding 2 means the producer is fine and the consumer index is wrong. Note it is decimated 1-in-200000, so it reports a STATE, not a rate.

## Known failure modes

(none recorded yet)
