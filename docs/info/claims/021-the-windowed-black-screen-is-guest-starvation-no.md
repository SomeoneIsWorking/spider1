---
id: C021
kind: claim
status: holds
created: 2026-08-05
tags: black-screen,windowed,headless,swapchain,vsync,gpu,starvation,re-07
depends: external/psxport/runtime/recomp/gpu_vk.cpp, external/psxport/runtime/recomp/gpu_native.cpp, game/core/sync_native.cpp
---

## Claim

The windowed black screen is GUEST STARVATION, not a render fault: the swapchain present mode was never set, so SDL's default VSYNC makes SDL_WaitAndAcquireGPUSwapchainTexture block the GUEST thread inside every present

## Evidence

MEASURED (static): gpu_vk.cpp:498 claims the window with SDL_ClaimWindowForGPUDevice and never calls SDL_SetGPUSwapchainParameters — that symbol appeared NOWHERE under external/psxport/runtime/ at the time of measurement. The acquire is gpu_vk.cpp:1007 inside GpuVkState::show_composite, and it runs on the GUEST thread: game/core/sync_native.cpp:202 vblank_advance() -> gpu_present(c), reached from every VSync(), every FUN_8005E748 field-wait iteration and every host turn (sync_native.cpp:287/297/326). There is no I/O thread — CD pump, MDEC, DMA completions and the guest share one thread. Headless never blocks: s_win is null and the acquire fails instantly at gpu_vk.cpp:1007-1009. MEASURED (a real stack, not inference): scratch/logs/windowed_run1.log shows show_composite -> SDL3 -> libvulkan_radeon -> drmSyncobjTimelineWait -> ioctl, entered via rec_irq_poll -> rec_host_turn. MEASURED (PSXPORT_DEBUG=presentskip, ~4100 presents each, one build): HEADLESS presents=4106 reuse_last=2165 rebuild_geom=1511 rebuild_vram=430 vram_writes=12812; WINDOWED presents=4027 reuse_last=4027 rebuild_geom=0 rebuild_vram=0 vram_writes=0. Windowed in 68 s never reached '[disc] opened', '[sync] VSyncCallback: guest registered' or '[gpu] display depth -> 24-BIT', all of which headless passes before present index 60. DECISIVE ONE-VARIABLE CONTROL, ZERO CODE CHANGED — windowed + MESA_VK_WSI_PRESENT_MODE=immediate: presents=4083 reuse_last=2933 rebuild_geom=654 rebuild_vram=496 vram_writes=11076, all three milestones reached, and the movie appears (f120 17.97% / 2197 colours, f200 50.76% / 6279, f300 36.35% / 6035). Logs scratch/logs/ps_head_full.log, win_upload.log, win_immediate.log, windowed_run1.log. INFERRED, and stated as inference: the causal chain 'no swapchain parameters -> SDL default VSYNC -> acquire blocks -> guest gets almost no CPU' is reasoning; what is MEASURED is the missing call, the blocking stack, the zeroed guest-progress counters, and that overriding the present mode from outside the process removes all of it. See issue 0005

## What would falsify it

if setting SDL_SetGPUSwapchainParameters to IMMEDIATE/MAILBOX in-process leaves a windowed run at vram_writes near 0 and the milestones unreached, the present mode is not the discriminator and the starvation has another source; equally, if a windowed run under the default VSYNC mode is ever observed with vram_writes > 0 and the milestones reached, blocking-on-vblank is not sufficient to explain the black screen. Note the scope: this explains the WINDOWED black screen at BOOT out to ~f2400 on one machine (Mesa/radv). It does not establish that VSYNC pacing is unusable once the guest is no longer CPU-starved, and it does not cover a non-Mesa driver
