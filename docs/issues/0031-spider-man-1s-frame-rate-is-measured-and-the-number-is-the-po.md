---
id: 31
title: Spider-Man 1's frame rate is measured — and the number is the PORT's cadence, because the native frame owner replaced the guest's field wait
symptom: docs/issues/0030 left the title's rate NOT ESTABLISHED and named the way out: "a headless run reporting game-frames-per-second AND the per-frame advance of [gp+0x0C74] together". Both numbers are now taken, by an instrument registered in the gate. The rate is 1.0000 display fields per presented frame at 59.71 presents/second. **That is this port's own schedule, not retail Spider-Man 1's, and the reason is measured rather than inferred.**
state_items: S009,S004
tags: cadence,frame-rate,presentation,vsync,fields,native-owner,dead-tap
created: 2026-09-29
updated: 2026-09-29
---

## Answer

**The two numbers 0030 asked for are measured, and they are 1.0000 fields per presented frame at
59.71 presents/second. The title's RETAIL frame rate is still not established, and the reason is now
a measured fact rather than an open question: the port's native frame owner replaces the guest's
field wait `0x8005E748` and the outer selector `0x8002C354`, so the guest's own per-frame field
count — the thing 0030 could not reduce to a constant — is never consulted. The host supplies
`quota=1` and paces one display field per present, 57 times out of 57.**

The interesting part of this issue is not the number. It is that the measurement, the instrument, and
the answer all changed shape once the run was actually taken, twice, and both changes are recorded
below because both would have shipped a confident wrong rate.

## 1. The counter's address is recovered from bytes, and the tool refuses without its contract

`[gp+0x0C74]` is not a constant anyone can type in: the PS-X EXE header's `gp` field is **0** for
this image, so the address depends on where the guest's crt0 puts `gp`. The instrument reads that
from the image and refuses the shape if it is not the one named:

```
0x80087418  lui  $gp, 0x800B
0x8008741C  addiu $gp, $gp, 0x47F4        ->  gp = 0x800B47F4
0x80087438  jal  0x8002C354               (the game entry, immediately after)
```

so `[gp+0x0C74] = 0x800B5468`. Then it censuses the writers of `0x0C74($gp)` over all 186,880 text
words and **refuses to measure** unless there is exactly one, that writer is preceded by an
`addiu` of exactly `+1`, and the counter is read somewhere:

```
[image]     186880 text words at 0x80010000
[gp]        0x800B47F4 from 0x80087418/0x8008741C
[counter]   0x800B5468 = [gp+0x0C74]
[census]    1 store(s), 6 load(s), of 186880 words scanned
[census]    writer 0x8005E53C sw $?, 0x0C74($gp), preceded by an addiu of 1
  ok    exactly one store to 0x0C74($gp) (found 1 of 186880 words scanned)
  ok    that store at 0x8005E53C is preceded by an addiu of exactly +1 (found 1)
  ok    the counter is READ somewhere (6 loads), so a delta is meaningful
```

which is `FUN_8005E510`'s `lw $v0, 0x0C74($gp)` / `addiu $v0, $v0, 1` / `sw $v0, 0x0C74($gp)`, the
title's registered display-field callback, and `FUN_8005E748`'s three-word compare loop. **This
re-proves issue 0030's "exactly one writer, +1" with a corrected method** (below).

### A census defect in this tool, on its first pass, that is worth the space

The first store sweep enumerated opcodes `0x20/0x21/0x23/0x24/0x25/0x28/0x29` and **omitted `sw`
(0x2B)**. It reported

> `0x0C74($gp) == 0x800B5468, the field counter` — `loads: 6, stores: 0`

on a counter whose writer is a single `sw` at a known address. That is a confident zero produced by
an incomplete enumeration, which is the most believable possible wrong answer, and it is the fourth
such defect in this workspace's history (after `is3d`, the `VSync(0)` census, `OtAttr`, and the
`0x800B2886` gate). The opcode set here is the complete MIPS store set `{sb, sh, swl, sw, sd}` and
`--selftest` pins it by shape, not by count:

```
  ok    census: 0xAF820C74 is classified as a store (it IS sw $v0, 0x0C74($gp))
  ok    census: sh and sb are classified as stores too, so the set is complete
```

The general rule this workspace now has four instances of: **name the complete set the classifier
accepts, or a zero from it is a statement about the classifier.**

## 2. The measurement, both numbers, from one paced run

```
[run] PSXPORT_NOPACE=0  (PACED: this run can measure a frame rate)
[sample 1] presented frame=1   interp=0 total=1   [gp+0x0C74]=0x00000002  t+0.0s
[sample 2] presented frame=19  interp=0 total=19  [gp+0x0C74]=0x00000016  t+0.3s
[sample 3] presented frame=38  interp=0 total=38  [gp+0x0C74]=0x00000029  t+0.6s
[sample 4] presented frame=57  interp=0 total=57  [gp+0x0C74]=0x0000003C  t+0.9s
[sample 5] presented frame=76  interp=0 total=76  [gp+0x0C74]=0x0000004F  t+1.3s
[sample 6] presented frame=95  interp=0 total=95  [gp+0x0C74]=0x00000062  t+1.6s
[sample 7] presented frame=114 interp=0 total=114 [gp+0x0C74]=0x00000075  t+1.9s
[sample 8] presented frame=133 interp=0 total=133 [gp+0x0C74]=0x00000088  t+2.2s
[sample 9] presented frame=152 interp=0 total=152 [gp+0x0C74]=0x0000009B  t+2.5s
[run] product reaped (rc=130)

[measurement] whole window: 2.5 s, 151 real presents (151 total incl. in-betweens),
            field counter 0x00000002 -> 0x0000009B (delta 153)
[measurement] presents/second = 151/2.5 = 59.71  <-- ALONE THIS CANNOT SEPARATE 30 FROM 60
[measurement] fields per presented frame (whole window) = 153/151 = 1.0132
[measurement] steady window (first sample dropped): 2.2 s, 133 presents, field delta 133
[measurement] fields per presented frame (steady) = 133/133 = 1.0000  <-- CADENCE FIGURE
```

`interp=0` at every sample, so the `total` figure is not being inflated by interpolated in-betweens
— the counter the `frame` command reports and the counter this reads are the same one.

**Why both, and not either.** 1.0000 fields/present at 59.71 presents/second is 59.94 game
frames/second; 2.0000 at the same presents/second would be 29.97. The presents/second figure is
**the same number in both worlds**, which is exactly why 0030 said frames/s alone cannot separate
them. The instrument's selftest pins both ends of that pair and requires them to stay distinct:

```
  ok    verdict: 1 field/frame reads 59.94 fps
  ok    verdict: 2 fields/frame reads 29.97 fps
  ok    verdict: 1 field/frame says 60, 2 says 30 -- the discriminator the issue asked for
```

## 3. The first run gave 1.022, and the 0.022 was a boot transient

The first paced run reported 95/93 = **1.022**. It is not a cadence and it is not noise: the first
sample reads presented frame 3 against a field counter of 4, so the counter is already one ahead at
the first instant the control surface answers. Dropping that sample and re-running gives 133/133 =
**1.0000** exactly, and a second independent run gave 19/19 = 1.0000.

The tool now prints **both** ratios and names the basis, because the two failure modes are symmetric
and each is a confident wrong number: folding the transient into a cadence inflates it (1.022), and
dropping it without saying so hides a real measurement. `report()` states which window it used.

## 4. The first run was UNPACED, and the tool would have printed a rate for it

The first run of the tool reported **1,204.46 presents/second**. The cause is a deliberate framework
policy, not a mistake: `launch_environment.agent_environment()` sets `PSXPORT_NOPACE=1` for **every
agent run**, and its own docstring says so — *"Return the explicit headless, silent, unpaced
automation environment."*

At 1,204 presents/second the display field rate is not in force at all — 1,204 is 20x a 59.94 Hz
field — and yet the first version of this tool divided by 59.94 and printed

> `[verdict] 1.022 fields per presented frame at 1204.46 presents/second; with a 59.94 Hz field
> that is 58.68 game frames/second, nearest standard rate 60 fps.`

**A frame rate for a program no player runs, and it agreed with the right answer to within 2%.** That
is the most dangerous shape a wrong number can take here. The verdict now refuses an unpaced run by
name, and the selftest pins the refusal on the measured figure while requiring a merely busy paced
run (120 presents/s) to pass:

```
  ok    verdict: the MEASURED unpaced run (1204 presents/s) is REFUSED as a frame rate
  ok    verdict: the refusal NAMES the unpaced cause rather than saying 'unknown'
  ok    verdict: a merely busy 120 presents/s is not misread as unpaced
```

`--paced` clears `PSXPORT_NOPACE`, and the run header prints which leg it took before any number.

## 5. WHY 1.0000 IS THIS PORT'S CADENCE AND NOT RETAIL'S — the measured reason

This is the finding, and it is not a caveat bolted onto a number. The guest's own per-frame field
count is exactly the quantity 0030 could not reduce to a constant, and **it is never consulted**,
because two of the three things that would consult it are native overrides:

| retail mechanism | status in this port | evidence |
|---|---|---|
| outer selector `0x8002C354` | replaced by the title's finite prefix + mode driver | `spider1_frame_driver.cpp:422`, and the boot log's `0x8002C354 and all mode loops` |
| guest field wait `0x8005E748` | **replaced** by a native override | `spider1_guest_layout.h:448` `guestFieldWait = 0x8005E748`; `spider1_frame_driver.cpp:194` `installNativeOverride(..., guestFieldWait, "Spider field wait", waitGuestFields)` |
| guest frame loop `0x8002C174` | **referenced nowhere in the title tree** | `grep -rn "0x8002C174" titles/ game/` returns nothing |

So the port, not the guest, decides how many display fields a frame consumes.
`Spider1FrameDriver::deliverField` increments `fieldsSinceCommit_` and dispatches the guest's own
field callback; `commitSubmittedFrame` / `commitMovieField` pass that count to
`presentation.commit(&core, fieldsSinceCommit_, ...)`, which is the `pace(guestFields, parts)` call.
And that count is **1, every time** — the product's own pacer channel, 57 consecutive lines from a
paced run:

```
57 × interval=16.6834ms sleep=15.7ms quota=1 parts=1 rate=59940mHz
```

`quota=1` is the host's declaration of "one display field per presented frame", 57 of 57. The
measured 1.0000 fields/present and the 59.71 presents/second are that declaration and the NTSC field
rate doing arithmetic. **They are the port's cadence, measured.**

This is not a criticism of the owner: replacing a non-returning retail loop with a host-driven one
is the architecture this workspace mandates, and `deliverField` correctly dispatches the guest's own
`FUN_8005E510` so the guest's counter still means what retail means by it. The consequence is only
that **the guest's cadence is not observable on a build where the guest's cadence has been replaced**,
and a reader who quotes 59.94 fps as "Spider-Man 1 runs at 60 fps" would be quoting this port's frame
pacer.

## 6. What is therefore established, and what is not

**Established, with denominators:**

* `[gp+0x0C74] = 0x800B5468`, recovered from the crt0's bytes, with exactly one `+1` writer over
  186,880 words and 6 readers.
* In the state this port currently reaches, the product presents at **59.71 presents/second** against
  a 59.94 Hz display field rate, with **exactly 1.0000 display fields per presented frame**
  (133/133 and 19/19 in two independent runs), `interp=0` throughout.
* That ratio is the **port's** declared cadence (`quota=1`, 57/57), because `0x8005E748` and
  `0x8002C354` are native overrides and `0x8002C174` never runs.

**Not established:**

* **Retail Spider-Man 1's frame rate.** 0030's question stands, with a sharper reason: it is not
  answerable from this build, because the code that would answer it is not executing.
* **Whether the guest's frame body waits 1 or 2 fields per frame** — the specific thing 0030 could
  not reduce to a constant. It cannot be answered until `0x8002C174` executes.
* **Whether an interpolation path is in scope.** Explicitly still undecided. Every other measured
  title in this workspace is 30 fps, which makes interpolation *probably* in scope, and "probably"
  is not a measurement.

## 7. What would settle it, precisely

The guest's own field-count decision becomes observable the moment the guest's frame loop runs
through Lightrec with `0x8005E748` served as a **poll on the guest's counter** rather than as a host
field injection. That is one change with a named condition:

1. `0x8002C174`'s body must execute as translated guest code (it is not currently reached, and S018
   is still `missing`).
2. `guestFieldWait`'s override must either be scoped away on that path, or `waitGuestFields` must
   count the fields the guest WAITED FOR rather than the ones the host decided to inject.

Then the same instrument, unchanged, reports the answer: `FUN_8005E748`'s argument is the guest's
own per-frame field count, and the two-number ratio resolves 0030.

**This is blocked on the black-picture frontier, not on the instrument.** The instrument is built,
registered and demonstrated on both answers.

## Falsifiers

* **If `quota` is ever not 1** on a paced run, the fields-per-present figure stops being the host's
  declaration and becomes a real measurement, and the conclusion above is wrong. The pacer-channel
  histogram is the falsifier and it is one command.
* **If `0x8002C174` is in fact reached** on some path — say by a runtime-loaded module reusing the
  address, which the address-only keying this port uses cannot rule out — then the guest's cadence
  *is* being consulted and 1.0000 is meaningful. `docs/issues/0030`'s own overlay caveat applies.
* **If a future build serves `0x8005E748` as a real poll** (item 2 above), this issue's conclusion
  is void and the measurement becomes the answer. The instrument does not need changing; only its
  interpretation does.
* **If the run is ever unpaced** — and by default it is, because `agent_environment` sets
  `PSXPORT_NOPACE=1` — the verdict refuses and no rate is reported. That refusal is pinned by
  `--selftest` on the measured 1,204.46 presents/second, so it cannot be removed silently.

## Instruments

* `tools/probe_spider1_cadence.py` — the two-number measurement. `--selftest` **22/22**, registered
  as `spider1_cadence_measure_selftest`. Its `--census-only` half (needs only the provisioned image,
  no disc and no product slot) is registered as `spider1_cadence_measure_census`.
* `tools/re_cd_stream.py` — the CD-stream recovery's own gate, `--selftest` 6/6, registered as
  `spider1_cd_stream_selftest` in `4b96e51`.
* Both REFUSE (exit 2) on an absent provisioned image rather than reporting a number, so a hosted CI
  run without game data is a refusal and not a pass.
