---
id: I014
kind: instrument
status: DISTRUSTED
created: 2026-08-05
distrusted_on: 2026-08-05
---

## Instrument

The frame-progress watchdog (PSXPORT_WATCHDOG=<sec>) as a GUEST-progress gate — watchdog_pet() from gpu_present_ex, external/psxport/runtime/recomp/gpu_native.cpp:1399

## Validated by

It is validated for what it was built for and INST-03 records that: across this port's successive stalls it named DIFFERENT locations (the libetc VSync deadline loop, then the disc-init retry loop), which is the behaviour of a working instrument. What it was never validated for is the use it has been put to — 'ran N frames, 0 abort' quoted as evidence the GUEST advanced. It is petted from the PRESENT path, and presents are driven by the host-turn timer independently of guest progress, so the pet has no guest-side denominator at all

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-05

STRUCTURALLY INCAPABLE of reporting guest starvation, which means every gate result of the form '0 abort, ran N frames' inherits a blind spot. MEASURED 2026-08-05: a windowed run produced presents=4027 with rebuild_geom=0, rebuild_vram=0 and vram_writes=0 — the guest did nothing for 4027 presents — and the watchdog never fired, because watchdog_pet() sits in gpu_present_ex (gpu_native.cpp:1399) and the host-turn timer keeps presenting regardless. A silence from this instrument means 'presents kept happening', never 'the game is running'. TRUST IT FOR: a hard hang where the present loop itself stops, and for its backtrace when it does fire (with INST-03's caveats — single sample, corroborate against disassembly). DO NOT TRUST IT FOR: guest liveness, boot progress, or as any part of a pass/fail gate. THE MISSING INSTRUMENT: a watchdog fed by a GUEST-side counter (guest vblank count, VRAM writes, or recompiled-function dispatches) with the denominator printed, so a negative carries 'guest advanced 0 of N'. Until that exists, quote presentskip counters (vram_writes / rebuild_geom) rather than the frame count. See issue 0005, C021

> Every result this instrument produced is suspect until it is re-validated.
