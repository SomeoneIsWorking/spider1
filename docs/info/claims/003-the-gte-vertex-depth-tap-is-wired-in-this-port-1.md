---
id: C003
kind: claim
status: holds
created: 2026-07-30
reconfirmed: 2026-08-24
tags: RE-08,depth,render
---

## Claim

The GTE vertex-depth tap is wired AND executes at scale in this port. Native per-vertex depth from the
guest's own swc2/mfc2 stores works end-to-end on the shipping default leg: over a bounded headless run
that reaches live 3D scenes (dem1/dem2/dem3/dem4/l1a1), ~61–64% of drawn primitives carry REAL
per-vertex depth and 84.6–86.0% of renderer depth lookups hit.

## Evidence (REPLACES the pre-2026-08-24 evidence; see history below)

MEASURED 2026-08-24 with a whole-run instrument that cannot alias (INST-29): `render_depth_coverage_report()`
(external/psxport/runtime/recomp/gpu_native.cpp — LIFETIME totals, never reset) now called from the port's
own submitFrame seam every 2048 calls (game/render/render_seam.cpp, kDepthReportEvery). Two independent runs:

    @2048 submits : 471561 of  741781 prims = 63.57% real-depth | cache 4186747 records, hits 1664616 / misses 270090  (86.04%)
    @4096 submits : 812421 of 1321727 prims = 61.47%             | cache 6726356 records, hits 2879363 / misses 508976  (84.98%)
    @6144 submits : 1354377 of 2223704 prims = 60.91%            | cache 11313797 records, hits 4774494 / misses 868877 (84.60%)

Run-to-run determinism: the second run's @2048/@4096 lines repeat the first's within ±1 primitive
(frame-boundary timing). Misses are ABSENT-dominated (496847 absent vs 12129 stale at @4096) and the
copy-carry bridge ran (2369181 carried of 69966073 copy sites = 3.39%). The report REFUSES to print a
percentage over zero classified prims ("NO PRIMITIVES WERE CLASSIFIED AT ALL … not 0% 3D"), so its
no-data case is distinguishable from a measurement by construction — the defect that invalidated the
old evidence does not exist in this one. Static denominators for the same day:
tools/re08_store_sites.py over MAIN + all 30 overlay modules — 2989 functions / 329101 words decoded,
77 swc2 screen-XY tap sites (dominant: FUN_8007C4D8 with 23), 10 mfc2->sw vertex taps
(FUN_8007B628/8007B798/8007B9CC/8007BE90/8007C2AC/80080988), 968 copy-carry sites,
593 swc2 stores of non-screen-XY cop2 registers left UNTAPPED by design.

## What would falsify it

A run reaching the same named 3D scenes whose periodic `[ndepth] depth coverage` lines report records
flat or hit%=0 while the prim denominator grows — i.e. the lifetime counters disagree with draws the
fcensus shows happening. Also falsified if the report line itself is shown to sample/reset per frame
again (regression against INST-29's mechanism).

## History — what this claim said before, and why it changed

2026-07-30 version: "tap wired but NEVER executed (records=0); the port never reaches 3D." Its
instrument was PSXPORT_DEBUG=ndepth, caught lying 2026-08-06 (INST-26): one-frame samples at fixed
frame parity, reset every present, zero/no-data collision — Spider-Man draws on alternate fields, so
every sample landed on a non-drawing field. That flag left the falsifier unevaluable. The codemap's
2026-08-06 refresh removed the false "port never reaches 3D" reason (the port demonstrably reaches
rooftop gameplay) and named re-measurement as RE-08's first move. Done 2026-08-24: re-measured with
the whole-run report above; the "records=0" reading was entirely the instrument. The old conclusion
is deleted, not annotated away — the Claim section above states the current truth.
