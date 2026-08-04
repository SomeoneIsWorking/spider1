---
id: I007
kind: instrument
status: trusted
created: 2026-08-04
---

## Instrument

ad-hoc MIPS immediate scan for I/O register offsets over ram.bin

## Validated by

DISTRUST IT — I wrote one this session and it LIED. Scanning 186880 instructions for load/store immediates 0x10F0/0x10F4/0x10A8/0x1810/0x1824 reported 'DICR: NONE', which would have 'proved' the guest uses no DMA IRQ. But the SAME scan reported GP0 (0x1810) and MDEC0 (0x1820) as NONE too, and the guest provably writes both every frame — the code addresses them through a base register (lui+addiu 0x1F801000 then a small offset), a form the scan cannot see. Any negative from this method is worthless. Use Ghidra's reference model (tools/ghidra_query.py xrefs) instead; it found the real writers of DAT_800c151c/1518/1520 immediately.

## Known failure modes

(none recorded yet)
