---
id: I005
kind: instrument
status: trusted
created: 2026-08-04
---

## Instrument

tools/check_reloc_model.py — do the .rel relocation shapes support base-relative emission, across all 30 CD.WAD modules

## Validated by

Ships --selftest, which feeds a synthetic module per check that MUST trip it (HI16 on a non-lui, J26 on a non-jump, LO16 on a non-immediate opcode, an offset named twice, an orphan LO16) plus a well-formed POSITIVE control that must stay clean — all 6 fire correctly. It also refuses rather than returning empty: a missing CD.HED/CD.WAD, or an index with no .bin/.rel pair, exits 1 saying NOTHING was checked. Every negative prints its denominator, and the one blind spot (the LO16 pairing scan is not flow-sensitive) is printed on every run. CAUGHT ITSELF LYING ONCE: a 64-instruction backward window reported 52 false UNPAIRED sites; the real maximum lui->lo16 distance in this game is 803 instructions and the pointer is copied between callee-saved registers, so the scan had to become unbounded and move-following.

## Known failure modes

(none recorded yet)
