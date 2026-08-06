---
id: I023
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_DEBUG=presentclock (game/core/sync_native.cpp) — per-present WALL-CLOCK submission spacing + present cost

## Validated by

TWO-CLASS: it printed BOTH answers on the same binary, same window, same 10-95 s steady window, same replay. Paced-as-shipped: median gap 16.666 ms, 4.77% of gaps under 4 ms, 4.97% over 25 ms, 5 periodic bursts. PSXPORT_NOPACE=1: median 16.683 ms, 0.20% under 4 ms, 0 bursts. It also brackets gpu_present, which is what makes it trustworthy: a POST-ONLY timestamp (the first version) reported the same 3ms/30ms alternation as a submission defect when present COST was in fact a flat 0.5 ms median — the two are separated on the line (dt_us vs cost_us) rather than inferred apart. Counters are incremented unconditionally on the present line, so an absent channel cannot be mistaken for absent presents. BLIND SPOT, bounded by grep not assumption: this is the sole gpu_present call site reachable from this port's frame loop; the framework's gpu_clear_display (FMV/splash teardown) and native_fmv's gpu_vk_present_image are NOT counted, so intro-FMV presents are invisible to it.

## Known failure modes

(none recorded yet)
