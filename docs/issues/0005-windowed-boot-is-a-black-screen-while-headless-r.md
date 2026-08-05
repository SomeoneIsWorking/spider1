---
id: 5
title: Windowed boot is a black screen while headless renders the intro movies: the swapchain present mode defaults to VSYNC and blocks the GUEST thread
status: open
symptom: Spider-Man boots into a black screen in a window; headless runs of the same build show the Activision and Neversoft logos at 99.95% / 25.70% non-black. Windowed is 0.00% non-black / 1 colour at every present index to f2400, and the guest writes ZERO bytes to VRAM over 4027 presents
tags: black-screen,fmv,windowed,headless,swapchain,vsync,gpu,re-07,re-04,starvation
created: 2026-08-05
updated: 2026-08-05
---

Measured 2026-08-05 on spider1 `3381fcc` / psxport `3f6a1e14` — the same build RE-07 was declared
complete on. One build, one variable changed per comparison.

**Everything under "MEASURED" was observed. Everything under "INFERRED" is reasoning from it.**

## The symptom, as the USER stated it

> "Spider-Man boots into a black screen"

Observed in a **window**. RE-07 had just been closed with "both intro logo movies play", so the
registry and the user disagreed. The user was right.

## Why the registry disagreed: every RE-07 number was taken HEADLESS

MEASURED. RE-07 / CLAIM-C019 reported f120 = 99.95% non-black / 11395 colours (Activision),
f300 = 25.70% / 8773 (Neversoft), f4000 = 99.44% (menu). All of those were produced under
`PSXPORT_VK_HEADLESS=1`.

MEASURED, same build, windowed: **0.00% non-black / 1 colour at every present index out to f2400.**
Not a degraded picture — nothing at all.

## Root cause

MEASURED (static, in the source):

* `external/psxport/runtime/recomp/gpu_vk.cpp:498` claims the window with
  `SDL_ClaimWindowForGPUDevice` and never calls `SDL_SetGPUSwapchainParameters` — the symbol
  appeared nowhere under `runtime/` at the time of measurement.
* `SDL_WaitAndAcquireGPUSwapchainTexture` is at `gpu_vk.cpp:1007`, inside
  `GpuVkState::show_composite`.
* That present is reached on the **guest thread**: `game/core/sync_native.cpp:202`
  `vblank_advance()` -> `gpu_present(c)`, called from every `VSync()`, every `FUN_8005E748`
  field-wait iteration, and every host turn (`sync_native.cpp:287`, `:297`, `:326`). There is no
  I/O thread in this port — the CD pump, MDEC, DMA completions and the guest all run on that one
  thread.
* Headless never blocks: `s_win` is null, so the acquire fails immediately at `gpu_vk.cpp:1007-1009`
  and `show_composite` returns.

MEASURED (a stack, not a guess): `scratch/logs/windowed_run1.log` holds the blocking backtrace —
`show_composite` -> SDL3 -> `libvulkan_radeon` -> `drmSyncobjTimelineWait` -> `ioctl`, entered via
`rec_irq_poll` -> `rec_host_turn`.

INFERRED, and it is the whole mechanism: with no swapchain parameters set, SDL's **default present
mode is VSYNC**, so the acquire blocks the calling thread until vblank. Because the caller is the
guest thread, windowed hands the guest what is left of a 16.6 ms budget after the block, while
headless hands it ~100% of the CPU. The guest is not rendering black — it is being **starved** and
never gets far enough to have anything to render.

## Evidence, with denominators

MEASURED. `PSXPORT_DEBUG=presentskip`, ~4100 presents in each run:

    HEADLESS  presents=4106  reuse_last=2165  rebuild_geom=1511  rebuild_vram=430  vram_writes=12812
    WINDOWED  presents=4027  reuse_last=4027  rebuild_geom=0     rebuild_vram=0    vram_writes=0

`vram_writes=0` over 4027 presents is the decisive number: the guest wrote nothing to VRAM at all.
Every windowed present reused the last (empty) composite.

MEASURED. Windowed, in 68 s, never reached any of:

* `[disc] opened`
* `[sync] VSyncCallback: guest registered`
* `[gpu] display depth -> 24-BIT`

Headless passes all three **before present index 60**.

## The decisive control — zero code changed

MEASURED. Windowed, same binary, same disc, same env plus `MESA_VK_WSI_PRESENT_MODE=immediate`
(which overrides the driver's present mode from outside the process):

    presents=4083  reuse_last=2933  rebuild_geom=654  rebuild_vram=496  vram_writes=11076

All three milestones reached, and the movie appears: f120 = 17.97% non-black / 2197 colours,
f200 = 50.76% / 6279, f300 = 36.35% / 6035.

This is a one-variable A/B on an unmodified binary. It isolates the present mode and nothing else,
which is why it settles the attribution rather than merely being consistent with it.

Logs: `scratch/logs/ps_head_full.log`, `win_upload.log`, `win_immediate.log`, `windowed_run1.log`.

## Not the cause (ruled out by the control above)

* Not the renderer, not the decoder, not the DICR gate, not the recompiler. Every one of those is
  identical between the two legs; only the present mode differed, and the movie appeared.
* Not a windowed-only render path. Nothing in the geometry or VRAM path branches on headless.

## Why this was invisible for a whole session — two instrument defects

Both are recorded in `docs/info/instruments.md` (INST-18, INST-19) and both certified a FALSE
NEGATIVE here.

1. **`PSXPORT_SHOT_AT` cannot see this defect the way it was used.** `GpuVkState::shot()`
   (`gpu_vk.cpp:1202`) -> `dump_to()` (`gpu_vk.cpp:1171-1201`) reads back the **guest VRAM texture**
   `s_vram_tex` and decodes the display region itself. It never samples the swapchain. It is
   truthful about "what is in guest VRAM"; it was read as "what the player sees", and it also
   cannot distinguish "the guest drew nothing" from "the window shows nothing".
2. **The watchdog is structurally incapable of reporting guest starvation.** `watchdog_pet()` is
   called from `gpu_present_ex` (`gpu_native.cpp:1399`). Presents keep flowing from the host-turn
   timer whether or not the GUEST advances, so a windowed run can be wedged in guest terms and
   never trip the watchdog. Every "0 abort, ran N frames" gate result inherits this blind spot.

## Cross-references

* **Issue 0004** — the FMV chain (DICR gate, RE-16, per-channel DMA completions). Those fixes are
  real and are not undone by this; they are what made the movie appear the moment the guest got CPU
  in the `MESA_VK_WSI_PRESENT_MODE=immediate` control.
* **RE-07** downgraded `re-verified` -> `re-partial`; **RE-04** likewise (its evidence only ever
  justified the descriptor-table RE).
* **CLAIM-C019** falsified as written (it claimed the movies "reach the screen"); re-issued scoped
  to headless as C020. Root cause recorded as C021.
* `docs/../PROTOCOL.md` "HEADLESS AND WINDOWED ARE ONE CODE PATH" — this is that rule's failure mode
  in its purest form. The split here is not a code branch anyone wrote; it is a **scheduling**
  divergence created by a swapchain parameter that was never set. A rule that only forbids
  `cfg_on("PSXPORT_VK_HEADLESS")` branches would not have caught it.

## The fix, and what it measured (2026-08-05)

`runtime/recomp/gpu_vk_present_mode.h` adds the pure policy `preferred_present_mode(mailbox_ok,
immediate_ok)` -> MAILBOX / IMMEDIATE / VSYNC, and `gpu_vk.cpp` calls
`SDL_SetGPUSwapchainParameters` immediately after `SDL_ClaimWindowForGPUDevice`, gated on
`SDL_WindowSupportsGPUPresentMode`, logging the mode actually in effect. Nothing else changed; the
change is AT THE SINK, which is the one place windowed and headless are allowed to differ.
(Framework patch: `coord/patches/gpu-vk-swapchain-pacing.diff`. RED first: with the policy returning
VSYNC unconditionally — today's behaviour — 3 of 5 cases in `tests/test_swapchain_present_mode.cpp`
fail; after the fix 6/6, whole suite 11/11.)

MEASURED, windowed, same flags as the runs above, this machine picks **MAILBOX**:

    BEFORE  presents=4027  reuse_last=4027  rebuild_geom=0    rebuild_vram=0    vram_writes=0
    AFTER   presents=4087  reuse_last=2977  rebuild_geom=665  rebuild_vram=445  vram_writes=11098

All three milestones now reached windowed, and the shots show the movie: f120 14.03% non-black /
2062 colours, f200 42.36% / 5559, f300 40.88% / 6283, f600 24.41% / 8187 (`scratch/logs/win_fixed.log`,
`scratch/screenshots/win_fixed/`). Headless is NOT regressed: presents=4111 reuse_last=2168
rebuild_geom=1515 rebuild_vram=428 vram_writes=12822 vs the 12812/1511 baseline
(`scratch/logs/head_fixed.log`).

## The residual divergence is NOT the swapchain — it is `gpu_pace_subframe`

MEASURED, windowed, `PSXPORT_NOPACE=1`, everything else identical:

    presents=4094  reuse_last=2164  rebuild_geom=1502  rebuild_vram=428  vram_writes=12796

That is headless, to within noise. So after the present-mode fix the remaining windowed deficit
(rebuild_geom 665 vs 1502, vram_writes 11098 vs 12796) is entirely `gpu_pace_subframe`
(`external/psxport/runtime/recomp/gpu_native.cpp:1340/1348`), which sleeps only when a window is up
and — because spider1's `GameConfig::paceQuota` is 0 — derives its quota from `mem_r8(0x1F800235)`,
an un-RE'd scratchpad byte that is ordinary working memory in this game. Separate defect, not fixed
here.

## Status

OPEN. The measurements above are ours; per PROTOCOL a user-reported bug is closed by the USER. The
window now reports `[gpu_vk] swapchain present mode: MAILBOX` and our shots show the logos — the
user needs to confirm they see them.

## 2026-08-05 — the XA submode read is CORRECT; suspect the wrong FILE

Narrowing, so the next session does not start where this one did.

`native_fmv.cpp:231` classifies an audio sector with `int submode = raw[18]; if (submode & 0x04)`.
Both halves check out:

- `disc_read_raw` (`disc.cpp:108`) copies from the CHD hunk at `RAW_FRAME` stride — a FULL 2352-byte
  raw frame including the 12-byte sync and 4-byte header. So the CD-XA subheader is at `raw[16..23]`
  and `raw[18]` genuinely is the submode byte.
- `0x04` is the correct CD-XA submode bit for an audio sector.

So "0 of 66 and 0 of 277 audio sectors decoded" is NOT a broken decode of sectors we found. It is
consistent with there being no XA-audio sectors interleaved in these STR files at all.

**Hypothesis to test first (NOT verified — do not act on it before measuring):** this disc carries a
separate `COMPILED.XA` (LBA 31263, 198737920 bytes) alongside `CINEMAS/*.STR`. If the intro movies
are video-only STR with their audio streaming from `COMPILED.XA`, then the FMV player is reading
audio from the wrong file by construction, and no amount of fixing the decoder will produce sound.

**The measurement that settles it, and it is cheap:** dump the submode byte (`raw[18]`) of the first
~200 sectors of `CINEMAS/ATVILOGO.STR` (LBA 128304) and histogram it. If no sector has bit 2 set,
the STR is video-only and the question becomes "who streams COMPILED.XA, and when". If sectors DO
have bit 2 set, the classification path is being reached differently than assumed and the decode
side is back in scope.

Report the denominator either way — "no audio sectors" means nothing without "of N sectors read".

## 2026-08-05 — RESOLVED (the audio half): the port never drove the SPU mixer

The `COMPILED.XA` hypothesis above is **FALSIFIED**, and so was the premise underneath it. Measured
with the existing `discdump subhdr` tool against three sector ranges, including a positive control:

| range | sectors read | XA-audio-flagged (submode bit 2) |
|---|---|---|
| `CINEMAS/ATVILOGO.STR` (LBA 128304) | 199 | **6** (submode 0x64) |
| `CINEMAS/LOGO.STR` (LBA 280000) | 199 | **25** |
| `COMPILED.XA` (LBA 31263) — positive control | 199 | 199 |

The STRs *do* carry interleaved XA audio (one audio sector every ~32, e.g. LBA 128335, 128367). So
the audio was never in the wrong file.

**The `0 of 66` figure was measured on the wrong player.** `native_fmv.cpp` does not play this
game's intro at all — boot logs `no boot FMV configured (GameConfig::bootFmv is empty)` and the
guest's own libstr/MDEC path plays the movies. The audio path for those is `xa_stream.cpp`, armed
from `cd_override.cpp` on ReadS.

**Root cause: nothing ever advanced the SPU mixer.** `main.cpp:74` calls `spu_audio.init()` and
`spu_audio.frame()` was called *nowhere in this port*. The mixer is what pulls
`CDC_GetCDAudioSample`, which is what decodes XA sectors — so the streamer armed at the movie's LBA
and stopped there without reading one sector. That is not an intro bug: the port had **no audio at
all**, SPU voices included.

Evidence, from a census added to `XaState` (`pulls` / `sectors`, reported at stop so a silent stream
says WHICH side failed):

    before:  [xa] STOP @ LBA 128304 — 0 pull(s) from the SPU, 0 audio sector(s) decoded
    after:   [xa] STOP @ LBA 128816 — 149940 pull(s) from the SPU, 16 audio sector(s) decoded
    after:   [xa] STOP @ LBA 280552 — 160965 pull(s) from the SPU, 69 audio sector(s) decoded

16 audio sectors over a 512-sector span matches the disc's measured ~3% interleave exactly.

**The fix** (`game/core/sync_native.cpp`): drive `spu_audio.frame()` once per OWED field from
`vblank_advance`, the port's field clock — the same seam the framework's own native frame loop calls
"tick + per-vblank audio + present + pace". Per owed field rather than per presented frame: presents
are capped for catch-up, and throttling audio to that cap drifts sound behind picture under load.

**Two defects fell out of wiring it up, both fixed:**

1. `spu_audio.cpp:176` called the OPTIONAL `audioMixFrame` hook guarded only by `if (game)`. This
   port binds hooks by name, so unbound hooks are null — the first frame of audio segfaulted at
   address 0. The framework's own idiom for optional hooks is a null test (`native_boot.cpp:422`).
   Extracted as `spu_mix_game_audio()` and gated by `tests/test_spu_mix_optional_hook.cpp`
   (3 cases / 9 checks, shown RED as SIGSEGV before the guard, and it asserts the hook IS still
   called when a port supplies one).
2. `PSXPORT_WAV` captured nothing on exactly the runs it exists for. `wavClose` patches the RIFF
   sizes from an `atexit` hook, but the watchdog ends on SIGINT/SIGTERM with `_exit(130)` and no
   port here ever returns from its frame loop — so every headless capture was a 0-byte file with
   the PCM still in the stdio buffer. The header is now patched + flushed about once a second, so
   the capture is a valid WAV at all times.

**Verification, headless, real disc** (`scratch/wav/intro_after.wav`, 99 s, 17.6 MB): 198 buckets of
0.5 s, **115 audible** (peak > 64, peaks to 16572), 83 silent. The analyser distinguishes both
classes within the one file, so "audible" is not an artefact of the measurement.

**NOT closed by me.** The user reports and the user closes: this needs a windowed run where they
actually hear the intro.
