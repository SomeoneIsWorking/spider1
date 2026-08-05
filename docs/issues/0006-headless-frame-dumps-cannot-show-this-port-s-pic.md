---
id: 6
title: headless frame dumps cannot show this port's picture - the SW rasterizer is bypassed when VK is on
status: open
tags: render,headless,instrument,vk
created: 2026-08-05
updated: 2026-08-05
---

Cost most of a session's measurements on spider1 AND spyro. Recording the mechanism so nobody re-derives it.

SYMPTOM: every headless frame capture is a single flat colour. PSXPORT_GPU_DUMP under PSXPORT_NOWINDOW, and again under PSXPORT_VK_HEADLESS: 60 frames, ZERO with more than one colour, and only 3 distinct colours across the whole run (black, grey, pale yellow). It reads exactly like "the port renders nothing".

IT DOES NOT. The guest is drawing the whole time. PSXPORT_DEBUG=gpu reports 610 prims / 6528 gp0words per frame, 17852 frame lines in one replay, and the display double-buffers correctly, alternating disp 512x240 at (0,0) and at (0,256) roughly 50/50 (8908 vs 8659 frames).

WHERE THE PICTURE ACTUALLY IS. Full VRAM dump (PSXPORT_VRAMDUMP) tiled 64x64 and scored by distinct 16-bit values: 63 of 128 tiles are rich (>16 distinct), and EVERY rich tile sits at x >= 512, the texture atlas. The framebuffer half (x < 512, where both display origins point) is flat. The reason is in gpu_native.cpp: the software rasterizer runs only under "if (sw_path()) raster_poly/raster_sprite(...)", while the prims are teed to the VK backend under "if (vk_path())". With VK enabled - which it is even headless, gpu_vk_enabled() returns 1 and only s_headless differs - the picture is rendered into the VK image and NEVER into s_vram.

CONSEQUENCE, and the trap: any capture that reads VRAM is STRUCTURALLY INCAPABLE of showing this port's frame. That includes PSXPORT_GPU_DUMP's s_vram path. It does not fail loudly; it returns a clean, plausible, uniformly-coloured image, which reads as "the game renders nothing" or "the game is stalled". Both are wrong. Two separate investigations this session (the spider1 pause-corruption repro, and spyro issue #45's "headless stall") were misled by exactly this.

THE CORRECT INSTRUMENT is the VK readback: gpu_native_shot takes the "if (vk_path())" branch and calls gpu_vk_shot_region over the display region. That is what Tomba's REPL shot uses, and why Tomba captures fine headless while these two do not.

STILL OPEN, do not assume: a VK-headless GPU_DUMP ALSO came back flat here, so the readback path is not yet confirmed working for this port. Before trusting any headless capture on spider1/spyro, run the positive control - capture a frame the game is definitely drawing and assert it has more than one colour. Do NOT conclude anything about rendering from a headless capture until that control passes.

CROSS-REF: spyro docs/issues #45 attributes a "headless stall" to the port. That conclusion is now suspect for the same reason and must be re-checked with the VK readback before being believed.
