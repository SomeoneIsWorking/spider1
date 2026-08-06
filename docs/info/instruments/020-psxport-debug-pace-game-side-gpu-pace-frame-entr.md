---
id: I020
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_DEBUG=pace — game-side gpu_pace_frame entry counter (game/core/sync_native.cpp), split by the two call sites, printed with the guest vblank counter and the requested wait length n

## Validated by

VALIDATED AGAINST BOTH CLASSES, RUN not reasoned about. Same binary family, two builds differing only in GameConfig::paceQuota, both WINDOWED, 260 s each.
POSITIVE-FOR-BROKEN (quota=2): 7650 pace lines parsed of 23235 log lines; 30.00 entries/s; 578 of 599 consecutive field-wait entries advanced the guest vblank counter by 2 while the request was n=1; median inter-entry spacing 33.30 ms.
POSITIVE-FOR-FIXED (quota=1): 15061 pace lines of 30594; 60.00 entries/s; 599 of 599 entries advance by exactly 1; median spacing 16.70 ms.
The two readings are cleanly separated in both directions, so the instrument is not stuck on one answer.
WHAT ITS NEGATIVE LOOKS LIKE: the two tallies are incremented UNCONDITIONALLY, on the same lines as the pace calls (lucent::debug does not evaluate its arguments when the channel is off, so a counter bumped inside the log call would only count while someone was watching). 'pace lines present but entries flat' therefore means the loops are spinning without pacing; 'no pace lines at all' means the channel is off, the build predates the instrument, or no field wait was ever entered. scratch/g1_pace/analyze.py prints 'PARSED 0 pace line(s)' and exits non-zero rather than printing a rate over an empty parse.
BLIND SPOT, bounded by grep not by assumption: it counts only the two call sites in game/core/sync_native.cpp. The framework's other gpu_pace_frame callers are native_boot.cpp / native_stub.cpp (the native frame loop, which this port does not run -- it runs the guest's own loop) and fps60.cpp (unreachable here: eligibility requires RenderQueue::drawWorldQuad, which this repo never calls).
HARD PRECONDITION: WINDOWED ONLY. gpu_pace_subframe returns immediately on !gpu_has_window(), so every entry a headless run logs represents a no-op and every headless pacing number is about a program the user never runs.
COMPANION: the log has no timestamps of its own -- rates must come from a timestamped capture (scratch/g1_pace/ts.py) or they are counts divided by an assumed duration.

## Known failure modes

(none recorded yet)
