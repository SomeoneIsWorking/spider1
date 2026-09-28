---
id: 30
title: Spider-Man 1's per-frame field count is NOT statically determinable, and that is the finding — the frame body waits through its own routine inside a GPU-fence loop, not through VSync
status: open
symptom: The workspace map recorded "`spider1`: `VSync(0)` x3, `VSync(-1)` x7 — **no waiting call at all** -> **unknown**", correctly refused to conclude, and left the title's lerp scope undecided. This issue answers it as far as the bytes allow, and says exactly what is left.
tags: cadence,vsync,frame-rate,presentation,interpolation,lerp,bytes
created: 2026-09-28
updated: 2026-09-28
---

## Answer

**NOT ESTABLISHED — and the honest shape of that is the result.** The title is neither confirmed
30 fps nor confirmed 60 fps. It must not be put into, or ruled out of, interpolation scope on the
strength of this run.

## What IS established, from bytes

**1. The game does not pace through VSync at all.** Its own field wait is
`FUN_8005E748(n)`, which spins until a field counter has advanced by `n`. The counter is
`[gp+0x0C74]`, and it has **exactly one writer**, `FUN_8005E510`, which adds exactly `+1`
(scanned 186,880 words, matched 1).

**2. The unit is a FIELD, by the game's own construction.** The VBlank handler at
`0x8008C2E8` adds 1 to the field counter at `0x800B397C` per field, then dispatches 8 callbacks
from the table at `0x800B395C`; the registrar at `0x8008C354` writes that same table. So
`FUN_8005E748(1)` is a one-field wait.

**3. `VSync`'s argument semantics hold here too, read from this image.** VSync is at
`0x80084C38`, found by shape (23 `bgez $a0` candidates scanned, matched 1):
`bgez a0` / `beq a0,1` / `blez a0` for the no-wait cases, `addiu a1,a0,-1` for `a0 >= 2`.
So `VSync(0)`, `VSync(1)` and `VSync(-1)` carry **no** rate information in this title either.

**4. Every field-wait call site in the image passes the literal 1.** All **15** `jal
FUN_8005E748` sites resolve to `addiu $a0,$zero,1`, matched 15 of 15.

## Why the number cannot be reduced to one value

The frame loop is `FUN_8002C174`. Its **head is not its entry** — `0x8002C174` runs a prologue
and an init block, and the per-frame body is a back-edge to `0x8002C1E0` (`j 0x8002C2F0`).
Body extent `0x8002C1E0..0x8002C2F0`, 69 instructions. Inside it:

```
8002C26C  jal 0x8005E748   ; n=1   -- conditional, a branch can skip over it
8002C274  sw  $zero,0x5474($at)
8002C27C  j   0x8002C28C
8002C284  jal 0x8005e234            <-- INNER LOOP
8002C28C  jal 0x8005E748   ; n=1   <-- inside the loop
8002C294  jal 0x800819A4            ; DrawSync(1): a GPU fence
8002C29C  bnez $v0, 0x8002C284      ; spins until the fence clears
```

**The second wait site is inside a back-edge loop whose trip count is decided at run time by
`DrawSync(1)`.** So the per-frame field count is `1 + k` for a `k` decided by how long the GPU
takes, not a compile-time constant. The unconditional non-repeating part sums to **1**, and that
is a **lower bound**, not the rate.

An earlier pass at this instrument printed `FIELDS PER GAME FRAME = 2` and exit 0. That number
was wrong and the reason is worth recording, because it is the exact shape of the failure this
workspace has already shipped twice:

- the scan for inner loops looked only at `j`/`jal`, so a **conditional** back-edge read as
  straight-line code;
- the branch target was computed as `((disp & 0xFFFF) << 2)` with no sign extension, and then
  OR-ed under `0xF0000000` instead of added to `PC+4`. `bnez $v0,0x8002C284` (`0x1440FFF9`)
  became `0xFFFFFFE4` — outside the image — so no backward branch was found at all.

Both mistakes make a loop *disappear*, and a loop that disappears yields a confident, smaller,
wrong total. The fixed tool reports the loop.

## The VSync census is a real zero, and it is still not evidence of absence

Scanned **186,880 words / 7,373 call-form instructions; matched 0 direct `jal` sites** to
`0x80084C38`. Further, the image names that address **nowhere**: 0 `lui`/`addiu` materialisations
and 0 stored pointers, both scanned over the whole 749,568-byte file.

That is a count, not an absence proof, for two reasons:

- the image's **346 `jalr` sites are `jalr $ra,$vN` through function pointers** (jump registers
  `$v1` 152, `$v0` 146, `$t2` 5, `$t8` 8, and 35 others; `$t9` **zero**), and a function pointer
  table can be built at run time;
- **0 overlay images are provisioned**, and a module loaded at run time reuses addresses.

The prior session's `VSync(-1)` x7 figure came from a `jal`-only view of a *different* call
graph. It is superseded by this census, and the correction matters: a `jal`-only census over
this image reports **0 for every library routine**, which is a guaranteed answer.

## What would settle it

A **headless run** with the game clocked against measured wall time, reporting **two** numbers
together:

1. game frames per second, and
2. the advance of `[gp+0x0C74]` per game frame.

Both are needed. Frames/s alone cannot separate 30 from 60 without knowing the field rate, and
the counter delta alone says nothing about how many frames the CPU retires between waits. The
product's `PSXPORT_DEBUG=pace` channel already emits 98.4% of field-wait entries against
`FUN_8005E748(n=1)` (claim C024), so the counter delta is already instrumented; what is missing
is the game-frame count alongside it, over the same run.

**This is a live measurement away, not a blocked one** — the image is provisioned and the port
runs headless. It is not answerable offline, and claiming otherwise would be the confident wrong
number this workspace has shipped before.

## Instrument

`tools/re_cadence.py`, registered as `spider1_cadence_selftest` and `spider1_cadence`. It
reuses this repository's own `tools/probe_mips_image.py` decoder and branch-target formula
rather than adding a second. `--selftest` is **7/7**:

| case | result |
|---|---|
| positive | `VSync at 0x80084C38, 0 direct call sites, 0 materialisations, 1 repeating wait` |
| field counter's `+1` broken | `not incremented by exactly +1; refusing` |
| VBlank handler's store broken | `the VBlank handler is not as recorded at +0x2C` |
| `VSync`'s `a0==1` no-wait branch broken | `VSync+0x1C is 0x1082003B, not 0x1082003A` |
| `VSync`'s signature anchor broken | `matched 0 sites (scanned 22 bgez-a0 candidates)` |
| frame-loop head's test broken | `0x8002C1E0 is not the frame loop head's test` |
| **discriminator** | with the inner loop removed the **same tool** reports a definite **3 fields/frame** |

The discriminator is the one that matters. Without it, "not established" is indistinguishable
from a tool that simply cannot count — a refusal that is really a broken instrument being read
as a finding. With it, the refusal is demonstrably about these bytes.

## Falsifier

If a live run measures `[gp+0x0C74]` advancing by **exactly 1** per game frame while game
frames per second is ~30, then the inner `DrawSync(1)` loop never iterates twice in practice, the
rate IS 1 field per frame, and this issue's "not a compile-time constant" is true but
immaterial — the answer would be 60 fps, and Spider-Man 1 would then need **no** interpolation
at all. That is the most likely way this issue is wrong, and it is the measurement above.

Second falsifier: if the inner loop's back-edge at `0x8002C29C` is not reached in retail — for
instance if `DrawSync(1)` returns 0 on the first call under every real workload — the same
answer follows.

Third: if a provisioned overlay module contains a `VSync(n >= 2)` call in a frame path, the
"the game never uses VSync to pace" statement is false. The tool reports 0 overlays today and
says so.
