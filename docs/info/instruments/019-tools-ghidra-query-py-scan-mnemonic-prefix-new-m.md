---
id: I019
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

tools/ghidra_query.py scan <mnemonic-prefix> — NEW mode. Walks every disassembled instruction in the Ghidra project, prints matches with operands + owning function, then dumps each hit's whole enclosing function. Reports its DENOMINATOR on every run (instructions walked, functions defined, bytes of memory) and its BLIND SPOT (percentage of the image Ghidra actually disassembled), and exits NON-ZERO on zero matches so '(none)' cannot be mistaken for 'I never looked'.

## Validated by

POSITIVE: 'scan ctc2' over the Spider-Man image found 1,404 sites across 130,588 instructions / 1,564 functions, of which 10 target cop2 control regs 24/25/26 — the two libgte projection leaves and InitGeom (scratch/logs/g7/scan_ctc2.txt). CROSS-CHECKED AGAINST AN INDEPENDENT METHOD because its own blind-spot line said Ghidra had disassembled only 24.9% of the 2 MB image: a raw word scan of all 524,288 words of ram.bin for every ctc2 rX,CR24/25/26 encoding returned the SAME 10 addresses, and for jal 0x8008BF14 / 0x8008BF24 / 0x8008BE5C the SAME single caller each. So on this corpus the 75% Ghidra never disassembled contained 0 hidden sites — measured, not assumed. NEGATIVE CASE RUN: `scan zzznosuch` on the same project prints the same denominator and blind-spot lines, then `// 0 match(es)`, and exits 1. So the zero-match path is exercised, carries its denominator, and is machine-detectable — a caller cannot mistake it for success.

## Known failure modes

(none recorded yet)
