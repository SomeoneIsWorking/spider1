---
id: I018
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_DEBUG=geomwatch — the camera-projection probe (game/core/diag_overrides.cpp diag_geom_setup on 0x80075D0C). Super-calls the guest body, then prints ProjParams valid/OFX/OFY/H, and on call #1 CALLS requireGeom() so the gate is executed rather than reasoned about. Prints an unconditional ARM line carrying the pre-call state, and caps by NOVELTY (first 8 calls + every change of the triple), not by first-N.

## Validated by

RUN AGAINST BOTH CLASSES, not reasoned about. POSITIVE (GameConfig hle.setGeomOffset=0x8008BF24 / setGeomScreen=0x8008BF14): 'valid=1 OFX=256 OFY=120 H=276' and 'GATE PASSED: requireGeom() returned OFX=256 OFY=120 H=276 without aborting' — scratch/logs/g7/run_gate.log, run_fix.log. NEGATIVE (identical binary path, identical run conditions, ONLY those two addresses set to 0 — the recompiled leaves still write the GTE so the picture is unchanged): 'valid=0 OFX=0 OFY=0 H=0' and requireGeom ABORTS with 'SetGeomOffset (OFX/OFY) NEVER RAN — SetGeomScreen (H) NEVER RAN' plus a backtrace — scratch/logs/g7/run_control.log, run_gate_control.log. Corroborated by an independent channel: [plat-hle] reports 8 primitives installed in the positive leg and 6 in the negative. BLIND SPOTS, stated: it observes only 0x80075D0C, so a projection set by any other path would be invisible (none exists — xrefs plus a raw whole-image jal scan agree there is exactly one caller of each leaf); and it only ever saw the boot/front-end viewport in a 70 s headless run, so it has NOT been shown to catch a per-viewport H change.

## Known failure modes

(none recorded yet)
