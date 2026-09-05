---
id: I027
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

The render seam's own reach + scene census (PSXPORT_DEBUG=rseam / the unconditional [rseam] and [scene] lines, game/render/render_seam.cpp)

## Validated by

WHAT IT ANSWERS: (a) did the override at 0x80061308 actually install and run, and (b) what scene identity did the game hold when it ran. The watchdog owns SIGINT/SIGTERM and calls `_exit(130)` (`external/psxport/runtime/psx/watchdog.cpp`), so the evidence was emitted during the run with explicit call and scene-change denominators. BOTH CLASSES OBSERVED 2026-08-06: the positive reached call #1 at frame 2 from ra=80061218; the negative first sample reported no identity before later reporting dem1 and l1a1. BLIND SPOTS: it counted only non-recursive entries and saw only submitFrame, so it said nothing about intro-FMV frames or within-level state. The retired registration machinery is not part of the dynamic product; an equivalent image-aware runtime census is still required.

## Known failure modes

(none recorded yet)
