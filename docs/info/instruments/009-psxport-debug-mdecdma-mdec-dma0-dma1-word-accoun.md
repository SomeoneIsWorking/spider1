---
id: I009
kind: instrument
status: trusted
created: 2026-08-04
---

## Instrument

PSXPORT_DEBUG=mdecdma — MDEC DMA0/DMA1 word accounting

## Validated by

Trustworthy for this use and carries its own denominator: over a 20337-frame run it emitted 24 lines total, i.e. exactly two decode attempts, and it names the abandoned-word count ('forced stop: 1548 word(s) abandoned') rather than going quiet. Cross-validated against disc ground truth: its reported DMA0 word counts (1824, 1440) equal the BS-header word counts read independently out of ATVILOGO.STR and LOGO.STR.

## Known failure modes

(none recorded yet)
