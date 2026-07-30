---
id: C006
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

The RE-16 reentry-seed/dispatch design is ARCHITECTURALLY WRONG for 0x8002A338: it converts the guest's O(1) jal/jr-$ra loop into O(N) nested C calls -- ~22,000 for one frame. The coroutine mechanism itself is correct (resume point 0x8002A424 held constant across 21,996 iterations); the cost is the defect. Demotion (inline + goto, keeping the loop flat) is the architecturally correct approach.

## Evidence

PSXPORT_DEBUG=coroentry with probes on 0x8002A338 and 0x8002A478, fix reinstated: 0x8002A478 entry #1..#4 ra=8002A424 (a seeded resume point) sp=807FFEFC; entry #21997 ra=03FF03FF sp=807FFF00 (guest frame already released, i.e. unwinding). Probe prints on CHANGE, so ra was 8002A424 for all 21,996 intervening entries.

## What would falsify it

if a flat (demotion/goto) emission of the same region also fails, the problem is not the call depth and this reopens; also if the iteration count turns out to be data-dependent and small for real content, the O(N) cost might be tolerable -- but 22k for one boot frame says otherwise
