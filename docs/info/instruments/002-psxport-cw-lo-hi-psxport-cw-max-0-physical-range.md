---
id: I002
kind: instrument
status: trusted
created: 2026-07-30
---

## Instrument

PSXPORT_CW=<lo>,<hi> + PSXPORT_CW_MAX=0 — physical-range store watchpoint (Core::cw_check_slow, mem.cpp); logs value + guest pc for every store landing in the range.

## Validated by

Run against BOTH classes in one session before trusting a zero: 0x80097DB8 (coroutine continuation slot) reports 0 hits, while the adjacent 0x80097DEC FMV table reports 2 with correct provenance (0x8002A858 pre-scan store, 0x80087220 CdPosToInt). Without that positive control a 0 is indistinguishable from a watchpoint that never armed — note the range is PHYSICAL (a & 0x1FFFFFFF), so passing a KSEG0 address silently never fires. PSXPORT_CW_MAX defaults to 64 and silently truncates late hits; set 0 for unlimited.

## Known failure modes

(none recorded yet)
