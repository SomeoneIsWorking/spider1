---
id: I021
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

The 'entering native frame loop' log line (external/psxport/runtime/psx/native_boot.cpp:295) as a NATIVE-FRAME-LOOP REACHABILITY test — the cheap way to ask 'is this GameHooks member ever called?'

## Validated by

BOTH CLASSES RUN, 2026-08-06. It is an unconditional lucent::info (no channel gate), so absence is a result, not a missing knob. NEGATIVE class (spider1, Phase 0): 0 occurrences over a 230s windowed run reaching present 13757 — scratch/logs/g8/ovload_census.log. Sink-liveness control in the SAME run: the sibling lucent::info from the same file 65 lines earlier, 'entering native crt0 (PC-driven)', DID print (scratch/logs/g8/base_psx.log:30), as did '[boot] Phase 0: dispatching guest main() 0x8002C354' — so the absence localizes to bootInit not returning, not to a dead logger. POSITIVE class (Tomba2Engine, native frame loop owner): the same line IS present — grep -h 'entering native frame loop' ../Tomba2Engine/scratch/logs/*.log. USE: before implementing any GameHooks member, grep psxport for its call sites; if they are all inside native_step_frame, this line tells you in one run whether the member can ever fire. BLIND SPOTS: it says the loop was ENTERED, not that a particular hook inside it fired (dualview-gated drawOTag at native_boot.cpp:194 is inside the loop and still conditional); and it cannot see a hook reached from some future path outside native_step_frame.

## Known failure modes

(none recorded yet)
