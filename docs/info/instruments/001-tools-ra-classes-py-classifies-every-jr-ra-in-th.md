---
id: I001
kind: instrument
status: trusted
created: 2026-07-30
---

## Instrument

tools/ra_classes.py — classifies every `jr $ra` in the substrate as a real return or a computed jump, by reaching-definitions on $ra, and enumerates a body's in-body `jal` resume points. Answers RE-16's set questions in seconds instead of a 10-minute build+run.

## Validated by

Has a --selftest that feeds it three synthetic $ra definitions (jal link, frame reload, restored continuation) and requires all three to classify DIFFERENTLY — a classifier hardwired to 'return' would pass every real case in this binary by accident, since 1721/1722 sites are returns. Also validated against the disassembly of 0x8002A338, where it must and does split the body's three `jr $ra` two ways. Two false-positive classes were caught BY the audit it prints and fixed: `move $ra,$sN` restore (10 -> 6 sites) and `sw/lw $ra` through a GLOBAL save slot rather than the stack (6 -> 1).

## Known failure modes

(none recorded yet)
