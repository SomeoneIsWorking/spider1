---
id: 2
title: Every presented frame is pure black from frame 1 to 1600+ (display config also collapses to 512x2)
status: open
symptom: black screen, no Activision logo visible, audio plays but nothing is drawn, presented frames 100% black
tags: render,gpu,fmv,black-screen,handoff
created: 2026-08-04
updated: 2026-08-04
---

HANDED BACK TO THE COORDINATOR — found while root-causing the `recomp-MISS 0x800C6684` abort (issue #1),
NOT investigated. Very likely the user`s ACTUAL reported symptom ("boots into black screen — I guess
they want to show Activision logo but it is absent"), and separate from that abort.

**The measurement.** Headless run, frames dumped with `PSXPORT_SHOT_AT`:

    cd /home/bhamil/repo/psx/spider1
    export PSXPORT_ASSET_DIR=external/psxport PSXPORT_VK_HEADLESS=1 PSXPORT_NOAUDIO=1
    export PSXPORT_SPIDERMAN_DISC="<disc>.chd"
    PSXPORT_SHOT_AT=1,2,3,5,10,20,40,80,150,300 PSXPORT_WATCHDOG=60 \
      ./scratch/bin/spiderman_port scratch/bin/spiderman/SLUS_008.75

Every dumped frame is **100.0%% pure black** — not "mostly black", not a dark image: the pixel
histogram has exactly one bucket, (0,0,0), at 100.0%%. Checked at frames 1, 2, 3, 5, 10, 20, 40, 80,
150, 300 in one run and 1, 50, 100, 200, 400, 800, 1600 in another. Log: scratch/logs/fmv_on.log,
scratch/logs/mod_dbg.log.

**Second observation — the display config collapses.** The per-shot geometry the presenter reports:

    frame 1     320x240 @ 0,0
    frame 3     512x240 @ 0,0
    frame 5-100 320x240 @ 0,256
    frame 150   512x2   @ 0,0      <-- 2 SCANLINES
    frame 300   512x2   @ 0,0
    frame 800   512x240 @ 0,0      (recovers later in the longer run)

A 512x2 display area is not a plausible game state; something is programming the display registers
with a degenerate height, or the presenter is reading them wrong, for a stretch of hundreds of frames.

**Why it is probably NOT the module abort.** The port gets a long way past boot: it loads and unloads
the front-end shell correctly (`SHELL/THUG/SHELL/COP/SHELL/SHELL`, each bracketed by an unload) before
the co-residency abort, and the frames are already black at frame 1, long before any of that.

**Why the FMV path is live.** The user HEARS the Activision logo, so the guest is playing
`CINEMAS/ATVILOGO.STR` through its own STR streaming; audio reaches the device and video does not.
Note headless silences audio unconditionally (`spu_audio.cpp:93`, `!gpu_windowed()`), so the audible
evidence only exists in the user`s WINDOWED run — do not expect to hear it in a headless repro.

**Also settled and worth carrying:** `GameConfig::bootFmv` is `{nullptr,...}` (game_config.cpp:92), so
the framework`s native boot-FMV player is not wired for this game and `PSXPORT_NO_FMV` gates nothing
this port uses. The guest plays its own movies. The `/CINEMAS/TTSLOGO.STR;1` CdSearchFile miss is
expected — that file genuinely is not on the retail disc (the disc carries ATVILOGO.STR and LOGO.STR).

**Not started.** No owner, no hypothesis tested.
