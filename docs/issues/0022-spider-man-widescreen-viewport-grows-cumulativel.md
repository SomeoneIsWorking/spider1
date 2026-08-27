---
id: 22
title: Spider-Man widescreen viewport grows cumulatively after first world-render call
status: resolved
symptom: live dem1 mapping grows 512->684->912->1024 instead of remaining 512->684 each frame
state_items: S008
tags: widescreen,viewport,culling,projection,spiderman1
created: 2026-08-27
updated: 2026-08-27
---

## Root cause

`Spider1Widescreen` left its widened horizontal bounds in the guest viewport descriptor after the
retail world renderer returned. The retail body rewrote the lens divisor to its native value but
did not restore those bounds. The next host call therefore observed a mixed descriptor — widened
width plus native lens — which did not equal either the saved native or previously applied tuple.
The publisher treated it as a genuine viewport change and widened it again. The first live reach
made the sequence explicit: `512x240 -> 684x240`, then `684 -> 912`, then `912 -> 1024`.

## What was tried / dead ends

The earlier unit test covered the width/lens formula for an isolated 512-wide input. That formula
was correct and could not detect descriptor lifetime or a partial retail rewrite between calls.
Do not clamp the result at 1024 or remember the first-ever viewport forever: the first masks the
feedback loop, while the second would discard legitimate retail viewport changes.

## Resolution

The host now treats left, right and lens divisor as one scoped input tuple. It snapshots the
current retail values, applies the projected tuple only while super-calling `gen_func_80075D0C`,
then restores all three original inputs together. Mapping history remains diagnostic state only and
cannot become the next projection baseline. The static ownership gate verifies one apply and one
restore for every field around the shipping retail super-call.

Final opposite-answer evidence: real-disc PID 3090229 exited 0;
`scratch/logs/spider1-wide-scoped-final.log` contains exactly one stable
`512x240 -> 684x240` mapping across the repeated `dem1` corpus and reconciles 5,150/5,150 fences.
The inspected frame contains live demo character and text output.
