---
id: 6
title: headless frame dumps cannot show this port's picture - the SW rasterizer is bypassed when VK is on
status: resolved
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

THE POSITIVE CONTROL HAS NOW RUN, AND IT PASSES (2026-08-05, spider1, psxport af28715a + the present-image-sink patch). PSXPORT_SHOT_AT - the VK readback, gpu_vk_shot -> dump_to - produces real frames headless on this port:

  f120  0.0% non-black,    1 colour   (genuinely nothing drawn yet)
  f200  11.4%,          1969 colours
  f300  19.7%,          3068 colours
  f600  23.3%,          5215 colours
  f1200 25.9%,          8825 colours  - legibly the Neversoft eyeball logo

So the instrument that was flat is PSXPORT_GPU_DUMP (the s_vram software-rasterizer path described above), NOT the VK readback. The f120 flat frame is the negative half of the same control: the tool says "1 colour" when there is nothing, and thousands when there is, on the same run. Headless SHOT_AT can be believed on spider1 - as a statement about GUEST VRAM (see instruments.md INST-18 for why that qualifier is load-bearing).

AND THE DEEPER GAP IS NOW CLOSED TOO. The reason no capture could speak about the PICTURE was one line in GpuVkState::present(): `if (s_headless) { SDL_SubmitGPUCommandBuffer(cmd); return; }` sat ABOVE show_composite, so headless did not merely present to a different sink - it never ran the composite stage at all. That violates the absolute "headless and windowed are ONE code path" rule, and it is the mechanism behind issue 0005. The present stage is now split into build_present_image (the picture, BOTH legs, driven by the pure present_plan.h) and show_present_image (the swapchain blit, windowed only), and PSXPORT_PRESENT_SHOT_AT reads the picture back in either leg. See instruments.md INST-20, including what it still cannot see.

REMAINING, and deliberately not claimed as fixed: nothing samples the SWAPCHAIN IMAGE itself. INST-20 reads the composite the blit consumes, so the acquire/blit/compositor hop is still uninstrumented.

CROSS-REF: spyro docs/issues #45 attributes a "headless stall" to the port. That conclusion is still suspect for the same reason and must be re-checked - now with PSXPORT_PRESENT_SHOT_AT, which is the instrument it wanted. Spyro's tree does not yet carry this patch (coord/patches/present-image-sink.diff).

### Resolution (2026-08-05)
CLOSED 2026-08-05 under the amended rule. The instrument gap this issue recorded is closed as far as
SDL_GPU permits, and the part that cannot be closed is now settled as IMPOSSIBLE rather than left
open as a to-do.

WHAT THIS ISSUE ASKED FOR: a headless capture that can show this port's picture, plus a positive
control before trusting any of them.

 1. POSITIVE CONTROL — PASSED, and it fires in both directions on the same run. PSXPORT_SHOT_AT
    (VK readback) headless: f120 0.0% / 1 colour (nothing drawn yet), f1200 25.9% / 8825 colours,
    legibly the Neversoft logo. The instrument that was flat is PSXPORT_GPU_DUMP's s_vram
    software-rasterizer path, NOT the VK readback — exactly as this issue's own analysis predicted.
 2. THE DEEPER CAUSE IS FIXED. GpuVkState::present() used to return at
    `if (s_headless) { submit; return; }` ABOVE the composite, so headless never ran the picture
    stage at all. It is now split into build_present_image (BOTH legs, driven by the pure
    present_plan.h) and show_present_image (the swapchain blit, windowed only), pinned by
    tests/test_present_plan.cpp — 9/9, 150 checks, with a compile-selectable negative control
    (-DPSXPORT_TEST_LEGACY_PRESENT_PLAN) that fails 8/9 against the old rule.
 3. THE NEW INSTRUMENT IS VALIDATED BY FORCED DISAGREEMENT, not by agreement:
    PSXPORT_PRESENT_SHOT_AT with a square 800x800 sink reads 20.2% while the VRAM shot stays pinned
    at 26.9% — 26.9 x 600/800 = 20.2, the letterbox a present-stage instrument MUST see and a
    VRAM-stage one CANNOT. Confirmed again on a natively rendered frame: menu at 512x240, VRAM 99.4%
    vs present 62.13%, and 99.4 x 0.625 = 62.1.

THE REMAINING HOP IS NOT A TO-DO, IT IS A WALL — recorded so nobody spends a session on it.
A true swapchain readback is IMPOSSIBLE under SDL_GPU: SDL_gpu.h:4306 declares the swapchain texture
write-only; SDL_gpu_vulkan.c:4699 creates it COLOR_ATTACHMENT|TRANSFER_DST with no TRANSFER_SRC and
no SAMPLED; no hint, property or device-create option adds them; and SDL_DownloadFromGPUTexture has
no usage assertion (SDL_gpu.c:2931), so calling it there returns no error and a plausibly ALL-ZERO
buffer — a black PPM, the precise false-negative shape this issue exists to record. SDL's own GPU
renderer solves this the same way we now do: render to an offscreen COLOR_TARGET|SAMPLER backbuffer
and read THAT (SDL_render_gpu.c:1353). s_present_img is SDL's own answer.

WHAT I DID NOT VERIFY: anything past the blit — a failed acquire, a blocking present mode, a
minimised window, the compositor. Covering that needs a SINK LEDGER (counting acquire failures and
presents that never reached the sink), not a picture. Not built. If a capture ever disagrees with
what you see on screen, that is where the difference lives — reopen and say so.

CROSS-REF now actionable: spyro issue #45's "headless stall" was diagnosed with the VRAM-stage
instrument this issue distrusts, and should be re-checked with PSXPORT_PRESENT_SHOT_AT. spyro's tree
does not yet carry the patch (coord/patches/present-image-sink.diff).
