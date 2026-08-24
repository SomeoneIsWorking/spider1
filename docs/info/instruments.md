# Instruments ledger — can the tool be trusted to show the OTHER answer?

> **RULE, learned the expensive way (2026-07-28): a probe must prove it CAN fire before its silence
> means anything.** `cfg_logf(chan, ...)` is channel-gated. A check written as
> `if (bad) cfg_logf("mychan", ...)` prints nothing when `mychan` is not in `PSXPORT_DEBUG` — and
> "condition never true" and "channel not enabled" are indistinguishable on screen. This produced a
> false negative that stood as a recorded conclusion for a full session ("VSync does not corrupt the
> slot"), and it is the THIRD distinct false-zero in this file (INST-04, INST-06, this).
> Every probe here now emits an unconditional arm/coverage line, so a silent run is evidence.

> **SECOND RULE, same day: `cmake --build … | grep error` is not a build check.** Run from the wrong
> directory, cmake printed `No rule to make target 'spiderman_port'` and exited — and the grep, which
> only looked for `error:`, swallowed it and printed a cheerful "built". The next 40-second run
> measured a STALE binary and reported zero hits for a probe that was never compiled in. Check the
> build's exit status, or look at the last lines (`Linking … / Built target …`), and confirm the
> artifact changed — `strings` for a format string the new code introduces is a two-second check that
> would have caught this immediately.

A broken instrument fails **silently**: "no signal" and "instrument returning nothing" are identical
on screen. Uniform output — all-black frames, all-zero dumps, "no diff", an empty log — is the tell.

Validate an instrument by feeding it a case that **must** differ and watching it say so. When one is
caught lying, mark it distrusted and re-check every result that used it.

> **THIRD RULE, 2026-08-05, and it is the most expensive one so far: an instrument that measures a
> DIFFERENT STAGE of the pipeline than the one you are asking about is not a weak instrument, it is
> a lying one — and it lies with true numbers.** `PSXPORT_SHOT_AT` reported 99.95% non-black on the
> intro logo while the USER, watching the window, saw a black screen. Every number was correct: it
> reads back guest VRAM, which did contain the logo, in the headless leg it was run in. Nothing in
> its output said "this never touched the swapchain". Two instruments (INST-18, INST-19) certified
> that false negative and one of them, the watchdog, is structurally incapable of ever contradicting
> it. **Before quoting an instrument, name the stage it samples and check that it is the stage in
> question.** See issue 0005.

---

## INST-18 — `PSXPORT_SHOT_AT` / `GpuVkState::shot()` PPM histogram — **DISTRUSTED 2026-08-05 for "what the player sees"; trusted for guest VRAM content**

*What it ACTUALLY shows:* `GpuVkState::shot()` (`external/psxport/runtime/recomp/gpu_vk.cpp:1202`)
calls `dump_to()` (`:1171`), which reads back the **guest VRAM texture `s_vram_tex`** and decodes
the display region itself. It never samples the swapchain. It is a measurement of VRAM CONTENT at a
present index, and of nothing downstream of that.

*What it was read as showing, for a whole session:* what is on screen.

*How it lied, with true numbers (MEASURED 2026-08-05, spider1 `3381fcc` / psxport `3f6a1e14`):* it
reported f120 = 99.95% non-black / 11395 colours for the Activision logo, and RE-07 was closed on
that. The USER, watching a window on the same build, saw a black screen. Windowed, the same
instrument reads 0.00% / 1 colour at every present index to f2400 — also true, and also not a
statement about the window. Root cause was upstream of both readings (C021, guest starvation) and
the instrument is blind to the whole segment where it lived.

*Validated where it IS good:* it does show the other answer within its own stage — 0.00% / 1 colour
on every intro shot before the RE-07 fixes, 99.95% / 11395 after (C020).

*Trust it for:* guest VRAM content at a present index, **with the headless/windowed leg stated**.
*Do NOT trust it for:* whether anything reached the display; whether present, composite, letterbox
or fade work; whether the window is alive; or distinguishing "the guest drew nothing" from "the
window shows nothing" — it gives the identical answer to both.

*The missing instrument, named so it stops being a surprise:* **nothing in this port samples the
SWAPCHAIN.** Until something does, every "the picture is correct" result in this repo is a claim
about VRAM. Building one is the cheapest way to stop repeating this failure.
**PARTLY BUILT 2026-08-05 — see INST-20.** `PSXPORT_PRESENT_SHOT_AT` now samples the PRESENT stage
(the composite the swapchain blit consumes), which covers letterbox / fade / source-selection /
24bpp. It still does not sample the swapchain image itself, so the last hop remains uninstrumented;
INST-20 states exactly where its blindness starts.

*Structured entry:* `I013` (distrusted). *See:* issue 0005, C019 (falsified), C020 (scoped re-issue).

---

## INST-29 — RE-08 depth coverage: `render_depth_coverage_report()` via the seam's periodic call, plus `tools/re08_store_sites.py` — **trusted 2026-08-24; both shown to produce both answers**

The whole-run instrument that replaced the aliased one-frame line (INST-26) for every depth question
this repo asks. TWO halves, each with a demonstrated negative:

1. **RUNTIME** — external/psxport's `render_depth_coverage_report(Core*, why)` (gpu_native.cpp),
   printing LIFETIME totals (`nd3dTotal`/`nd2dTotal`, `ProjPrim::totals()`), called from
   game/render/render_seam.cpp every `kDepthReportEvery = 2048` submitFrame calls. Trust basis:
   (a) the counters are never reset between reports, so no sampling parity can alias them onto a
   non-drawing field — the exact defect that killed INST-26; (b) its zero case prints "NO PRIMITIVES
   WERE CLASSIFIED AT ALL … not 0% 3D" instead of a percentage, so silence cannot wear a number;
   (c) it was seen to produce the OTHER answer: run 1 vs run 2 differ at matching submit counts by
   ±1 primitive while percentages repeat (63.57%/61.47%/60.91% across report points), and misses
   carry a stale-vs-absent split that moves independently of the hit rate. It is emitted mid-run
   because the watchdog `_exit(130)`s and an exit-time dump would never print.
2. **STATIC** — tools/re08_store_sites.py: walks the same images the substrate was built from
   (MAIN + all 30 overlay modules from `scratch/overlays/spiderman1`, function boundaries taken from
   the generated dispatch tables,
   analysis imported from emit.py rather than re-derived) and counts every vertex-store form with
   denominators on every line. REFUSES (exit 2) on missing/mismatched corpus instead of printing an
   empty table — refusal observed live during development (28 images vs 30 dispatch tables).
   Hermetic gate tests/test_re08_store_sites.py feeds a synthetic module carrying all four store
   forms and asserts BOTH classes: the SXY swc2 is a tap AND the SZ swc2 is counted as UNTAPPED;
   shown RED by mutation (adding 18 to XY_REGS flips swc2_xy=1→2 / other=1→0 and fails).

Blind spots, stated: the runtime report says nothing about pixels or about whether 61% prim coverage
is good for this scene mix (dem* attract scenes are 2D-heavy — per-scene breakdown not yet built);
the static scanner trusts generated/ dispatch tables as the function-set ground truth and decodes
function bodies linearly, so data-in-body words count as undecoded noise (9478 of 329101, mostly
SHELL). *See:* C003 (the claim this instrument re-established), RE-08.

---

## INST-28 — `tools/gate.py` (THE RUN GATE) — **trusted for BOOT REACH, after being seen to fail 17 ways; blind to pixels and to the pc_render leg. Added 2026-08-12, extended 2026-08-22**

*What it shows:* whether the already-built `scratch/bin/spiderman_port` still boots and keeps
advancing. `gate.py boot` launches it headless, capped (never `./run.sh`), and
asserts on the port's own log lines: boot exe loaded, guest main dispatched, render seam installed AND
fired, first-to-last ADVANCE of the periodic `[rseam] submitFrame calls=… frame=…` counters, frame and
submit floors at 35% of the recorded baseline RATE, ≥2 scene changes into printable scene names, and no
failure pattern. `check-log <path>` runs the same analyser over any captured log.

*Why it is not a REPL gate like the sibling port's:* this port never enters the framework frame loop
that services the REPL (`game/core/game_hooks.cpp` rec_dispatches the guest main, which never returns),
and the port's own cfg audit reports `PSXPORT_REPL` as a knob that "did NOTHING in this run". So the
gate cannot step the game; it can only read what a capped launch printed. See issue 0014.

*Validated in BOTH directions, which is the whole point:* `--selftest` runs 18 cases — 16 through the
SAME `analyse()` the real gate uses plus 2 through the REAL launcher — and 1 known-good capture PASSES
while 17 broken variants are caught (12 FAIL, 4 REFUSE, 1 GPU-device-loss STOP), each keyed on exactly
one changed line. The added case feeds the exact `Fps60::rq_capture OVERFLOW` that issue 0017's
pre-fix live runs emitted; the analyser now fails it by name instead of returning an ambiguous
short-run refusal. It also fires on REAL logs, not just synthetic ones: `check-log scratch/re20/logs/pcleg_final.log` → exit 1 on the pc_render leg's
genuine `[FATAL:error] unimplemented native rendering` abort, and `check-log
scratch/logs/gate_newpin.log` → exit 3 on a real `context is lost` line from an earlier run.

*Calibration trap, recorded because copying the sibling would have hit it:* a plain `/\babort\b/`
failure pattern MATCHES this port's healthy startup banner ("… then aborts at the next scene naming
it. That abort is the correct result"), so it would have failed every green run. Patterns are anchored
to how a failure PRINTS instead — lucent renders an error as `[<channel>:error]`, hence
`[FATAL:error]` — and `[watchdog] STUCK` is case-sensitive because the ordinary timeout-kill line
contains the word "stuck" in prose.

*Known limits, stated so a pass is not overread:*
- **Blind to pixels.** It counts frames and submits; it never looks at an image. Use INST-20/INST-25
  for that. It is blind to the pc_render leg entirely — the default leg is psx_render.
- **The rate floors are LOAD-SENSITIVE.** Several agents share this machine, so wall-clock throughput
  is not a pure function of the code. A failure on the frame/submit floor ALONE, with every other
  assertion green, is the one verdict to re-run before believing.
- **A short run REFUSES rather than passing.** The port prints a progress line every 512 submitFrame
  calls, so under two lines there is nothing to compare and exit 2 says so. But an abort outranks that
  refusal — an early version got this ordering wrong and reported "nothing proven" over a FATAL it had
  already read (selftest case 14 pins it).
- **Says nothing past the ~2 minutes it runs**, and nothing about audio or input.

*AUDITED 2026-08-12 (second pass, by re-running it rather than re-reading it) — two real defects found
in the LAUNCHER, both in the HANG branch, which had ZERO selftest coverage. Both are fixed and selftest
case 17 now drives that branch through the real `cmd_boot`:*
- **The hang refusal saved an EMPTY log.** The handler wrote only `e.stdout`, and the port writes 100%
  of its output to STDERR — measured: 92 stderr lines, 0 stdout lines from a 12s capped run. So the
  one failure that most needs a log got `Log: <path>` pointing at 0 bytes. Now writes stdout+stderr and
  reports the captured line count in the refusal itself.
- **It orphaned the game.** `subprocess.run(timeout=)` kills only the DIRECT child, so killing a
  wrapper left `spiderman_port` alive and reparented — measured with a stand-in: 2 survivors
  per hang. A GPU-holding orphan is what the NEXT gate run then contends with, while this run reported a
  tidy refusal. Now `start_new_session=True` plus `killpg`, and the refusal states how many processes
  were signalled and how many were still alive after.
- *The audit's own instrument lied twice before it worked*, which is why the fix is trusted: the
  survivor check first scanned `ps` for the script's NAME (the grandchild's argv is `sleep 600`), then
  for a marker in `sh -c 'sleep 600 # MARK'` — which dash EXEC-OPTIMISES away. Both printed
  "surviving=0" against a launcher that measurably leaked. It now detects survival by an advancing
  HEARTBEAT mtime and reports `grandchild spawned=…` as its denominator, so "nothing survived" cannot
  be confused with "nothing ever ran".

*Structured entry:* `I030` (trusted). *See:* issue 0014, `docs/codemap.md` (THE RUN GATE row).

---

## INST-20 — `PSXPORT_PRESENT_SHOT_AT` / `GpuVkState::present_shot()` — **trusted for the PRESENTED PICTURE, in either leg, after passing a discriminator in both directions**

*What it shows:* a readback of `s_present_img`, the sink-resolution composite built by
`GpuVkState::build_present_image` — the picture AFTER letterbox, fade, native-vs-ires source
selection and 24bpp decode. It is the first capture in this project that samples the present stage
rather than guest VRAM, and it exists in BOTH legs because the composite itself now does (the
`if (s_headless) return` that sat above it is gone; see issue 0006 and the psxport
`present-image-sink` patch).

*READ THIS BEFORE THE TABLE BELOW — the USER caught it, 2026-08-05.* Every capture in the original
validation (presents 120/200/300/600/1200) was taken while the display register was in **24-bit
320x240 — the MDEC/FMV mode**. All five were intro-movie frames. Not one was a frame this port
RENDERED; the game's own mode is 15-bit 512x240 and I sampled none of it, then wrote up "the port
renders in both legs" and put the Neversoft logo in the codemap as the proof. The instrument was
fine; the INFERENCE from it was the same false generalisation this ledger exists to catch — I picked
the present indices, and every one of them landed in the movie.

*Re-measured on a NATIVELY RENDERED frame (present 3900, main menu, 15-bit 512x240):* VRAM 99.4%
non-black / 1048 colours; present shot at a 960x720 sink 62.13% / 1048. The gap is the widescreen
letterbox and it is arithmetic, not hand-waving: 512:240 is width-limited in a 4:3 sink, so the
picture fills 960x450 of 960x720 = 62.5% of rows, and 99.4 x 0.625 = 62.1. This is the STRONGER
validation, because a 512x240 frame exercises the widescreen letterbox path that a 320x240 FMV frame
never touches — the FMV captures could not have caught a bug in it.

*Why it can be believed where INST-18 could not:* it was validated by forcing the two instruments to
DISAGREE, not by watching them agree. MEASURED 2026-08-05, spider1, one build, headless, present
1200 — **an FMV frame, per the correction above**:

| sink | `PSXPORT_SHOT_AT` (VRAM) | `PSXPORT_PRESENT_SHOT_AT` (present) |
|---|---|---|
| 960x720 (4:3) | 320x240, 25.9% non-black, 8825 colours | 960x720, **25.9%**, 8825 colours |
| 800x800 (square, `PSXPORT_PRESENT_SINK=800x800`) | 320x240, **25.9%** — unchanged | 800x800, **19.42%**, 8825 colours |

The square sink letterboxes a 4:3 picture into 800x600 of 800x800 rows, so a present-stage
instrument MUST read 25.9% x 600/800 = 19.43%; it read 19.42%. A VRAM-stage instrument cannot move
at all, and did not. That is the discriminator run against BOTH classes: the tool showed the other
answer when the other answer was true, and refused to when it was not.

*Also validated:* it reports its own negative. With no composite yet it logs
"no present image yet ... NOTHING captured" and writes no file, so a missing capture cannot be read
as a black frame. And every successful capture carries its non-black count AND its denominator on
the same line, unconditionally — an all-black present is a real answer that must stay
distinguishable from a capture that never ran.

*CORRECTION, same day, and the reason this entry is worth re-reading rather than trusting:* when
first written, the paragraph above was FALSE in its most important case. `image_write_rgb24` was
`void` with a bare `if (fopen)`, and nothing in the framework creates `scratch/screenshots/` — so
run from any cwd lacking that directory, the instrument logged
`wrote scratch/screenshots/present_200.ppm ... non-black 11.20%` and produced **no file at all**.
Measured: 2 armed captures, 2 success lines, 0 files on disk. The instrument built to stop this
project certifying measurements it never took was doing exactly that, and the guarantee was written
into its own claim before anyone tried it from a fresh directory. The pre-existing VRAM shots
(`gpu_shot`, `shot_region`, `vram_region`, the software-GPU shot) had the identical defect.
NOW FIXED AND VERIFIED IN BOTH DIRECTIONS: the writer returns bool, creates the parent directory,
and names the failing step itself; every capture path says `NOTHING captured` instead of `wrote`.
Forced-failure control (make `scratch/screenshots` a regular file):

    [image_write:error] cannot create the parent directory of ... — NOTHING written
    [present_shot:error] NOTHING captured for ... — the picture itself was 12.10% non-black

The measurement still rides with the failure, so a failed WRITE does not also lose the READING.
*Lesson worth keeping:* the first repair printed `strerror(errno)` = "Timer expired" for a failed
mkdir, because `std::filesystem` reports via `error_code` and never sets errno. A diagnostic that
states a confidently wrong cause is worse than one that states none.

*A SECOND WAY IT WAS SILENT, found by measuring on Tomba2 rather than by reading:* both
`PSXPORT_SHOT_AT` and `PSXPORT_PRESENT_SHOT_AT` were armed inside `GpuState::gpu_present`, but
`Fps60::present_vk` calls `gpu_present_ex` directly, so a port running fps60 never reaches them.
Tomba2Engine ships `fps60=1`: `PSXPORT_SHOT_AT` over 2802 presents there produced ZERO files and
ZERO log lines, while the same binary with `fps60=0` captured normally. Both instruments were
structurally inert on the reference consumer, and "no capture" was indistinguishable from "not wired
here". The triggers now live at the tail of `gpu_present_ex`, the chokepoint every presenter shares.

*Trust it for:* what the composite hands to the sink — whether the game has a picture, and whether
the letterbox/fade/24bpp path is right. Cross-leg comparison holds **only in a windowed,
non-fullscreen, 1x-display-scale run**: the headless sink defaults to the window's *logical* creation
size (960x720), but the windowed sink is the DRAWABLE, so HiDPI gives the same window a 1920x1440
shot, `PSXPORT_FULLSCREEN` makes it the monitor size, and the window is resizable. Every capture logs
its sink size and leg so the mismatch is visible in the output rather than assumed away — an earlier
version of this entry promised "directly comparable pixel for pixel" without those three conditions.

*Do NOT trust it for — and this is the honest edge of it:* anything in the LAST HOP. It reads the
image the swapchain blit consumes, not the swapchain image. It is therefore still blind to a failed
`SDL_WaitAndAcquireGPUSwapchainTexture`, a blocking present mode starving the guest thread
(INST-18's issue 0005 root cause), a minimised or dead window, the RmlUi overlay (deliberately
composited into the sink pass, not into the picture), and anything the compositor does afterwards.
**If the user says the window is black and this instrument says it is not, believe the user: the
fault is in the hop this cannot see.** That hop is now the only uninstrumented one, which is a much
smaller unknown than "everything after VRAM", but it is not zero.

*AND IT CANNOT BE CLOSED BY A READBACK — settled 2026-08-05, so nobody spends a session trying.*
`SDL_gpu.h:4306`: "The swapchain texture is write-only and cannot be used as a sampler or for another
reading operation." `SDL_gpu_vulkan.c:4699` creates it with `COLOR_ATTACHMENT|TRANSFER_DST` only — no
`TRANSFER_SRC`, no `SAMPLED` — and no hint, property or device-create option adds them.
`SDL_DownloadFromGPUTexture` carries no usage assertion (`SDL_gpu.c:2931`), so calling it on a
swapchain texture returns **no error and a plausibly all-zero buffer**: a black PPM, i.e. exactly the
false-negative shape INST-18 already produced once. **Do not build it, not even to see.**
Corroboration that this is the right architecture rather than a limitation we are stuck behind: SDL's
own GPU renderer draws into an offscreen `COLOR_TARGET|SAMPLER` backbuffer and blits that to the
swapchain, and `GPU_RenderReadPixels` (`SDL_render_gpu.c:1353`) reads **the backbuffer**.
`s_present_img` is SDL's own answer to this problem.
*The instrument that COULD cover this hop* is not a picture at all: a sink ledger counting acquire
failures, null swapchain textures and presents that never reached the sink. That is precisely the
failure class issue 0005 turned out to be. Not built yet.

*Reproduce:*

    PSXPORT_NOWINDOW=1 PSXPORT_NOAUDIO=1 PSXPORT_PRESENT_SHOT_AT=1200 \
    PSXPORT_PRESENT_SINK=800x800 PSXPORT_WATCHDOG=90 timeout 90 ./run.sh
    python3 ../spyro/tools/ppm_look.py scratch/screenshots/present_1200.ppm

---

## INST-25 — `tools/present_geometry.py` — **the SHAPE check, built because every other instrument here is blind to shape. TRUSTED for the DIRECTION (stretched vs fills-the-sink); its STRETCH MAGNITUDE is DISTRUSTED as of 2026-08-06 — it measures a CONTENT BBOX, not the display rect**

*Why it exists, and it is the most important sentence in this entry:* issue 0008 (the picture
presented 1.6x too wide) survived a full session of numeric checking because **every instrument in
this repo is INVARIANT UNDER IT**. A uniform rescale changes no non-black coverage, no
distinct-colour count, no mean brightness, no per-tile richness. `ppm_look.py` reported
"real frame, 62.0% non-black, 528 colours" on a stretched frame and every number was correct. The
bug was found by the USER asking "are these stretched wide?" — no check here could have found it,
and none of them was wrong. **The gap was a missing DIMENSION, not a weak instrument.**

*What it measures:* the content band (non-black bounding box) inside the sink, its aspect against an
expected one, the stretch factor, and the bars — explicitly, because "big black bars" is what this
bug looks like to a human and bars read as deliberate letterboxing. That misreading is exactly how
the defect was dismissed repeatedly in one day.

    python3 tools/present_geometry.py <shot.ppm> [--expect 16:9] [--tol 0.02]

*Validated in FOUR directions, not one:*

| input | result |
|---|---|
| the real defect frame | `STRETCHED 1.600x WIDE`, rc=1 |
| synthetic correct 4:3 (fills the sink) | `OK`, rc=0, `bars none` |
| synthetic 16:9 band, default expectation | `STRETCHED 1.333x` |
| same band, `--expect 16:9` | `OK` |
| entirely black frame | **REFUSES**, rc=2 |

That last row is the load-bearing one: a 0x0 band would compare unequal to any expected aspect and
print a confident "STRETCHED" for a frame containing no picture at all.

*Do NOT trust it for:* whether the picture's CONTENT is undistorted — it measures the frame's
geometry, not the geometry of things drawn in it, and cannot tell a correct 4:3 picture from a
square picture of a square thing. It also measures the NON-BLACK band, not the intended picture
rectangle, so a fade or a genuinely dark scene can measure smaller than the real picture; it prints
a CAUTION when the band covers under 5% of the sink rather than reporting a confident aspect.

*A defect found in the tool in its first minute, recorded because it is this file's whole subject:*
the first argument parser filtered on a leading `--`, leaving the VALUE of `--expect` ("16:9") in the
positional list to be opened as a PPM. It crashed loudly, which is the only reason it was caught
immediately — an arg parser that mistakes a flag's value for an input is otherwise exactly how a
tool measures the wrong file and reports it with confidence. Unknown options now refuse (rc=2)
instead of being silently ignored.

### DISTRUSTED FOR THE STRETCH FACTOR, 2026-08-06 — the caveat was in the tool and NOT in this ledger

The paragraph above ("it measures the NON-BLACK band, not the intended picture rectangle") was
written as a soft limitation. It is not soft: **the number this tool leads with is wrong by an amount
nobody bounded, and it was quoted as a measurement.**

MEASURED: run against spyro's present, **this repo's copy** reports the horizontal stretch as
**1.714x**. The real present stretch on spyro is **1.600x** — 512:240 presented where 4:3 belongs,
which is exact arithmetic and not itself in doubt. The error is 7%, in the direction of "worse than it
is", and the cause is the one already in the tool's own docstring: **black content rows shrink the
measured band**, so a picture that does not reach the top and bottom of its own display rect measures
shorter than it is and therefore wider in aspect. spider1's own defect frame happened to reach its
edges, which is why the same tool hit 1.600x exactly here and looked precise.

THE DISCREPANCY IS EXACT, NOT FUZZY, AND THAT NAMES THE CAUSE: `1.714 / 1.600 == 240 / 224`. Spyro's
guest draws **224 of its 240 display lines**, so 16 rows of the display rect are black BECAUSE THE
GAME DREW THEM BLACK — not because of a letterbox bar. The band-only measurement cannot tell those two
kinds of black apart from pixels, so it charges guest-drawn black to the letterbox and inflates the
aspect by exactly 240/224. (The 224 figure is INHERITED from spyro's own registry, not measured here;
what is checked is that the ratio is exact.)

WHY THIS BELONGS IN THE LEDGER AND NOT ONLY IN THE DOCSTRING: this ledger is what gets consulted
before a number is cited. A caveat that lives only in the source is a caveat that gets read after the
number is already in a report. The tool's `CAUTION` also fires only when the band covers under 5% of
the sink — nowhere near a 7% aspect error — so on the spyro frame it printed a confident, wrong,
unqualified `1.714x`.

**HOW TO USE IT, unchanged in value and narrowed in scope:**

* TRUSTED, and still the only instrument here that can see this class of bug at all: the VERDICT —
  `STRETCHED` vs `OK / bars none (fills the sink)`. That is a direction, it survives a bbox that is a
  few rows short, and it is what a before/after gate actually needs.
* TRUSTED: the before/after TRANSITION on the same content (same present index, same scene, one code
  change between them). The bbox error is common to both legs and cancels.
* **DISTRUSTED: the stretch factor and the aspect ratio as absolute numbers.** Do not put `1.714x`
  into an issue, a claim or a codemap row. Derive the expected stretch from the display mode
  (`disp_w : 240` vs 4:3), which is exact, and use this tool to confirm the direction.

### THE FIX ALREADY EXISTS, IN A DIFFERENT COPY OF THE FILE — `spider1/tools/present_geometry.py` IS THE STALE ONE

Found while registering this, 2026-08-06, and it changes the remedy from "someone should write it"
to "copy it": **`present_geometry.py` is DUPLICATED across the workspace and the copies have
diverged.** `spyro/tools/present_geometry.py` and `Tomba2Engine/tools/present_geometry.py` carry a
repaired version (registered as spyro **I042**, `--selftest` 16/16, mutation-tested against 3
injected defects); **this repo's copy is the ORIGINAL and has not been updated.** The repaired version:

* **REFUSES with rc=3 (AMBIGUOUS)** when black margins make band-vs-picture undecidable, instead of
  printing a confident band aspect. On a spyro-shaped frame the old copy printed `STRETCHED 1.714x`;
  the new one refuses.
* accepts `--active 512x224 --display 512x240` (or `--guest-frame <fb dump>`) so the caller supplies
  the guest's real drawn extent, and then resolves that same frame to `STRETCHED 1.600x`, rc=1.
* was validated in BOTH directions on that frame: the FIXED present with the SAME flags gives
  `OK`, rc=0.

**REMEDY:** `cp spyro/tools/present_geometry.py spider1/tools/` — not done here, because this step was
docs-only and a tool copy is a change the operator should see as its own act. Until it is done, THIS
REPO'S COPY REMAINS AS DISTRUSTED ABOVE. **Before quoting a number from ANY copy, run
`md5sum */tools/present_geometry.py` from `~/repo/psx`** and check which one you have; no hash is
recorded here on purpose, because a hand-copied hash rots at the next edit. Measured 2026-08-06:
spyro's and this repo's copies have DIFFERENT md5s.

Note what the repaired tool does NOT solve, per I042's own entry: `--active` assumes the present is a
UNIFORM SCALE of the guest display rect, so a presenter that crops, pans, or scales the axes
differently makes the correction silently wrong and undetectable. And the tool's proper home is
`external/psxport/tools/`, which needs a coord claim; three diverging copies is the current state.

*See:* issue 0008 (this repo, RESOLVED), spyro issue 0047 (the same defect, still OPEN there, where
the 1.714x reading was taken), spyro I042 (the repaired copy and its selftest).

---

## INST-26 — `PSXPORT_DEBUG=ndepth`, the `3D%=` coverage line (`external/psxport/runtime/recomp/gpu_native.cpp:1646`) — **DISTRUSTED 2026-08-06: it printed `3D%=0.0` for an entire run while measuring nothing at all**

*The failure in one line:* it prints a ONE-FRAME sample formatted as a coverage measurement, and it
prints the SAME `0.0` when it has no data whatsoever.

**MECHANISM, read out of the code (this tree, `runtime/recomp/gpu_native.cpp`):**

    :1646   the report is gated  `if (s_frame > 0 && (s_frame % 60) == 0)`
    :1673   `core->rsub.stats.nd3d = core->rsub.stats.nd2d = 0;`   — runs on EVERY present, unconditionally
    :1660   `core->rsub.projprim.statsReset()`                      — same, so the projprim records/hit/miss
                                                                      line beside it has the identical shape

The counters therefore cover **one frame**, not the 60 between reports, and the line never says so:
`[ndepth f600] real-depth(3D) prims=… OT-band(2D) prims=… 3D%=…` carries no denominator and no
statement of its window. 59 of every 60 frames are counted and discarded unread.

**AND THE ZERO CASE IS INDISTINGUISHABLE FROM THE NO-DATA CASE.** The percentage is
`(nd3d+nd2d) ? 100.0*nd3d/(nd3d+nd2d) : 0.0`, so a frame in which NOTHING was counted prints
`3D%=0.0` — byte-for-byte identical to a frame that genuinely drew 0% 3D. Silence was given a number,
which is this file's oldest and most expensive failure mode wearing new clothes.

**MEASURED, and this is the case that caught it:** Spider-Man draws on ALTERNATE FIELDS. 60 is even.
So every `s_frame % 60 == 0` sample landed on a NON-DRAWING field, and the channel printed `3D%=0.0`
for an entire run. It read as "the native depth path covers nothing"; it was measuring nothing. The
aliasing is a property of the SAMPLING PARITY against the drawing cadence, so it can hit any port
whose cadence divides into 60.

**WHAT ELSE RESTED ON THIS CHANNEL — re-check before citing.** In this repo, claim C003 ("the GTE
vertex depth tap is wired in this port") cites `PSXPORT_DEBUG=ndepth` reporting
`projprim(vtx) records=0 lookups hit=0 miss=0` on every sample across a 100 s run, 636 lines. Those
lines come off the same 60-parity single-frame sample and the same reset-every-frame counters, so
"636 lines" resolve to roughly ten single-frame snapshots (f60..f600+), all of them at the same
parity. That does not make C003's conclusion wrong — it means its evidence has one systematic error
running through every sample, exactly the shape INST-24 in this file is about, and its own stated
falsifier ("a run that reaches 3D gameplay still reports records=0") cannot be evaluated with an
instrument that reports records=0 for a field on which nothing was drawn.

**THE ALIASING HALF IS SPIDER-MAN-SPECIFIC; THE STRUCTURAL HALF IS NOT.** Registered in the sibling
port as spyro I041, where the picture is different and worth stating so nobody generalises: spyro's
C145 evidence records the port ALTERNATING `hit=1547/miss=0` and `hit=0/miss=1540` across CONSECUTIVE
ndepth samples, which is only possible if its samples span both phases — so spyro is demonstrably not
phase-locked and has NOT shown this failure. The missing denominator and the zero/no-data collision
apply to every port, because they are in the code.

**TO TRUST IT AGAIN — both, neither optional:**

1. **CARRY THE DENOMINATOR.** Accumulate over the whole interval (or say "1 frame" in the line), and
   print the frame count and the raw totals, so `0 of 0` can never be rendered as a percentage. A
   no-data sample must print something a reader cannot mistake for a measurement —
   `3D%=n/a (0 prims counted over 1 frame)`.
2. **SAMPLE BY DRAW EVENT, NOT BY FRAME PARITY.** Report on the Nth frame THAT DREW SOMETHING, not on
   every 60th frame index, so an alternate-field or otherwise periodic cadence cannot alias the
   sample onto a silent field.

Its `OVERFLOWED — records were DROPPED` flag is still worth having: that one reports an EVENT, not a
rate, so neither defect applies to it.

*See:* spyro `docs/info/instruments/041-*` (I041, the same instrument registered in the port whose
claims depend on it), C003.

---

## INST-27 — `scratch/screenshots/` AS A CORPUS — **a SHARED ACCUMULATOR; globbing it manufactured an entire false root cause. Method finding, 2026-08-06**

This is not a tool, it is the DIRECTORY every tool writes into, and it is registered here because it
produced a wrong answer that no instrument in this file could have caught.

*The property:* `scratch/screenshots/` is written by **every run, by every tool, from every session,
and nothing ever clears it.** Files from unrelated runs days apart sit side by side with no run
identity on them. So `scratch/screenshots/*.ppm`, `sorted(glob(...))[-1]`, and "the newest one" all
silently mix runs.

**MEASURED CONSEQUENCE, 2026-08-05:** a glob over this directory swept a STALE leftover file into an
analysis as "the correct reference frame". Comparing the run's real output against it produced **an
entire false root cause for Spider-Man's flicker** — a widescreen explanation that was subsequently
REFUTED. Nothing about the run looked wrong. The analysis was internally consistent and was simply
about a different run's file.

**WHY IT IS ALMOST UNCATCHABLE BY THE CHECKS WE ACTUALLY RUN:** a stale capture is a REAL, VALID
picture of a REAL run. Every sanity check in this repo passes on it — plausible frame, right
dimensions, non-black, sensible colour count, coherent histogram. `ppm_look.py` will describe it
accurately. There is no signal of wrongness in the artifact at all; the wrongness is entirely in
WHICH FILE IT IS, and nothing in the file records that.

**THE RULE, and it is two halves — the first alone is not enough:**

1. **Write to a PER-RUN DIRECTORY** named for the run (`scratch/screenshots/<run-id>/…`), created
   fresh for that run. Never write into the shared root, and never read a series out of it.
2. **VERIFY EVERY FILE AGAINST ITS OWN `present_shot` / capture LOG LINE BEFORE READING IT.** The
   run's own log names the path and the present index it wrote. A file that no log line in THIS run's
   log claims is not this run's file — whatever directory it is in, and whatever its mtime says.

mtime is not proof and must not be used as one: a run that dies before capturing leaves the PREVIOUS
file newest. That is not hypothetical either — it is the same shape as spyro's C138, where a
capture-then-copy idiom mislabelled three of five captures and the byte-identical results read as a
finding for a day.

*See:* INST-20 (the present-stage capture that writes here), spyro I036 (the same hazard, and the
mtime guard added to `shot.py` there, which fixes only the single-file half of it).

---

## INST-24 — VRAM/RAM DUMPS AS EVIDENCE ABOUT A PERIODIC SIGNAL — **four "independent" dumps agreed, and all four were the same systematic error**

*This is the most expensive instrument lesson in the file, because nothing about it looked like an
instrument failure.* Issue 0007's headline read: "the CLUT strip is 100.0% the single value 0x3333,
confirmed on FOUR INDEPENDENT VRAM DUMPS". Every dump genuinely read 100.0%. The conclusion was
still false.

The strip is **bimodal with a period of ~64 presents** — either 61 distinct values (real palettes)
or exactly 1 (0x3333), never anything between. Measured across every dump on disk:

    11600 / 11604 / 11900 / alt10000 / pal2020            61 distinct,   0.0% 0x3333
    3d / 3d_a / 3d10000 / coordlens / lens / lens9800
      / menu3900 / pal9612 / pay / alt10032                1 distinct, 100.0% 0x3333

`alt10000` and `alt10032` are thirty-two presents apart in one run — the two halves of the cycle.

*The defect in the reasoning, stated so it generalises:* the four dumps were independent in COUNT
and identical in PHASE. They were taken at present indices chosen for other reasons (a scene of
interest, a round number), and those indices all happened to land in the wiped half. **Repetition
only buys independence if the repeats differ in the dimension that matters.** For a periodic signal
that dimension is phase, and none of the four varied it.

*How to not repeat it:* when a dump-based claim is about state that could be REBUILT PER FRAME —
anything in VRAM, any buffer the guest re-uploads — sample at several phases before generalising.
Two dumps a half-period apart would have caught this instantly; so would one `PSXPORT_TEXWATCH` run,
which shows the writes rather than a single instant (and is what finally did).

*Corollary, and the reason this sat undetected through a whole investigation:* a single-instant
instrument cannot distinguish "this value is always here" from "this value is here right now". Its
clean verdict has to carry "cannot rule out a different value before or after the dump" — the same
caveat `docs/info/instruments.md` already records for scanners under INST-13's entry.

*See:* issue 0007. *Cost:* an entire investigation branch chasing a missing palette load that was
never missing.

---

## INST-22 — `PSXPORT_RAMDUMP_FRAME` — **USELESS ON spider1, AND SILENT ABOUT IT**

*What it claims:* dump 2 MB of guest RAM mid-run at frame N.
*What it does here:* nothing at all. The trigger lives in the NATIVE frame loop
(`native_boot.cpp:555`, `for (uint32_t f = 0; ...)`). spider1 does not run that loop — it dispatches
the guest's own `main()` on the recompiled substrate ("[boot] Phase 0: dispatching guest main()
0x8002C354"). MEASURED 2026-08-05: a 400 s run with `PSXPORT_RAMDUMP_FRAME=11900` produced **no file
and no log line of any kind**.
*Why it belongs in this ledger rather than a bug list:* "no dump was produced" and "this knob is not
wired on this port" are indistinguishable on screen, which is the failure shape this file exists to
catch. A knob that cannot fire must SAY it cannot fire.
*Until fixed:* to read guest RAM mid-run on spider1, use `PSXPORT_WWATCH` (see INST-23 for its one
untrustworthy field) or the debug server. *Fix would be:* arm it from the substrate path too, or
refuse at startup with a diagnostic naming the reason.

---

## INST-23 — `PSXPORT_WWATCH` — **trusted for address/value/width/frame; the `pc` it names is FICTION in recompiled code**

*The trap.* Watching the CLUT source buffer, it reported

    [wwatch] f3 store [801461A4]=33333333 by pc=80064FA0 ra=800653D4

and `pc=0x80064FA0` disassembles to `sll $a1, $a1, 2` — **not a store**. The containing function
decompiles (Ghidra) to heap free-list coalescing, which has nothing to do with palettes. `Core::pc`
is not updated per-instruction inside recompiled bodies, so the reported pc is wherever it was last
written, not the storing instruction.

*Trust it for:* that a store of value V hit address A at width W on frame F. All of that is real and
reproduces — it is how the 0x33 fill was found at all, and the watch is genuinely good at that.
*Do NOT trust:* `pc` or `ra`, unless the run is under the interpreter rather than recompiled code.
Acting on that pc is how an RE session acquires a wrong attribution — the exact class
`tools/ghidra_query.py`'s own header documents four instances of.
*If you need the writing site:* re-run the region under the interpreter, or use
`PSXPORT_WWATCH_BT=1` (host backtrace, names the `gen_func_*` chain) rather than the guest pc.

---

## INST-21 — `PSXPORT_GPU_TRACE`'s `batch tri=/tex=/semi=` counters — **DISTRUSTED as "is the native raster drawing?"; use the `drawn` counters added beside them**

*The trap, and it caught me within minutes of relying on it (2026-08-05).* The trace line samples
`s_tri_n / s_tex_n / s_semi_n` — the LIVE accumulator — at the **top of `GpuVkState::present()`**,
which is *after* the previous `frame_end` reset it (`gpu_vk.cpp:1569`) and *before* the current
frame's drawing. On a game that presents at the top of its frame loop, it therefore reads **0 every
single time no matter how much the native raster is doing**. In spider1's 3D scene it read
`batch tri=0 tex=0 semi=0` at every sampled present, which reads exactly like "no primitive reaches
the native rasterizer" — a dramatic conclusion, and false. The reset point, not the renderer, is what
the number described.

*The fix, in the same line so the two can never be confused again:* the trace now also prints
`drawn tri= tex= semi=` from `s_dbg_tri_c/tex_c/semi_c` — what `render_geom` actually rasterised, the
same counters `gpu_vk_stats()` and the debug server report. Same scene, same run:

    batch tri=0 tex=969 semi=204 | drawn tri=0 tex=969 semi=204     (present 11600)

*Trust `drawn` for:* whether the native rasterizer is doing anything, and how much of it is textured
vs flat vs semi-transparent.
*Do NOT trust `batch` for that at all* — it is only meaningful read at a point where the accumulator
is known to be full, and the trace is not such a point.
*Known limitation of both:* GPU_TRACE samples every 200th present. 200 is even, so against a 30 Hz
draw cadence the sample lands on a fixed parity — the per-present distribution it shows is biased and
must not be read as a duty cycle.

*See:* issue 0007. *Generalises to:* any counter sampled near its own reset. The denominator rule
this ledger opens with has a sibling — **a counter's sample POINT is part of what it measures.**

---

## INST-19 — The frame-progress watchdog (`PSXPORT_WATCHDOG=<sec>`) used as a GUEST-progress gate — **DISTRUSTED 2026-08-05; see INST-03 for what it is genuinely good at**

*The defect, and it is structural rather than a bug:* `watchdog_pet()` is called from
`gpu_present_ex` (`external/psxport/runtime/recomp/gpu_native.cpp:1399`). Presents are driven by the
host-turn timer whether or not the GUEST advances, so the pet has **no guest-side denominator at
all**. A run can be completely wedged in guest terms and the watchdog will never fire.

*MEASURED, not argued (2026-08-05, `PSXPORT_DEBUG=presentskip`):* a windowed run produced

    presents=4027  reuse_last=4027  rebuild_geom=0  rebuild_vram=0  vram_writes=0

— the guest wrote zero bytes to VRAM across 4027 presents — and the watchdog reported nothing. The
headless leg of the same build, same duration: `presents=4106 reuse_last=2165 rebuild_geom=1511
rebuild_vram=430 vram_writes=12812`.

*What this invalidates:* **every gate result of the form "0 abort, ran N frames".** That phrasing
appears throughout this repo's logs and reports; it means "presents kept happening" and never "the
game is running". Re-read any conclusion that leaned on it.

*Trust it for:* a hard hang where the present loop itself stops, and for its backtrace when it does
fire — with INST-03's caveats (single sample, corroborate against the disassembly).
*Do NOT trust it for:* guest liveness, boot progress, or any part of a pass/fail gate.

*The missing instrument:* a watchdog fed by a **guest-side** counter — guest vblank count, VRAM
writes, or recompiled-function dispatches — printing its denominator, so a negative reads "guest
advanced 0 of N" instead of silence. Until that exists, quote the `presentskip` counters
(`vram_writes`, `rebuild_geom`) rather than the frame count.

*Structured entry:* `I014` (distrusted). *See:* issue 0005, C021, and INST-03.

---

## INST-01 — `PSXPORT_DEBUG=vsync` (VSync call log) — **trusted**

*What it shows:* every `VSync(mode)` the guest makes, with the caller's `ra` and the current vblank
counter. Implemented in `game/core/sync_native.cpp`.

*Validated:* it distinguishes the two answers it exists to distinguish. It reported a **427,643 to 1**
split between query and blocking calls — not a uniform stream — and reported *different* `mode`
values and *different* `ra` values within the same run. An instrument that could only ever print one
mode would have been indistinguishable from the truth here, so this mattered: the split is what
established CLAIM-02.

*Known limit — read this before citing it:* it can only observe calls the port actually reaches, and
the port currently stops in the disc-init retry loop during boot. So it measures the **boot** call
pattern, not the gameplay loop. Do not cite it as evidence about gameplay pacing.

---

## INST-02 — `tools/redump_ram.py` + the framework's `disasm.py` — **trusted**

*What it shows:* the guest instructions at any address, from the retail executable.

*Validated:* it produced *different, structurally coherent* disassembly at every address queried —
a recognisable Sony crt0 at the entry point, a stable-read counter loop at `0x80084BE0`, a countdown
loop at `0x80084D58` — rather than uniform garbage, which is what a wrong load-address mapping would
produce. Cross-checked independently: the string pointer the disassembly showed being loaded
(`0x80096020`) resolves to the diagnostic text `VSync: timeout`, confirming both the byte mapping and
the function identification at once.

*Known limit:* it lays down only the executable's `.text` at load time. Anything the game pages in
from `CD.WAD` at runtime is **not** in the image and will disassemble as zeros. Zeros mean "not
present in this image", not "no code there".

*Second known limit — SILENCE IS NOT "EMPTY" (found 2026-07-28):* `disasm.py` is a thin capstone
wrapper, and capstone's `disasm()` **stops at the first word it cannot decode and yields nothing
further**. Ask for a range that *begins* on data — e.g. `0x8008C3B0`, which holds a string pointer —
and the tool prints **nothing at all** and exits 0. That is indistinguishable from "this range is
empty", and it is exactly the read-silence-as-a-negative trap that has already cost this project
three false conclusions. If a range comes back empty, dump the raw words before believing it; the
code is usually there, just preceded by a literal pool or a BIOS-call stub. Nudging the start
forward past the data resumes normal output.

---

## INST-09 — `PSXPORT_DEBUG=bios` (every BIOS call, handled or not) — **trusted**

*What it shows:* one line per A0/B0/C0 dispatch — table, function number, the four argument
registers, and `$ra`. The BIOS stubs are tail jumps (`li $t2,0xA0; jr $t2`), so `$ra` survives as the
real call site. For `SysEnqIntRP`/`SysDeqIntRP` it also dumps the guest `InterruptElement`'s four
words.

*Why it exists:* the pre-existing `UNIMPL` log only fires for calls that fall THROUGH the dispatcher,
which answers "what is missing" but not "what does this game use" — and the second question is the
one that decides which BIOS subsystem a port must model next. It was also the fastest way to settle
a structure layout that no header could be trusted for.

*Validated:* it distinguishes — 21 distinct function numbers over one boot, with counts that match
independent expectations (`A0:0x39 InitHeap` exactly once from crt0; `B0:0x08 OpenEvent` eight times
against eight `B0:0x0C EnableEvent`; `C0:0x03` immediately before `C0:0x02` on the same element, the
standard deq-then-enq idiom). Cross-checked against the binary: the element words it printed
(`0x80087660`, `0x800875F8`) disassemble as a handler/verifier pair, and the register base the
verifier loads (`*0x800B12C4`) reads `0x1F801070` in the load image — the address `I_STAT` should be.
Uniform or empty output would have been the tell; it produced neither.

*Known limit:* it sees only calls that reach `Hle::dispatchBios`. A BIOS routine the guest reaches by
some other route — an address baked into a table, a driver vtable it filled itself — does not appear.
Absence here is not proof the guest never used a facility.

---

## INST-10 — `PSXPORT_DEBUG=irq` (interrupt-controller traffic) — **trusted**

*What it shows:* every `I_STAT`/`I_MASK` read and write with `$ra`, the before/after value on an
acknowledge, and an explicit line each time the CD controller raises IRQ2.

*Validated — and the first version FAILED this, which is why the entry exists.* A run with only the
read/write logging produced **35 lines and not one CD-raise**, because the raise was latched lazily
on the next `I_STAT` read and nothing in this boot reads `I_STAT`. That is the file's own rule biting:
the probe could not fire, so its silence meant nothing. Moving the latch to where the controller
actually raises made the same boot report **152 raises**. It also distinguishes values rather than
printing a constant — `I_MASK` is observed as `0x000`, `0x001` and `0x00D` at different points, and
acknowledges print a real before→after transition.

*What it settled immediately:* the guest's `I_MASK` reaches **`0x00D`** — VBlank, CDROM and DMA all
enabled — so this game does want the CD interrupt, and the remaining gap is delivery, not masking.

*Known limit:* `I_STAT` bit 2 is the only bit any source asserts, because it is the only interrupt
source the framework models. A zero in any other bit means "no source implemented", not "the hardware
was quiet". Do not read this log as evidence that VBlank or DMA interrupts did not occur.

---

## INST-11 — `PSXPORT_DEBUG=cdisr` (CD service-routine entry probe) — **trusted**

*What it shows:* every entry to libcd's CD service routine `0x8008C3E0` and its return value, via an
observe-only override that super-calls the original body (`game/core/diag_overrides.cpp`).

*Why it exists:* RE-03 turns entirely on whether that routine executes, and the two cheaper
instruments cannot answer it. A store watch on `0x800B3DF0` traces ONE byte, so it cannot separate
"did not run" from "ran and took a path that stores nothing". Host backtraces are confounded by
`-foptimize-sibling-calls`, and guest `pc`/`ra` are stale under static gen-to-gen calls (INST-07).
An override at the callee's own entry is immune to all three.

*Validated — it emits an unconditional ARM line.* This file's standing rule is that a channel-gated
probe's silence is worthless unless the run proves the probe was installed. The arm line prints, then
zero call lines follow, so **the routine genuinely never runs** — a real negative rather than an
absent instrument. Counting is decimated after the first 8 entries because the wait loop could
otherwise call it thousands of times and bury the answer.

*Extended (`PSXPORT_DEBUG=cdcb`):* the same shape now covers libcd's two INSTALLED callbacks,
`0x8009152C` (descriptor slot `+4`) and `0x800913AC` (callback #3). Neither has a static call site, so
an entry probe is the only instrument that can see them — and both report zero entries with their arm
line present.

*Known limit:* it proves the routine was never ENTERED. It says nothing about why its three ungated
call sites (`0x8008CAAC`, `0x8008CD2C`, `0x8008DA58`) are not reached — that is a separate question.

---

## INST-12 — Ghidra headless (`tools/ghidra_query.py`) — **trusted, after failing its first control**

*What it shows:* real cross-references (calls AND data), the function containing an address with its
extent, the call set of a function, annotated data dumps — and **decompiled C**.

*Why it replaces the old approach:* INST-02 (capstone + hand-rolled address scans) is what produced
this project's run of confident wrong answers — "no callers" for a routine installed into a table, a
`.data` pointer read as zero, a store blamed on the wrong `jal` because it sat in a branch delay
slot, function boundaries guessed from `jal` targets. An address scan finds only the reference FORMS
you thought to look for. Prefer this for anything about references, boundaries, or control flow.

*It FAILED its first control, and that is the entry.* A raw-binary import gives Ghidra no entry
point, so it disassembled nothing and **every** reference query returned zero — indistinguishable
from "genuinely unreferenced", the exact failure this tool exists to remove. Worse, the first
PyGhidra binding silently opened a NEW EMPTY program rather than the imported one, so memory reads
threw and the seeder reported "created 0 functions" while seeding nothing.

*Validated only after both were fixed* (`open_project` + `program_context` binds the saved program;
`tools/ghidra_seed.py` feeds it the recompiler's own 1561 function entries): a control query on
`0x8008C3E0` now returns **exactly the four `UNCONDITIONAL_CALL` sites** an independent hand scan
found — `0x8008CAAC`, `0x8008CD2C`, `0x8008D188`, `0x8008DA58` — each with its owning function.
Memory reads agree with a raw dump (`0x800B38EC = 0x80096450`).

*Run a control after any re-import.* An empty answer from this tool has now been wrong twice for
reasons that had nothing to do with the question asked.

*Known limit:* it sees STATIC references. A pointer written into a table at runtime still shows zero
call refs — for those, an entry probe on the callee (INST-11) is the instrument, not this.

---

## INST-13 — `PSXPORT_DEBUG=guest` (the game's OWN diagnostics) — **trusted**

*What it shows:* every string the executable prints through BIOS `A(3Fh) printf` / `B(3Fh) puts`.

*Why it matters more than it sounds:* these were being discarded as `UNIMPL A0:0x3F` — hundreds per
boot — which threw away the most direct evidence a port can get: the binary saying, in English, what
IT thinks is wrong. This project's single most useful identification (`VSync: timeout`) already came
from a string the executable emits; every other one was going in the bin.

*Validated, and it corrected a live conclusion within one run:* the sector headers being served
looked correct (`LBA 8850 -> 02:00:00 mode 02`, matching its Setloc exactly), from which I had just
concluded the guest's drive-position check was passing. The channel immediately reported
`CdRead: sector error` x34 and `CdRead: retry...` x68 — so the check is being REJECTED. It also
distinguishes rather than echoing one string: five distinct messages in one boot, including
`CD_init:` and `ResetGraph:jtb=...,env=...` with correctly formatted arguments.

*Known limit:* it sees only what the game routes through the BIOS calls. A title with its own serial
or screen logger prints nothing here, and silence is not evidence that the game is content.

---

## INST-04 — `tools/go_public.py scan` — **trusted, but it CAN report a false green**

*What it shows:* pre-publication scan of the git HISTORY for copyrighted/binary assets (section A),
foreign or absolute paths (section B), and docs referencing sensitive gitignored items (section C).

*Caught lying once, and this is the important entry in this file:* run against a repo whose `HEAD`
had been deleted, it reported **`RESULT: clean ✓ — ready to publish`**. There was no history to walk,
so it found nothing — and "found nothing" and "nothing is there" print identically. Re-running the
identical command after a real commit reported **0 blocking + 58 to review**. Same repo, same
content, opposite-looking verdicts.

*FIXED IN THE TOOL 2026-07-28 — this no longer relies on the reader remembering.* `report()` now
receives the commit count, refuses to issue any verdict when it is <= 0 ("NOT A VERDICT — this
repository has no commits"), and prints `scanned N commit(s) of history` on every run so the reach is
never implicit. Fixed in all three copies (this repo, Tomba2Engine, the global skill).

*The fix itself needed two attempts, which is worth recording:* the first version recomputed the
count inside `report()`, where `cwd` is not in scope, and hid the resulting `NameError` behind
`except Exception` — so it reported "could not be queried" for a healthy 16-commit repo and would
have failed closed on everything. Caught by validating BOTH directions (empty repo must refuse, real
repo must scan) rather than only the case being fixed.

*How to use it so it cannot lie:* **only scan a repo that has commits, and confirm the verdict is
non-empty.** A `clean ✓` with zero items listed in any section is the tell — a real scan of a real
repo essentially always has section-C items to review. Treat a silent all-clean as an unrun scan
until proven otherwise.

*Validated positively:* it distinguishes answers when it has history. It found a genuine blocking
item (a tilde home path in a vendored tool's comment) and, once that was fixed, correctly moved it
off the blocking list while keeping the 58 unrelated review items — rather than flipping everything
to clean at once.

*Interpreting section C:* the review items on this repo (100 at last count) are filename PATTERNS appearing as text
(`SLUS_`, `.chd`, `.bin`, `.exe`) in documentation and provisioning code that tells a user to supply
their own disc. That is intentional and portable. Sections A and B are the ones that block, and both
are empty here.

---

## INST-08 — `PSXPORT_DEBUG=cdarg` (entry-argument override) — **trusted; the one that works here**

*What it shows:* the guest ABI arguments at the ENTRY of a chosen recompiled function, logged by a
native override that then super-calls the original body (`game/core/diag_overrides.cpp`).

*Why it is the right tool:* it depends on neither of the two things that mislead in this codebase —
it does not read guest `pc`/`ra` (stale on gen-to-gen calls) and it does not walk host frames
(collapsed by `-foptimize-sibling-calls`). It runs with the registers as the caller actually set
them.

*Validated:* it reported a NON-uniform distribution (17x `a0=0x01`, 17x `a0=0x0A`) rather than one
repeated value, and that result then disproved a previously-recorded backtrace attribution — an
instrument that can overturn a standing belief is doing its job.

*Cost when off:* nothing. The override is installed only when its channel is set, deliberately: an
always-installed wrapper would insert a native frame into every call chain and perturb the very
tail-call behaviour under investigation.

---

## INST-17 — "do two resident modules share guest bytes?" — the `overlay_place()` abort — **trusted; a runtime invariant, not a check tool**

*The question this answers:* how do I know two live CD.WAD modules are not overlapping in guest RAM
— the failure that silently makes one module's code run as another's, and that killed the port at
recomp-MISS `0x800C6684`?

*The answer is that you cannot reach a state where they do.* The check is not a tool you remember to
run against a dump; it is enforced on **every** placement, in
`external/psxport/runtime/recomp/overlay_router.cpp:132-151`:

```cpp
    // The invariant the base-relative design rests on: no two resident modules overlap. Checked on
    // every placement, because the alternative is one module's code silently running as another's.
    const uint32_t lo = base & 0x1FFFFFFFu, hi = lo + size;
    for (int j = 0; j < R->overlay_count && j < Core::kRecMaxOverlays; j++) {
      if (j == i || !c->ovBase[j]) continue;
      const uint32_t jlo = c->ovBase[j] & 0x1FFFFFFFu;
      const uint32_t jhi = jlo + (R->overlays[j].end - R->overlays[j].base);
      if (lo < jhi && jlo < hi) {
        lucent::error("ovload", …);
        fflush(stderr);
        abort();
      }
    }
```

The diagnostic it aborts with names both modules and both live ranges, and states the only two
things that can produce it: *"The game's own allocator cannot produce this — it means the loader
intercept named the wrong allocation as the module body, or a body was freed without evicting it
from this registry."*

*Why this is a better instrument than any dump scanner.* A scanner samples ONE instant and its clean
verdict has to carry "cannot rule out an overlap before or after the dump". This has no sampling
window at all: `overlay_place()` is the single writer of `Core::ovBase`/`ovDelta` (see
`runtime/recomp/core.h:114`), so a violating state is unreachable, not merely unobserved. And a
scanner's silence is ambiguous — the abort's silence is not, because the *absence* of the abort over
a run is the same evidence as a green check at every load in that run.

*Note the invariant CHANGED, and is not the one a slot-era check would test.* Co-residency itself is
now **normal and correct** — L5A5LSC, LIZMAN and VENOM are simultaneously live at
`0x8014A6D0` / `0x801BDA30` / `0x801C6238` on a real boot. Only *overlap* is a fault. A check that
flags "two modules live at once" would today report a violation on every healthy run.

*Corroborating instrument for the live picture:* `PSXPORT_DEBUG=ovload` logs each placement with its
name, live range and delta from the link base, and each eviction — so the residency set over time is
observable without a dump.

*Its blind spot, stated plainly:* the abort branch itself is **not covered by a test**.
`external/psxport/tests/test_overlay_reloc.cpp` exercises 6 cases (co-resident routing, half-open
ranges, eviction, reload, per-`Core` isolation, fixed-base/unknown-name refusal) and none of them
places an overlapping pair, because tripping it calls `abort()` and the harness has no death-test
facility. So the *routing* around the invariant is proven; the abort's own firing is proven only by
reading it. Adding a death test is the outstanding work — until then, treat "it would have caught
it" as reasoning, not measurement.

---

## INST-07 — `PSXPORT_DEBUG=cdcw,cdcbt` (CD register writer) — **trusted, with one hard caveat**

*What it shows:* every write to a CD controller register with the register, value, current bank, and
guest `pc`/`ra` (`cdcw`); plus a host backtrace on the command-register write (`cdcbt`).

*Validated:* it distinguishes answers — different registers, values and banks per line, and it
resolved a caller chain that a blind instrument (INST-06) could not, then that chain was corroborated
against the disassembly (`0x8008D4E4` really does call `0x8008CE8C`, from four sites).

*THE CAVEAT — the `pc`/`ra` fields lie under recompiled execution.* The recomp does not refresh guest
`pc`/`ra` on static gen-to-gen calls. This instrument's very first output attributed a CD command
write to `0x8008B900`, which disassembles as a three-instruction getter (load halfword, return) that
touches no CD register. `ra=0` is the tell. Treat `pc`/`ra` as a hint only — a plausible non-zero `ra` is weak evidence, a zero `ra` is none.

*SECOND CAVEAT, found later: `cdcbt`'s host backtrace is ALSO confounded.* Generated code is compiled
with `-foptimize-sibling-calls` (required, or guest tail-jump loops grow the stack without bound), so
a tail call REPLACES the caller's frame. The backtrace can name a function that merely tail-called
into the chain, with intermediate frames gone. So this instrument can localise a write to a region
but **cannot be trusted to name the immediate caller** — this was later confirmed the hard way, when
an entry-argument override (INST-08) disproved a caller this backtrace had named. For a definitive answer, log the argument on
entry via a framework override + super-call rather than reading frames.

---

## INST-15 — `PSXPORT_SELFTEST=mdecpump` (MDEC pending-DMA / voffs placement A/B) — **trusted, negative-validated**

*What it shows:* whether the guest-visible MDEC DMA path (pending-channel latch in `mem.cpp
mdec_dma_pump`, per-word `voffs` scatter into guest RAM) produces BIT-IDENTICAL output to the
independently verified native_fmv drain path, on a synthetic 16-macroblock 16bpp frame. Also asserts
the mechanism itself engaged: DMA0's busy bit must still be SET right after its start (deferral
really happened) and both busy bits CLEAR after DMA1's start completes the ping-pong.

*Validated both ways (2026-07-30):* passes on the real code; deliberately zeroing `offs` in the pump
made it FAIL with `964 of 2048 words` differing. It also guards its own blind spot: an all-alike
reference frame (which would compare equal under any placement bug) is itself a FAIL — the test
requires >= 16 distinct output words before the comparison counts.

*Known limits:* single whole-frame DMA1 at 16bpp with block size 0x20 — it does not exercise 24bpp
(WWS=6), multi-transfer-per-frame DMA1 (per-strip `DecDCTout`), or a DMA1-first start order. Those
paths are reasoned + traced, not A/B-proven.

*Run:* `PSXPORT_SELFTEST=mdecpump ./scratch/bin/spiderman_port scratch/bin/SLUS_008.75` (exit 0/1).

---

## INST-14 — `re_frontier.py check` — **FIXED 2026-07-29; my first diagnosis was WRONG**

*What it claimed:* "re-frontier OK: no unknown deps, no cycles, every re-verified step cites
evidence." It printed that on every run for a whole session, including right after edits that
introduced a contradiction, because it was parsing **zero entries**.

*The cause I recorded first, and it was not the main one.* I wrote that the skill's parser wants
`### <ID> — <title>` with `- status:` fields while this file used `## RE-05 — … — <status>`, so every
step was being read as an AREA. That IS a real defect and the file did need converting — but it was
the SECOND problem, and fixing it alone changed nothing.

**The primary cause was that the tool was reading a file that does not exist.** `ROADMAP` defaults to
`<parent-of-script>/docs/re-frontier.md`, which is correct only when the tool lives at
`<repo>/tools/`. Running from the global skill dir it resolved to
`<skills-dir>/docs/re-frontier.md` — a path that never exists. And `load()` opened with

    if not os.path.exists(ROADMAP):
        return {}, []

so a missing roadmap was **indistinguishable from a healthy one**: zero entries, then a green check
over nothing. That silent early-return is the worst failure a validator can have, and it is what made
the output uniform.

*Fixed, in both places:*
- the global `re-frontier` skill now **exits non-zero** with the path it
  looked for and the `RE_FRONTIER_ROADMAP` override to set — the early-return is deleted;
- this project's `docs/re-frontier.md` is converted to the machine-readable schema, with duplicate
  IDs resolved (`RE-03` appeared three times, `RE-04`/`RE-05` twice each on *different* topics —
  the OT/packet-pool and scheduler steps are now `RE-12`/`RE-13`, and the two superseded `CdInit`
  investigations are `HIST-03a`/`HIST-03b`, `skip-by-design`).

**Invoke it with the path — the bare command is still wrong for this repo:**

    RE_FRONTIER_ROADMAP=docs/re-frontier.md python3 "$CLAUDE_SKILLS/re-frontier/re_frontier.py" check

*Validated, and it can now show the OTHER answer — which is the whole bar:* with no roadmap it exits
1 and says so; with the roadmap it rendered the full dependency tree, listed 5 RE-ready steps, and
**immediately failed with 11 real problems** (re-verified steps whose evidence lived only in prose,
with no machine-readable `- evidence:` field). Those are now filled and it passes. A tool that could
only ever print OK produced none of that.

*The reusable lesson, and it is the same one this page keeps teaching:* I diagnosed a uniform-output
instrument from its OUTPUT FORMAT and stopped there. The cheap check I skipped was "does the input
file it reads actually exist" — one `os.path.isfile` away.

*Invocation correction (2026-08-05):* use the IN-REPO path, `python3 tools/re_frontier.py`, from the
repo root. `$CLAUDE_SKILLS` is unset in a plain shell, so the form above collapses to
`/re-frontier/re_frontier.py` and fails. The `RE_FRONTIER_ROADMAP=docs/re-frontier.md` prefix is
still required.

*SECOND DEFECT, found 2026-08-05 and it DESTROYS DATA: `re_frontier.py set` silently deletes any
field not in its schema.* `FIELDS = ["status", "area", "deps", "evidence", "where", "gap", "notes"]`
(`tools/re_frontier.py:61`); a step is re-serialised from that list on every `set`, so anything else
is dropped with no warning and no diff the tool shows you. **Measured:** a `set RE-07 status=... gap=...`
silently removed RE-07's hand-written `- where-2:` line (the per-channel DMA-completion `where`),
caught only by reading `git diff` afterwards. It has been merged into `where:` and is not lost.
**So: never add a non-schema field to this file, and always read `git diff` after a `set`.** The
proper fix is for `set` to preserve unknown fields (or refuse), and it is NOT done.

*THAT SECOND DEFECT IS FIXED 2026-08-05 — and the paragraph above is now out of date on purpose,
kept because it names the failure.* The cause was bigger than the field schema: `save()`
REGENERATED the whole file from the parsed model, so unknown fields, the file's own header and
1846 lines of prose all died the same death (issue 0003). `tools/re_frontier.py` now edits the
lines it means to change and copies every other byte through, and every write is gated on a
preservation check that refuses (exit 2, naming the lost lines, writing nothing) if anything else
would go missing. **You may again add a non-schema field** — `show` reports it as
`(extra field this tool does not parse, kept verbatim)`.

*Validated against BOTH classes, which is the bar this page keeps setting.* Same harness, same
corpus (the 1979-line prose roadmap, `git show 74af0c6:docs/re-frontier.md`): against the OLD tool
`re_frontier.py selftest --tool <old> --corpus …` reports **1562 of 1641 non-blank lines DROPPED**
and exits 1; against the new one, **0 lost** and exit 0. The preservation gate itself was proven to
fire by sabotaging the editor. Run it any time: `python3 tools/re_frontier.py selftest`, or
`pytest tools/test_re_frontier.py` (2 tests: the embedded prose fixture and this repo's real
roadmap).

*A THIRD defect, found the same day: the INST-14 fix above never reached THIS repo's copy.* The
entry says the early-return was deleted "in the global skill"; `tools/re_frontier.py check` on a
missing roadmap still printed "re-frontier OK" and exited 0 as of 2026-08-05 — the exact
green-over-nothing this entry exists for, alive in the copy the CLAUDE.md tells you to run. It now
exits 1 with "does not exist — checked NOTHING", `add` refuses to create a roadmap at a mistyped
path, and every OK line carries its denominator (`23 entr(ies) parsed from docs/re-frontier.md
(218 lines)`). **Lesson: a fix recorded against a tool is not a fix of every copy of that tool.**

*A FOURTH defect, 2026-08-11 — the same entry's own failure mode, still live in the OTHER HALF of the
case split, plus proof the "lesson" above had to be learned twice.* The MISSING-FILE case refused
correctly. A roadmap that **existed and parsed ZERO entries** printed `re-frontier OK: … no unknown
deps, no cycles, every re-verified step cites evidence` and exited **0** — every one of those claims is
**vacuously true over an empty set**. Measured on all three drifted copies (they are 890 / 443 / skill
lines — they have diverged enormously):

| copy | before | after |
|---|---|---|
| `spider1/tools/re_frontier.py` | exit 0, but did print `0 entr(ies)` | exit 1 |
| `spyro/tools/re_frontier.py` | exit 0, **no count printed at all** | exit 1 |
| `~/.claude/skills/re-frontier/re_frontier.py` | exit 0, **no count printed at all** | exit 1 |

`next` was worse than useless on an empty parse: it printed *"(none — every unblocked step is done, or
blocked on upstream RE)"*, i.e. it told the reader the project was FINISHED when the roadmap had never
been read. It now distinguishes the two cases explicitly. All three copies verified in BOTH directions
after the fix — empty roadmap exits 1, each repo's real roadmap still passes (spyro 28 entries, spider1
30).

**Two lessons, and the second is the expensive one.** (1) A file that exists but yields nothing is a
BROKEN PARSER, not a clean document — "exists" is the wrong existence check. (2) Fixing "the missing
input case" does not fix "the empty input case", and a case split is exactly where a
green-over-nothing hides after the obvious half is patched. Also note this entry itself said the third
defect was fixed, and the tool was still lying in a neighbouring branch — **a registry entry claiming a
fix is not evidence the class is dead; re-run the discriminator against both classes.** This is the
concrete argument for hoisting these tool ENGINES into one place (psxport) with per-game DATA, since
this bug has now been fixed four times in three copies.

---

## INST-06 — `PSXPORT_WWATCH` (guest store watch) — **TRUST RESTORED 2026-07-28 — my distrust was wrong**

*What it should show:* the guest `pc`/`ra` of any store landing in an address range
(`PSXPORT_WWATCH=lo,hi`), which is exactly the tool for "who writes this register?".

**RETRACTION.** This entry marked the tool distrusted. Re-tested directly: it arms correctly
(`cfg_str` returns the range, `sscanf` parses `lo=800B397C hi=800B3980`) and fires — **892 hits** over
a boot on the validation address. The tool works. Leaving it marked distrusted was the more damaging
error, because it steers the next session away from a working instrument toward hand-rolled probes.

Why it reported zero earlier is **not established**. The likeliest candidates are an intervening
framework change to the per-guest-write path, or a flaw in my original test. Recorded as unresolved
rather than guessed. Validate before relying on it — but expect it to work.

*Discrimination re-validated 2026-07-29 — it can show the OTHER answer.* The strongest check yet, and
it came free: within the same binary and the same boot, the watch reports **62,114 hits** on
`0x800A5130` (the game's per-frame pad mirror) and **4 hits** on `0x800A50EC` (the libpad buffer,
written only by init). A blind instrument returning a uniform answer cannot produce those two numbers
from the same run. It also attributes them to *different* writers (`0x8006B3C8` vs `0x80091330`),
which a stuck instrument could not do either. This is the "must be able to report the other answer"
bar, met on real data rather than on a synthetic probe — treat the tool as trusted for both
directions, including the negative ("nothing writes this").

*The original (incorrect) reasoning, kept because the discipline was right even though the verdict
was wrong:* it reported **zero hits on an address that is written continuously**. Armed
over the vblank counter `0x800B397C..0x800B3980` — which this port's own native VSync writes on every
call, thousands of times a run — it logged nothing at all. A tool that cannot see a guaranteed write
cannot be used to prove a write does not happen.

*What it nearly cost:* it was armed over the CD command register `0x1F801801` to answer RE-03's open
question, returned zero hits twice, and the obvious reading — "the guest never writes the command
register" — is a substantive claim about the game that would have gone into the frontier notes as
fact. It is not supported by anything. **Both zero-hit results are void.**

*Two traps found while testing it, worth knowing if it is ever repaired:*
  * `wwatch_check` ORs `0x80000000` into the store address before comparing, but the env-arm path
    (unlike the programmatic `wwatch_arm`) does NOT normalise the configured bounds. So an I/O watch
    must be armed in KSEG1 form (`9f801801`), not `1f801801`, or it silently never matches. This is
    documented in a comment in `mem.cpp` and is easy to walk straight into.
  * The env parse shadows the width parameter `w` with the config string `w` inside `wwatch_check`.
    Harmless as written, but it is the kind of thing to check first.

*Root cause not established.* Only SBS calls `wwatch_arm`, and SBS was not running, so the
"pre-armed, env never read" theory does not explain it. Ordering is not the problem either —
`wwatch_check` runs before `io_write` in `mem_w8`, so I/O stores do pass through it.

*Before using it again:* re-validate against a known-written address and confirm it fires. Treat a
zero result as "unproven", never as "does not happen".

---

## INST-05 — `PSXPORT_DEBUG=cdc` (framework CD-controller channel) — **trusted**

*What it shows:* every command the guest issues to the modelled CD controller
(`external/psxport/runtime/recomp/cdc_native.c`), with its parameters, and whether the model handled it.

*Validated:* it distinguishes answers — it names a specific command byte and reports handled versus
`UNHANDLED`, and it is the reason RE-03 has a mechanism rather than a guess. It turned "CdInit returns
0 for some reason" into "the guest issues command 0x00 77 times and the model acks each one".

*Known limit:* it reports what the MODEL received, not what the guest meant. `cmd 0x00` means a zero
reached the command register with index 0 — it does not establish that the guest intended a command.
That distinction is exactly the open question in RE-03, so do not read this channel as intent.

---

## INST-03 — The watchdog backtrace (`PSXPORT_WATCHDOG=<sec>`) — **trusted, with a caveat**

*What it shows:* where the process was when no frame got presented in time — the framework's stall
diagnostic.

*Validated:* it distinguished states rather than always saying the same thing. Across the port's
successive stalls it named *different* locations — the libetc VSync deadline loop, then the disc-init
retry loop — and each pointed at a cause that, once addressed, moved the stall somewhere new. That is
the behaviour of a working instrument.

*Second caveat, which DID mislead here:* a single sample lands wherever the process happens to be,
not necessarily at the cause. On this port it repeatedly sampled inside the 100-vblank wait of a
retry loop, which says only "it is retrying" — the actual failure was one call further on. Worse, a
backtrace taken against the seed-contaminated substrate showed a call chain (`0x8008CE8C`) that does
not exist in the clean disassembly, and it was written into the frontier notes before being caught.
**Corroborate any backtrace against the disassembly before recording it as a call chain.**

*Caveat that cost time here:* it is a **single sample**, so it cannot by itself distinguish "spinning
in a tight loop" from "making slow progress". Confirm which by sampling more than once and comparing
— an identical backtrace across independent runs is the spin signal. Attaching an external sampler
(`gdb -p`, `eu-stack -p`) did **not** work in this environment; repeated runs are the practical
substitute.
