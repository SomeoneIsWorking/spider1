---
id: 13
title: A native producer whose only scene is the boot-init frame cannot be pixel-gated — the window is 2 frames of black
status: open
symptom: producer-enabled vs producer-disabled A/B reports 0 pixels differing even though the producer provably ran (produced=2 clears=2 in band)
tags: render,producer,gate,negative-control,envelope
created: 2026-08-06
updated: 2026-08-06
---

## What happened

The frame-envelope producer (game/render/frame_envelope.cpp) is the FIRST native producer behind
this port's render seam, chosen by the abort order: the native leg's first abort named scene '....',
and the RE-21 census (PSXPORT_DEBUG=fcensus) measured that scene's ENTIRE display list as libgpu's
own chain-terminator packet — a GP0(80) 2x1 copy of (0,0) onto (0,0) — plus four GP0(00). No
geometry. So that scene's whole picture is the envelope: the GP1(05) page flip, the GP0 E3/E4/E5/E1/
E2/E6 drawing state, and the GP0(02) background clear that PutDrawEnv performs when isbg is set.

The required gate is 'pixels changed producer-enabled vs producer-disabled, captured where the layer
is on screen'. It measured **0 of 524288**, and the producer was NOT inert:

* enabled  -> '[rseam] native producers reached this run: frame envelope produced=2 clears=2'
* disabled -> '[envelope] SUPPRESSED build ... clears=0' and 'produced=2 clears=0'

## Why (measured, not inferred)

At the abort — the last instant of the producer's window — the whole 1024x512 CPU VRAM was dumped
and split by region:

    framebuffer page A  x0-511  y0-239   : 0 non-black
    framebuffer page B  x0-511  y256-495 : 0 non-black
    texture area        x512-1023 y0-511 : 115001 non-black

Both framebuffer pages are entirely black, so the clear had nothing to clear and the page flip moved
between two identical black pages. GP1(08) is not a discriminator either: the boot FMV path programs
the display mode directly, so both legs log the same '[gpu] display depth -> 15-bit (GP1(08)=
08000002, 512x240)'.

## Why the native leg cannot be driven somewhere better

The seam aborts at the first scene with no display-list producer, which is 'dem1' at submitFrame
call #2. That abort is the designed behaviour and must NOT be softened — a leg that renders past a
scene it cannot draw is exactly the plausible-looking fallback external/psxport/docs/workspace/PROTOCOL.md forbids. So the
producer's window is genuinely two frames, both black, until a 'dem1' producer exists.

## What was used instead, and its limits

Word-exactness against libgpu, on the reference leg where the guest's own DR_ENV packet exists to
compare against: checked=2560 mismatch=0, with a one-bit perturbation control taking mismatch to
100% (claim C033, instrument I029). That proves the ARITHMETIC, and says nothing about whether those
words reach the screen — which is precisely what claim C034 records as unproven.

## What closes this

The first 'dem1' display-list producer (RE-21; the emitter chain is already attributed —
FUN_8002BD5C -> FUN_80076480 -> FUN_80077D64 -> FUN_8007C4D8 -> FUN_8007D978). The native leg then
survives into frames with real content, and this A/B must be re-run in dem1 calls #2..#728. If it
STILL shows zero there, the envelope really is inert and C033 is measuring something that never
reaches the screen.
