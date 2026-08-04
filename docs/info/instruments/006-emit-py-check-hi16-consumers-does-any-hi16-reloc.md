---
id: I006
kind: instrument
status: trusted
created: 2026-08-04
---

## Instrument

emit.py check_hi16_consumers — does any HI16-relocated lui's RAW high half escape as a value (build gate)

## Validated by

It went RED on real data twice before it was right, and both reds were the instrument, not the code: first it flagged storing the COMPOSED address (3 sites in blackcat) because it did not distinguish a register holding hi-alone from one holding hi+lo; then it flagged a DESTINATION register as a use (4 sites in cop) because rt is a source in some MIPS encodings and a destination in others. Both fixes are reg_reads/reg_writes plus the raw-vs-composed distinction. It fails the BUILD, not a log line, and it prints its denominator (sites, uses examined) on every module whether or not it fires.

## Known failure modes

(none recorded yet)
