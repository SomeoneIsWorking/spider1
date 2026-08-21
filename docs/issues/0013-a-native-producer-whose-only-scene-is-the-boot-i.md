---
id: 13
title: A native producer whose only scene is the boot-init frame cannot be pixel-gated — the window is 2 frames of black
status: open
symptom: producer-enabled vs producer-disabled A/B reports 0 pixels differing even though the producer provably ran (produced=2 clears=2 in band)
tags: render,producer,gate,negative-control,envelope
created: 2026-08-06
updated: 2026-08-21
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

## Why the new guest-frame debt still does not create a pixel gate

The user explicitly authorized actual guest-time GTE/OT output as a non-interpolated fallback for
graphics whose native producer remains unported. HACK-03 can therefore drive Native past `dem1` and
into `l1a1`, but it submits the WHOLE guest frame and mechanically skips every native producer for
that frame. Layering it over the frame envelope would double-draw shared frame state and would make
the pixel result unattributable, so the fallback refuses native overlap instead. Consequently it
does not extend the envelope producer's observable window: the only frames that exercise the native
envelope remain the two black boot-init frames. Forcing HACK-03 off restores the former call-#2
`dem1` abort, now named `DISABLED`.

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

### Note (2026-08-21)
2026-08-21 RE-21 advanced one dependency-ready step: executable disassembly plus the live source-boundary probe decoded the first observed mesh face as a 28-byte direct-textured quad with four source vertex indices, UVs, CLUT and texture page (C039; scratch/logs/gate-boot-20260821-025743.log). This does not close the issue: no display-list producer exists yet, so the dem1 pixel gate is still unavailable.

### Note (2026-08-21)
2026-08-21 RE-21 advanced its next dependency without adding a producer: retail disassembly and the source-boundary validator establish the zero-rotation/unscaled object-local-to-camera contract (C040/I034). The live log scratch/logs/gate-boot-20260821-032403.log matched (objectPosition20p12 sra 12)-cameraPosition at both direct FUN_80077D64 callsites under distinct matrices. It also corrected a prior probe attribution: 0x8018BB90/flags 0x9000 was the outer list head, while actual first owner 0x8018BBB4 has flags 0x0000. This issue remains open because no display-list producer or meaningful pixel A/B exists.

### Note (2026-08-21)
RE-21 advanced through the exact retained face-cook/lifetime dependency without adding a producer. FUN_80068BB0 -> FUN_80074C98 cooks the raw Dem1_G first face in place; the copied post-cook 28-byte record later matched FUN_8007C4D8 byte-for-byte in scratch/logs/re21-mesh-cook-live-final.log, with one-word MISMATCH, MISSING, and UNLOADED opposite answers. The issue remains open: face flag 0x1000 enters the unresolved projection/cull/lighting/colour path, so a meaningful dem1 producer and pixel A/B are still unavailable.

### Note (2026-08-21)
HACK-03 adds only a mutually-exclusive whole guest-frame path. A bounded Native run reached `dem1`
and `l1a1` with `nativeSubmitted=0 interpolation=0`; the disabled and FPS60 controls refused at the
same `dem1` boundary as `DISABLED` and `INTERPOLATION_FORBIDDEN`. This does not close the issue: the
fallback deliberately skips the envelope, so it cannot supply an attributable dem1 envelope A/B.
