---
id: C028
kind: claim
status: holds
created: 2026-08-06
tags: flicker,render,negative-result
depends: tools/present_flicker.py
---

## Claim

Spider-Man's presented PICTURE does not oscillate in the front-end, rooftop gameplay, or the pause screen — 390 consecutive presents across those 3 scenes show zero A-B-A-B alternation

## Evidence

3 windowed captures at the PRESENT stage via the new game-side PSXPORT_PRESENT_BURST, each into its OWN directory and each analysed from that run's [present_shot] manifest rather than a glob. pause screen resting (150 presents, burst 3600): 1 distinct picture, 546 colours in every present, 0 of 149 consecutive pairs differ. Rooftop gameplay (120 presents, burst 1500): 37 distinct pictures, hold lengths 4,4,4,4,4,5,4,4,4,4,3,1..., 2 revisits, no alternation. Front-end/loading (120 presents, burst 500): 2 distinct pictures, holds 1/32/32/55 — a ~2 Hz blinking indicator. DENOMINATOR 390 presents = 6.5 s of content, 269,568,000 per-pixel comparisons.

## What would falsify it

This is a claim about THREE SCENES ONLY and dies the moment anyone samples a fourth. NOT sampled: in-game cutscenes, FMV->gameplay transitions, indoor levels, water, damage/hit overlays, or any scene replays/bugs/pause-corruption.pad does not reach. Also blind to anything the compositor does with the frames after submission — pictorial identity of consecutive presents does NOT mean the screen was stable, which is exactly why the temporal claim exists alongside it.
