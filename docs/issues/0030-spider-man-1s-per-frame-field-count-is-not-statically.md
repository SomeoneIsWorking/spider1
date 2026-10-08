---
id: 30
title: Spider-Man 1's per-frame field count is not statically known, and the one live run measured the port's cadence rather than the guest's
symptom: the interpolated-60fps scope question for this title needs the guest's own per-frame field count, and neither a static reading of the image nor a run of the current product produces it
tags: cadence,frame-rate,vsync,native-owner,dead-tap
created: 2026-09-29
updated: 2026-09-29
---

## Status: open, and the scope of S009 is therefore undecided

The guest's own per-frame field count has not been established, for two independent reasons.

## Why it is not a static fact

The frame body waits through its own `FUN_8005E748` rather than through VSync, and one of its two
wait sites sits inside a back-edge loop (`0x8002C29C`) whose trip count a `DrawSync(1)` GPU fence
decides at run time. So the count is a function of GPU state, not a compile-time constant, and no
reading of the image alone can answer it. Every field-wait call site passes the literal 1, which is
consistent with both 30 and 60.

## Why a run of the current product does not answer it either

A paced run of the product reports 1.0000 display fields per presented frame at 59.71 presents per
second. That number is THIS PORT's cadence, not retail's, and the reason is structural rather than
inferred: `0x8005E748` (the guest's field wait) and `0x8002C354` (the outer selector) are native
overrides in this port, and `0x8002C174` — the guest's own frame loop, whose second wait site is the
loop above — is referenced nowhere in the title tree and never runs. The host supplies `quota=1` to
the framework's frame pacer. So the guest's cadence is not observable on a build where the guest's
cadence has been replaced.

Consequence: S009 may neither be closed as out-of-scope nor started as in-scope. The rate is not
"probably 30 fps because every other measured title here is"; it is unmeasured, for a named reason.

## What would settle it

Two changes, both blocked on the black-picture frontier rather than on measurement:

1. `0x8002C174` executes as translated guest code, so the guest's own frame loop runs.
2. `0x8005E748` is served as a poll on the guest's field counter `[gp+0x0C74] = 0x800B5468`
   (recovered from the crt0's own bytes; it has exactly one `+1` writer, at `0x8005E53C` over
   186,880 text words) rather than as a host field injection.

With both, one bounded headless run reporting game-frames-per-second and the per-frame advance of
that counter together answers the question.

## Falsifier

If, once `0x8002C174` runs, the counter advances by exactly 1 per game frame while game frames per
second is ~30, then the inner `DrawSync(1)` loop never iterates twice in practice, the rate IS one
field per frame, and Spider-Man 1 needs **no** interpolation at all. That is the most likely way
this issue is wrong.
