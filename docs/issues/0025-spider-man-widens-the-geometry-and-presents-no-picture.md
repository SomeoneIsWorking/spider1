---
id: 25
title: Spider-Man 1's widening owner widens the geometry and the product presents 0.00% non-black, so there is no picture to prove it with
status: resolved
symptom: The title has a hand-written widescreen owner, a booting product, and no captured 4:3-versus-16:9 pair. Its two legs were run to presented frame 14,000 and both composed frames came back 0.00% non-black.
state_items: S008,S018,S019
tags: widescreen,render,evidence,black-frame,cd,dma,proving
created: 2026-09-28
updated: 2026-10-09
---

> **2026-10-09, resolved.** The product presents a picture and the widening is proven on 2D and 3D
> frames. The title runs the Gte path (`RenderCapabilities::widescreenOnly`), where the guest draws into
> a widened frame and the host centres 4:3-authored guest primitives by the margin.
>
> **Part 1, stale margins.** Observed: the 16:9 menu (512 wide) sat between a left margin showing the
> previous demo frame and a right margin of VRAM leftovers. Expected: the 4:3 picture centred with black
> columns. Cause: `GpuVkState::draw_wide_margin` (`gpu_vk_wide_margin.cpp`) laid its black base only to
> the right of the native picture, so the left columns the 2D centring leaves undrawn kept the
> persistent composite's old pixels. Fix: `plan_centred_wide_margins` (`wide_margin_plan.h`) gives one
> band per side for a frame the host centres, chosen by `wide_2d_centres_guest_frame`. Measured at
> 16:9 on the menu: the inner 512 columns are pixel-identical to the 4:3 shot (mean abs diff 0.0 at
> offset 86) and both margins are 0.0% non-black.
>
> **Part 2, 3D.** Observed: the 16:9 demo frame was the 4:3 picture shifted by 172 px (twice the 86 px
> margin) with nothing to the right, and the cull window was not widened. Cause 1,
> `Spider1Widescreen::publishProjection`: it shifted the window to 86..598 instead of widening it, so
> culling stayed at the 4:3 extent. The guest compares projected x against both bounds as unsigned
> halfwords (`FUN_8007C2AC` outcodes), so the left bound cannot go below 0 and the widening is `near` 0,
> `far` 512 + 2M with the lens divisor scaled by the same ratio (2365 -> 3159) so H stays 276; the
> frustum planes `FUN_80075D0C` builds from the span widen with it. Cause 2,
> `wide_2d_layout` (`present/wide_2d_layout.cpp`): every guest primitive is classified 2D on the Gte
> path, so the host added the margin to coordinates the widened projection had already placed. Fix:
> `GuestWidescreenProjection::guestCoordinatesWidened` (the title's per-frame claim) makes
> `wide_2d_guest_space` return `RQ_2D_WIDE_FINAL`. The menu overlay publishes the same record from its
> own walk (`ra 0x80155A88`) with a 3D model inside a 2D layout, so only publications from the resident
> render walk (`ra 0x8002BE14`) widen; the claim ends at `commitSubmittedFrame` (a frame number is no key: field presents inside one guest frame advance it).
> `Spider1BootstrapTurn::step` now calls `synchronizePresentation`, which had no caller.
>
> **Pair.** Frame 8052 (16:9) / 8047 (4:3), the same guest tick of `dem3` (the run drifts 5 presented
> frames between aspects because a widened cull changes instruction counts and with them the CD
> completion time), `scratch/widescreen/tk4_t60.ppm` 512x240 and `tk16_t60.ppm` 684x240: the 16:9 frame
> is the 4:3 picture at x+86 (best offset 86) with 86 extra columns of scene on each side (ground and
> skyline on the left, the building and red tower on the right); no pop-in at either edge. The HUD
> ("DEMO") is not stretched but stays at its 4:3 x, because its primitives cannot be told from world
> primitives. Residual: in some frames (`y16x9_8000.ppm`) polygons that retail clips off the right edge
> draw as black quads in the extension (traced in issue 0032: the widened draw area overwrites texture pages).
>
> **The attract demo.** The guest's own disc reads (Setloc LBAs, matched to the `CD.WAD` file table the
> guest keeps at `0x800BA738`; byte offset = 3-byte field * 256) are `dem1.vab` 8714, `dem1.sfx` 8713,
> `dem1_t.trg` 8914, `dem1_l.psx` 8907, `dem1_o.psx` 8913, `dem1_g.psx` 8903 for the first demo
> (frames ~4000-6000), and `dem3.sfx` 9120, `dem3_l.psx` 9346 for the second (frames ~7000-8500).
>
## Answer

**No picture pair exists, and the reason is not the widening owner.** The owner widens — the product
announced `render_width=428` against `native_width=320` on the 16:9 leg — and the product then
presents **0.00% non-black** at both aspects. There is nothing on screen to widen.

## Both legs, from the product, with the widths it printed itself

Method: `tools/probe_spider1_widescreen_pair.py`, two processes, one tracked settings file per leg
(`config/aspect_4x3.ini` / `config/aspect_16x9.ini`, byte-identical except the `aspect` line, both
`fps60=0`), the real `Spider-Man (USA).chd`, guest-resolution `shot` readback, and the product's own
`PSXPORT_PRESENT_SHOT_AT` coverage line. The tool's selftest is 10/10.

| leg | announced `[wide]` lines | steady state (last line) | `render_width > native_width` | guest readback | product's own present shot |
|---|---|---|---|---|---|
| 4:3 | 2 | log:39 | `render_width=320` `native_width=320` → **False** | 320x240 | `non-black 0/230400 (0.00%)` |
| 16:9 | 4 | log:82 | `render_width=428` `native_width=320` → **True** | 428x240 | `non-black 0/306720 (0.00%)` |

**The instrument reported both answers.** The 4:3 leg reads NOT widened from the same code that reads
widened on the 16:9 leg, so the widening verdict is not a check that can only say yes. The coverage
numbers are the PRODUCT's own: `GpuVkState::present_shot` writes the file and then logs the size and
the non-black fraction it saved, so `0/230400` and `0/306720` are counts over a file that exists, not
a missing capture.

## Why the `[wide]` line has to be read twice, on this title specifically

`picture_announce` prints on CHANGE, and Spider-Man's own display publication happens after the first
announce. Every leg's log therefore opens with a **pre-publication** value at a display width the
guest has not settled on and closes with the steady state:

    log:24  [wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=512
    log:40  [wide] native picture: aspect=1 wide_engine=0 native_width=320 render_width=428
    log:66  [wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=512
    log:82  [wide] native picture: aspect=1 wide_engine=0 native_width=320 render_width=428

A reader who quotes the FIRST line of that list gets `512 == 512` and the correct conclusion for the
wrong reason, which is the same shape of error as quoting a goal string as a measurement. The tool
records every occurrence and reports all of them, with the count.

## The margins are black, which is a failed widening, and it is reported as one

The shared reporter, on the two captures:

    scratch/wproof/shots/4x3-f14000.ppm 320x240  vs  scratch/wproof/shots/16x9-f14000.ppm 428x240
      predicted offset for a pure widening : +54
      best translation                     :     0.00 at dx=-8
      worst of 125 offsets tried          :     0.00 at dx=+116
      stretch hypothesis                   :     0.00
      left  margin  54px wide          :   0.0% non-black, 1 colours, 53/53 repeated columns   <- NOT SCENE
      right margin  54px wide          :   0.0% non-black, 1 colours, 53/53 repeated columns   <- NOT SCENE
    REFUSED: ... NOTHING WAS COMPARED at the joins.

Both margins are black, one colour, and every column repeated — the signature of no picture at all,
not of a pillarbox. The tool REFUSES at the joins for the same reason it refuses a flat capture: with
every column identical there is no ordinary column-to-column variation for a break to stand out from.
`present_geometry.py --guest-frame` refuses the same capture by name: *"the guest frame is ENTIRELY
BLACK, so it has no drawn extent to correct with."*

**Both hypotheses score 0.00 across all 125 offsets.** That is not a pass and not a stretch; it is two
uniformly black pictures, and the separation this tool exists to measure does not exist between them.

## The simulation was undisturbed by the aspect change

| counter | 4:3 leg | 16:9 leg |
|---|---|---|
| `translated_blocks` | 1,494 | 1,494 |
| `cache_misses` | 1,497 | 1,497 |
| `executed_blocks` | 35,362,993 | 34,557,997 |
| `executed_instructions` | 210,643,926 | 205,843,693 |
| `host_dispatches` | 45,904 | 44,908 |
| `invalidations` | 3,750,285 | 3,676,585 |
| `faults` | 0 | 0 |
| all six `refused_*` reasons | 0 | 0 |
| presented frames reached | 14,419 | 14,062 |
| instructions per presented frame | 14,610 | 14,639 |

**These are NOT byte-identical, and this issue does not claim they are.** Both legs stop at the first
poll that saw the frame target, so they ran to slightly different points (14,419 against 14,062
presented frames). Normalised, they are the same trajectory to within 0.2% (14,610 against 14,639
instructions per presented frame), and the counters that do not depend on run length are exactly
equal: 1,494 translated blocks and 1,497 cache misses in both. Closing this properly needs both legs
driven to a fixed guest instruction count, which is a change to how the legs stop, not to the port.

`calls=0` in both. That is the endpoint's own counter reading zero, over 210M executed instructions
with 45,904 host dispatches, so it is a real zero and not an absent field.

## Why there is no picture: the CD channel is armed and never started

This is issue 0024's boundary, still open, and it is upstream of every pixel. The run reaches the
authenticating boot, `pre-main ResetGraph` at `0x8008479C`, the field callback at `0x8005E510`,
`CdInit` moving `I_MASK` `0x009 -> 0x00D`, the STR movie-field resume at `0x8002AC8C`, and then:

    [irq] pending I_STAT&I_MASK=0x004; no SysEnq element claimed it (1 in chain), custom exception exit installed

and then presents 14,000 black frames while executing 205M guest instructions. S018 (*"Spider-Man
reaches `dem1` dynamically"*) is `missing`, and this is the same frontier measured from the
presentation side. The one archived run that reached `dem1` at host frame 4,941 did so through the
RETIRED generated-code product, so it is not evidence about this one.

## What is NOT the cause, so the next agent does not go looking for it

- **Not the widening owner.** It widens: `render_width=428` against `native_width=320`, measured from
  the product. S008's other half is untested, so this is not a claim that the owner is *correct* —
  only that it fires and the frame is black anyway.
- **Not the settings.** Both legs are pinned by name through `PSXPORT_SETTINGS`, they differ in exactly
  one key, and the 4:3 leg reads `render_width=320` from the same code that reads 428 on the 16:9 leg.
- **Not `PSXPORT_VK_HEADLESS`, `PSXPORT_PRESENT_SINK` or `PSXPORT_NATIVE_FRAMES` doing nothing.** All
  three were set and the product acted on all three: the headless renderer came up
  (`headless renderer up (VRAM 1024x512 RG8 = PSX 1555)`), the sink took the requested size (320x720
  and 426x720), and the run reached 14,419 presented frames. A knob the port ignores is worth
  checking, and these are not them.

## Next step

Resolve issue 0024's sector handoff at `0x8008DCC8(0x190)` so the guest accepts a sector into the
libstr ring, then re-run this exact tool unchanged. It already refuses correctly on a black pair and
already reports both aspect answers, so the day a frame appears this measurement needs no new
instrument — only a product that draws.
