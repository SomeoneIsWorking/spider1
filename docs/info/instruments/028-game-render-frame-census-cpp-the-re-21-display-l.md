---
id: I028
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

game/render/frame_census.cpp — the RE-21 display-list inventory (PSXPORT_DEBUG=fcensus / fcensusv)

## Validated by

WHAT IT ANSWERS: what a given submitFrame call is actually being asked to draw — OT nodes traversed, primitive words, and a per-GP0-class census (poly3/poly4 with tex/gouraud/semi sub-counts, line, rect, sprite, fill, VRAM copy, self-copy, upload, env, nop), alongside the DRAWENV and DISPENV the same call submits. fcensusv adds a per-primitive word dump capped BY NOVELTY (every primitive while the frame is small, then the first of each opcode), so a 4400-node frame shows one example of every class instead of 48 copies of the first. BOTH CLASSES OBSERVED 2026-08-06: it reports 59 pixel-writing primitives for the dem1 attract frame and 0 for the boot-init frame in the SAME run, so a zero is a measurement and not a silence. THE NEGATIVE IS DESIGNED: every line carries nodes= (the denominator), term=yes/no (a chain that did not terminate inside 8192 nodes makes the counts a LOWER BOUND and says so), and unknownOp= (a GP0 byte the length model does not cover, which under-counts the rest of that node). It found the fact the first producer rests on: the boot-init frame's whole display list is one GP0(80) 2x1 copy of (0,0) onto (0,0) — libgpu's own chain terminator — plus four GP0(00). BLIND SPOTS: it walks the OT the guest built, so it is blind to anything drawn outside this seam (the intro FMV path); it does not decode WHAT an object is, only its GP0 class; and it must never be used to resolve a producer's geometry — it exists to size the backlog.

## Known failure modes

(none recorded yet)
