---
id: I026
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_WWATCH=<lo>,<hi> as a POINTER-PROVENANCE reader (external/psxport/runtime/psx/mem.cpp Core::wwatch_check_slow) — read a runtime-allocated guest pointer WITHOUT adding any code

## Validated by

VALIDATED 2026-08-06 (scratch/re12/logs/wwatch_ot.log). Question: what are the runtime values of Spider-Man's OT and packet-pool pointers, which are heap-allocated and therefore absent from the load image? PSXPORT_WWATCH=8009A754,8009A7D4 over a 45 s headless boot logged 1551 stores in range and showed the four slots of interest written EXACTLY TWICE each and never again: 0xFFFFFFFF by pc=0x80061124, then ot=0x800C65EC / 0x800CA5F4 and pool=0x800CE5FC / 0x800E5604 by the allocator at pc=0x80064FA0 with ra = 0x80061268 / 0x80061280 / 0x80061298 / 0x800612A0. CROSS-CHECK, which is what makes this trustworthy rather than merely plausible: those four ra values are the four call sites the Ghidra decompile of FUN_80061230 shows, IN ORDER, and the sizes (0x4000 ot, 0x17000 pool) match the ClearOTagR(ot, 0x1000) and DrawOTag(ot+0x3FFC) the same RE predicts. Two independent derivations agreeing. CAN IT SHOW THE OTHER ANSWER: yes — the same run logged 1551 hits across the whole watched range including stores it was not looking for, and each line carries the address, so a slot that was rewritten every frame would have been visible as repeated lines. A slot NEVER written would have produced no line for that address, which is distinguishable because the surrounding addresses did produce lines (the denominator is visible in-band). LIMITS: it is a STORE watch, so it cannot see a pointer that is only ever read; the range is a single [lo,hi) so unrelated neighbouring stores are noise you must filter; and it logs the guest pc/ra, which go stale under natively-executed bodies (use PSXPORT_WWATCH_BT=1 for a host backtrace there).

## Known failure modes

(none recorded yet)
