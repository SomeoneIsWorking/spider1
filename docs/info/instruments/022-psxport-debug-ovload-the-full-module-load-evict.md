---
id: I022
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_DEBUG=ovload — the FULL module load+evict census (framework external/psxport/runtime/recomp/overlay_router.cpp:156 'live at' and :170 'evicted from'). Use this, NOT PSXPORT_DEBUG=module, whenever the question is 'which modules are resident NOW'

## Validated by

2026-08-06. It has NO silent half: it prints placement AND eviction, so the resident set can be reconstructed exactly. That is why it supersedes PSXPORT_DEBUG=module for residency questions — game/core/module_loader.cpp logs only load(), so the same 230s run reported 5 events on the 'module' channel (scratch/logs/g8/module_census2.log) and 8 on 'ovload' (scratch/logs/g8/ovload_census.log); the 3 missing ones were all EVICTIONS, and reading only the 'module' channel would have made the resident set look monotonically growing when SHELL is in fact evicted at ~present 620 and never reloaded. BOTH ANSWERS SHOWN in one run: 8 events inside the first ~780 presents, then exactly 0 over the remaining ~13000 — a live channel producing a genuine zero, not a channel that was off. NEGATIVE CARRIES ITS DENOMINATOR: quote the present count the census covered, or 'no module changes' is indistinguishable from 'the run was 3 seconds long'. BLIND SPOT: it reports CODE-module residency only; it says nothing about which scene/substate is running inside a resident module, which is exactly why it is a weak scene discriminator (C026).

## Known failure modes

(none recorded yet)
