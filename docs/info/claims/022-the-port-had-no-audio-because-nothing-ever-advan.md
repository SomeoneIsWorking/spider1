---
id: C022
kind: claim
status: holds
created: 2026-08-05
tags: audio,xa,spu,intro,fmv,sync,re-07,issue-0005
depends: game/core/sync_native.cpp, game/core/main.cpp, external/psxport/runtime/recomp/spu_audio.cpp, external/psxport/runtime/recomp/xa_stream.cpp
---

## Claim

The port produced NO audio of any kind — not just silent intro FMVs — because `spu_audio.frame()` was called nowhere in it: the SPU mixer never advanced a clock, and the mixer is the only thing that pulls `CDC_GetCDAudioSample`, which is what decodes XA-ADPCM sectors off the disc. Driving it once per owed field from `vblank_advance` produces real audio.

## Evidence

FALSIFIED FIRST, the hypothesis this replaces: `discdump subhdr` over three ranges with a positive control — CINEMAS/ATVILOGO.STR (LBA 128304) 199 sectors read, 6 XA-audio-flagged (submode 0x64, at LBA 128335/128367, ~1 in 32); CINEMAS/LOGO.STR (LBA 280000) 199 read, 25 flagged; COMPILED.XA (LBA 31263) 199 read, 199 flagged. The STRs DO carry interleaved XA audio, so "the audio lives in COMPILED.XA, the player reads the wrong file" is wrong. The earlier "0 of 66 audio sectors" was measured on `native_fmv.cpp`, which does not play this game's intro at all: boot logs "no boot FMV configured (GameConfig::bootFmv is empty)" and the guest's own libstr/MDEC path plays the movies. MEASURED, guest ground truth: `PSXPORT_DEBUG=cdc` shows the guest programming Setmode 0xE0 for the STR stream (speed=2, XA-ADPCM enable 0x40 SET, whole-sector 0x20 set), so the drive is genuinely being told to decode audio; and 336 of 4232 sectors presented to the data FIFO over a boot are XA-audio-flagged. MEASURED, the decisive census (added to XaState as `pulls`/`sectors`, reported at stream stop so a silent stream names WHICH side failed): BEFORE "[xa] STOP @ LBA 128304 - 0 pull(s) from the SPU, 0 audio sector(s) decoded" — the head never moved off the start LBA; AFTER "[xa] STOP @ LBA 128816 - 149940 pull(s), 16 audio sector(s) decoded" and "[xa] STOP @ LBA 280552 - 160965 pull(s), 69 audio sector(s) decoded". 16 audio sectors over a 512-sector span matches the disc's measured ~3% interleave. MEASURED, output: scratch/wav/intro_after.wav, 99 s / 17.6 MB captured headless on the real disc, 198 buckets of 0.5 s, 115 audible (peak > 64, peaks to 16572), 83 silent — the same analyser separates both classes inside one file. Static fact behind all of it: `main.cpp:74` calls `spu_audio.init()` and `grep -rn spu_audio game/` returned that one line, no `frame()` call anywhere. Logs: scratch/logs/cdc_census.log, xa_probe.log, after_guard.log, wav_run.log.

## What would falsify it

if a windowed run with the fix in place is still silent to the user, then advancing the mixer is necessary but not sufficient and the remaining fault is in the SDL sink or the CD-to-SPU mix volume (SPUCNT bit0 / CDVol), neither of which this measured; equally, if `pulls` is ever observed non-zero with `sectors` still 0, the disc side is at fault and this claim's causal direction is wrong. Scope: this establishes that audio is PRODUCED and reaches a WAV capture. It does NOT establish A/V SYNC — the XA streamer and the guest's video path run two independent read heads over the same file, and their relative pacing is unmeasured. Nor is it confirmed by the user in a window, which is what actually closes issue 0005
