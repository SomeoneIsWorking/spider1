---
id: I033
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/ghidra_query.py disasm — exact saved-project instruction-range reader

## Validated by

On the saved Spider-Man executable project, disasm 0x8007C570 0x8007C58C returned the seven expected instructions and a denominator of 28 requested bytes (scratch/logs/re21-face-control-disasm-final.log); disasm 0x8007E278 0x8007E27C found zero instructions, printed REFUSED, and exited 1 (scratch/logs/re21-data-range-refusal-final.log).

## Known failure modes

The mode only returns instructions already defined in the saved Ghidra project. Its refusal means
"no defined instruction in this range", not "these executable bytes cannot be code". Re-import the
real executable when the load image changes, and use a raw executable disassembler or byte scan as
the negative control when undiscovered code is possible. The exclusive end address must cover full
instructions; a clipped range deliberately reports only instructions whose start lies inside it.
