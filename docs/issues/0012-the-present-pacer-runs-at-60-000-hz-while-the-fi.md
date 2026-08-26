---
id: 12
title: the present pacer runs at 60.000 Hz while the field clock runs at 59.940 Hz: the beat drops ~1 rendered frame in 3 for ~1.8 s out of every 16.7 s
status: open
state_items: S009
symptom: windowed gameplay judders/flickers in a recurring ~2 s burst roughly every 17 s; presents arrive as a 1 ms pair followed by a 32 ms gap instead of one every 16.68 ms; new pictures reaching the screen fall from 29.0/s to 19.7/s inside a burst
tags: pacing,temporal,flicker,windowed,presentclock,gpu_pace_subframe,vblank_advance,beat,framework-wart,MAILBOX
created: 2026-08-06
updated: 2026-08-06
---

FOUND 2026-08-06 (step G6, reproducing the user-reported Spider-Man flicker). This is a TEMPORAL
defect, not a pictorial one. See the "what this is NOT" section at the bottom: across 390 consecutive
presents in 3 scenes the PICTURE never alternated.

## Symptom, measured

Windowed, `PSXPORT_PAD_REPLAY=replays/bugs/pause-corruption.pad`, `PSXPORT_DEBUG=presentclock`.
Present submissions are timestamped by the new game-side `presentclock` channel
(`game/core/sync_native.cpp`, brackets `gpu_present` so submission cadence and present cost are
separate numbers).

Steady window t=10..95 s, 5096 presents: mean rate 59.95/s — correct — but the SPACING is bimodal.
87.8% of gaps are a clean 16 ms; 4.77% are under 4 ms and 4.97% are over 25 ms. They come in
alternating pairs, sustained:

    n=4943  t= 82.8000s dt= 0.98ms  vbl=4963  late vs its field boundary = +0.65ms
    n=4944  t= 82.8324s dt=32.35ms  vbl=4964  late = +16.33ms
    n=4945  t= 82.8337s dt= 1.36ms  vbl=4965  late = +1.00ms
    n=4946  t= 82.8657s dt=31.97ms  vbl=4966  late = +16.29ms
    n=4947  t= 82.8666s dt= 0.84ms  vbl=4967  late = +0.45ms

Every second present is a FULL FIELD late. The port submits two frames 1 ms apart, then nothing for
32 ms. This is not continuous: it arrives in bursts of ~1.8 s, and the bursts are PERIODIC.

## Root cause

TWO CLOCKS RUNNING AT DIFFERENT RATES, one on each side of the same wait loop.

  * `gpu_pace_subframe` (external/psxport/runtime/recomp/gpu_native.cpp:1568) sleeps to a deadline
    advanced by `quota * 1000.0 / 60.0` ms — exactly **60.000 Hz**.
  * `vblank_advance` (game/core/sync_native.cpp) derives the field count from elapsed real time at
    `60000/1001` — exactly **59.940 Hz**, the NTSC field rate this US release actually runs at.

The pace loop is `while (counter < target) { gpu_pace_frame(c); vblank_advance(c); }`: the pacer
decides WHEN to wake, the field clock decides whether a field has elapsed. They slip by
16.67 us per field. When the accumulated slip reaches one whole field period the pacer starts waking
just BEFORE the field boundary, so one iteration presents nothing and the next presents two fields'
worth — the 1 ms + 32 ms pair. The slip then keeps accumulating and the pattern slides out again.

PREDICTED beat period, from arithmetic alone with no fitted parameter:
    1 / (60 - 60000/1001) = 16.683 s
MEASURED interval between burst centres, steady state:
    run1: 16.58, 16.86, 16.62, 16.78, 16.59, 16.55   (mean 16.66 s)
    run2: 16.67, 16.87, 16.59, 16.90, 16.48, 16.88   (mean 16.73 s)
Within 0.5% of the prediction, in two independent runs.

The median gap is a second, independent fingerprint of the same two clocks:
paced = 16.666 ms (the pacer's 60.000 Hz), unpaced = 16.683 ms (the field clock's 59.940 Hz).

## Ablation — the same instrument, same mode, produced the failing answer

Three legs, identical replay, identical env, same 10..95 s steady window, same `presentclock`:

| leg                                   | presents | rate/s | median dt  | paired <4 ms | gaps >25 ms | periodic bursts |
|---------------------------------------|----------|--------|------------|--------------|-------------|-----------------|
| A  PACED, field 59.94 Hz  (as shipped) | 5096     | 59.95  | 16.666 ms  | 243 (4.77%)  | 253 (4.97%) | 5, gaps 15.1/16.7/16.9/16.6 s |
| B  NOPACE, field 59.94 Hz  (control)   | 5095     | 59.94  | 16.683 ms  |  10 (0.20%)  |  12 (0.24%) | 0 |
| C  PACED, field 60.000 Hz  (ablation)  | 5100     | 60.00  | 16.667 ms  |  13 (0.25%)  |  16 (0.31%) | 0 |

Leg C is the decisive one: it KEEPS the pacer and removes ONLY the rate disagreement (one character,
`1001` -> `1000` in `vblank_advance`, reverted afterwards). The defect collapses 4.77% -> 0.25%. So
the cause is the RATE MISMATCH, not the existence of the pacer and not scheduling noise.

## What it costs the picture

Joined `presentclock` with `presentskip` (per-present rebuild/reuse) in one run, 5995 presents over
t=10..110 s. 27.60 rebuild_geom/s, i.e. each rendered frame is presented ~2.17 times.

The swapchain is MAILBOX (`[gpu_vk] swapchain present mode: MAILBOX`), so a present submitted 1 ms
after its predecessor replaces it before scanout and the earlier one is never seen. INFERRED from the
present mode, not measured — the compositor's scanout is not observable from inside the process.

  * 221 paired submissions in the window; in 70 of them (31.7%) the DISCARDED present carried a
    newly-rendered picture. Baseline rate of newly-rendered presents is 46.7%, so the pairing is NOT
    phase-locked to the render cadence — WHICH rendered frames get dropped is arbitrary, which is
    what makes it read as judder rather than as a clean halving.
  * Inside burst seconds (19 s of the 100 s window): new pictures reaching the screen fall to
    **19.7/s**, against **29.0/s** outside. A 32% drop, ~1.8 s long, every ~16.7 s.

## What this is NOT — the pictorial hypothesis is negative, with its denominator

390 consecutive presents captured at the PRESENT stage across 3 named scenes, each analysed by
`tools/present_flicker.py` from the run's own `[present_shot]` manifest (never a directory glob):

  * pause screen, resting (150 presents, `PSXPORT_PRESENT_BURST=3600:150:...`):
    1 distinct picture, 546 colours every present, 0 of 149 consecutive pairs differ.
  * rooftop gameplay (120 presents, burst 1500): 37 distinct pictures, 2 revisits, hold lengths
    4,4,4,4,4,5,4,4,... — strictly-new pictures, NO A-B-A-B alternation.
  * front-end / loading (120 presents, burst 500): 2 distinct pictures, holds 1/32/32/55 — a ~2 Hz
    blinking indicator, not flicker.

NOT SAMPLED, so nothing here speaks to them: in-game cutscenes, FMV->gameplay transitions, indoor
levels, water, damage/hit overlays, and any scene this one recorded replay does not reach.

## Proper fix (NOT applied — this entry is a reproduction, not a repair)

`gpu_pace_subframe` must pace on the SAME clock the consumer counts fields on. The 60.0 divisor is a
framework constant that assumes 60 Hz for every game; the field rate is a per-game property
(`kFieldRateMilliHz = 59940` already exists in this port and is already handed to
`rec_host_turn_register`). The fix is to feed the pacer that same rate rather than a literal 60.0 —
framework-side, `external/psxport/runtime/recomp/gpu_native.cpp`, and it needs a claim under
external/psxport/docs/workspace/PROTOCOL.md. Deliberately not done here: G6 was scoped game-side/measurement-only, and the two
framework claims open this session own other files.

Do NOT "fix" this by changing `vblank_advance` to 60.000 Hz. That would make the beat disappear by
making the port's field clock wrong — 59.94 is the hardware rate this game's timing is derived from,
and every animation paced by the guest's vblank counter would then run 0.1% fast. It is the ablation
that identified the cause, not a candidate repair.
