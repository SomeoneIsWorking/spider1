# RE Frontier — the ordered RE dependency chain toward a faithful BL2

Tracked by `tools/re_frontier.py` (consult it FIRST; update it in the SAME commit
that changes a step). This is the fine-grained companion to `docs/codemap.md`:
the codemap says *what subsystem exists*, this says *which ordered RE step is
real reverse-engineering vs a hack that jumped ahead*.

**Hard rule (no hacks / no fallbacks):** a `⛔ hack` status is DEBT, never an
acceptable resting state. It marks a shortcut standing in for absent RE and MUST
be removed as its real mechanism lands. `re_frontier.py hacks` is the debt list;
`re_frontier.py next` tells you the next RE-ready step.

**`re-verified` MEANS FAITHFUL to the real target — not "the mechanism runs."** A
step is `re-verified` only when its OUTPUT matches the real game/binary (look /
sound / behavior) on real data. An internal trace ("bytecode reached the call
site", "N rows attached") is a mechanism check, NOT faithfulness — if it runs but
the result doesn't match the real target, it is `re-partial` with the
faithfulness gap named. The user observes the running system; that observation
overrides any internal trace.

**Fail fast & loud:** a failure must surface loudly, never silently fall back —
unless the fallback IS intended behavior of the real target being reproduced.

Statuses: ✅ re-verified · 🟡 re-partial (honest gap) · 🔬 in-progress ·
⛔ hack (debt, must remove) · ⬜ todo · ➖ skip-by-design · ⏸ blocked (computed).

<!-- Machine-edited by tools/re_frontier.py add/set. Format: `## <area>` sections;
     each entry is `### <id> — <title>` followed by `- <field>: <value>` lines. -->

## Frontier

### RE-00 — Provision + statically recompile the executable
- status: re-verified
- deps: 
- evidence: PS-X EXE header: entry 0x8008739C, load 0x80010000, text 0xB6800; 1561 fns from 335 seeds (game/recomp_seeds.json is empty by design)
- where: 
- gap: 
- notes: 

### RE-01 — crt0 / boot seam
- status: re-verified
- deps: RE-00
- evidence: crt0 0x8008739C decoded instruction-by-instruction: bssZero 0x800B5994..0x800C65D4, gp 0x800B47F4, libcInit 0x8008DC98, gameMain 0x8002C354; boots into the guest's own main
- where: 
- gap: 
- notes: 

### RE-02 — libetc VSync
- status: re-verified
- deps: RE-01
- evidence: wait helper 0x80084D58 emits the string at 0x80096020 = 'VSync: timeout'; 427,643 VSync(-1) vs 1 blocking VSync(0) over a 60s boot (CLAIM-02)
- where: 
- gap: 
- notes: 

### RE-03 — stock libcd — the whole CD stack is PC-native
- status: re-verified
- deps: RE-02
- evidence: retries 38->0, sector errors 38->0, CD_init 28->1; CD.HED LBA 390/12525B and CD.WAD LBA 397 resolve to real extents; boot reaches asset loading
- where: 
- gap: 
- notes: 

### HIST-03a — libcd CdInit — the earlier investigation
- status: skip-by-design
- deps: 
- evidence: 
- where: 
- gap: 
- notes: 

### HIST-03b — libcd CdInit — the investigation that got here
- status: skip-by-design
- deps: 
- evidence: 
- where: 
- gap: 
- notes: 

### RE-03b — SPU upload wait
- status: re-verified
- deps: RE-03
- evidence: SPU transfer-complete event delivered via DMA4 completion; the upload wait no longer spins
- where: 
- gap: 
- notes: 

### RE-03c — MDEC decode
- status: re-verified
- deps: RE-03
- evidence: pending-channel DMA pump (mem.cpp `mdec_dma_pump`); boot run 2026-07-30: 0 `mdec:error`
- where: 
- gap: 
- notes: 

### RE-03d — XA / CD streaming
- status: re-verified
- deps: RE-03
- evidence: ring slots reach status 2 and the stream runs; boot loads game assets through the stock libcd path
- where: 
- gap: 
- notes: 

### RE-04 — Movie / streaming playback
- status: re-partial
- deps: RE-03d
- evidence: movie descriptor table at 0x80097DEC (24-byte stride) read from the indexing routine 0x8002B0F4; entry 0 frameBytes 0x25800 = 320*240*2 is self-consistent
- where: 
- gap: HONEST GAP, named 2026-08-05: only the DESCRIPTOR TABLE is RE-verified. The 'evidence' line above establishes the movie descriptor table at 0x80097DEC (24-byte stride) and that entry 0's frameBytes 0x25800 = 320*240*2 is self-consistent — that is a static read of a table, and it is sound. It says NOTHING about faithfulness of PLAYBACK, which is what this step is titled for. Per this file's own header, re-verified MEANS FAITHFUL to the real target, not 'the mechanism runs'; the step was marked re-verified on evidence that never addressed the output. Specifically NOT verified: (a) frame PACING against the movies' real duration — no run has ever been compared against it; (b) XA/ADPCM AUDIO — every run to date used PSXPORT_NOAUDIO=1, and audio is additionally gated on !gpu_windowed() at external/psxport/runtime/recomp/spu_audio.cpp:94, so a HEADLESS run cannot measure it even in principle; (c) that playback reaches the DISPLAY — see RE-07 and issue 0005. What IS measured: delivery is byte-correct (C016) and the decode path produces both logos in guest VRAM headless (C020). To reach re-verified this step needs a pacing measurement against the STR's own duration and an audio measurement in a WINDOWED run
- notes: 

### RE-05 — Input buffers + the host-turn seam
- status: re-verified
- deps: RE-03
- evidence: pad buffers confirmed 3 ways (PadInitDirect args 0x22 apart at 0x8006AE34; consumer 0x8006B27C walks stride 0x22; libpad itself inits that range at runtime). Host turn: title wait terminates, pad writes 4 -> 1660
- where: 
- gap: 
- notes: 

### RE-12 — Per-frame OT / packet-pool layout
- status: re-verified
- deps: RE-03,RE-19
- evidence: RE-VERIFIED 2026-08-06, static RE plus a runtime read. THE LAYOUT IS THE STANDARD SONY DOUBLE-BUFFER 'DB' PAIR, at two fixed addresses: ctx[0] = 0x8009A6E4, ctx[1] = 0x8009A75C (stride 0x78). Fields, each confirmed by the size of the libgpu copy that consumes it: +0x00 DRAWENV (PutDrawEnv memcpys 0x5C bytes), +0x5C DISPENV (PutDispEnv memcpys 0x14 bytes), +0x70 u_long* ot, +0x74 void* primitive pool. FUN_80061140 is the graphics init and states the geometry: SetDefDrawEnv(&ctx0, 0, 0, 0x200, 0xF0) / SetDefDrawEnv(&ctx1, 0, 0x100, 0x200, 0xF0) / SetDefDispEnv(&ctx0.disp, 0, 0x100, 0x200, 0xF0) / SetDefDispEnv(&ctx1.disp, 0, 0, 0x200, 0xF0) — i.e. a 512x240 framebuffer, two pages at VRAM y=0 and y=256, isbg=1 with a black background. That 512x240 is INDEPENDENTLY confirmed at runtime by the port's own GPU log line '[gpu] display depth -> 15-bit (GP1(08)=08000002, 512x240)', and it is the same 512 that makes RE-17's measured OFX=256 the framebuffer centre. FUN_80061230 ALLOCATES all four blocks from the engine heap: ot = alloc(0x4000) x2, pool = alloc(0x17000) x2 — 0x4000 bytes = 4096 OT entries, which is exactly the ClearOTagR(ot, 0x1000) in beginFrame and the DrawOTag(ot + 0x3FFC) head in submitFrame. MEASURED AT RUNTIME with PSXPORT_WWATCH=8009A754,8009A7D4 over a 45 s headless boot (scratch/re12/logs/wwatch_ot.log, 1551 hits in range): the four pointer slots are written EXACTLY TWICE each and never again — first 0xFFFFFFFF by FUN_80061124 (the 'unallocated' marker), then ctx0.ot=0x800C65EC, ctx1.ot=0x800CA5F4, ctx0.pool=0x800CE5FC, ctx1.pool=0x800E5604, all by the allocator at pc=0x80064FA0 with ra = 0x80061268/0x80061280/0x80061298/0x800612A0 — the four call sites decompiled in FUN_80061230, in order. The packet pool is used from BOTH ENDS: gp+0xCBC (0x800B54B0) grows up from ctx->pool and gp+0x7F4 (0x800B4FE8) is seeded to ctx->pool + 0x16F00 by FUN_8002BB9C and grows down; FUN_8002BD5C's tail records the frame's usage as gp+0x7C8 = cursor - pool.
- where: guest: DB pair 0x8009A6E4 / 0x8009A75C (stride 0x78; ot at +0x70, pool at +0x74); DB* current 0x800B54A8; pool cursors 0x800B54B0 (up) and 0x800B4FE8 (down); gfx init FUN_80061140; allocation FUN_80061230; engine allocator FUN_800651C8 (store pc 0x80064FA0). Runtime values observed this boot: ot 0x800C65EC / 0x800CA5F4, pool 0x800CE5FC / 0x800E5604.
- gap: THE FRAMEWORK'S GameConfig FIELDS CANNOT EXPRESS THIS, and they stay ZERO deliberately. otRegionBase / otRegionStride / packetPoolBase / packetPoolStride are LITERAL guest addresses that native_step_frame uses directly as 'envp = otRegionBase + parity*otRegionStride'. Spider-Man HEAP-ALLOCATES both OTs and both pools at boot (FUN_80061230), so there is no literal to write: the only stable addresses are the four POINTER SLOTS (0x8009A754/0x8009A7CC/0x8009A758/0x8009A7D0) and the current-context pointer 0x800B54A8. Writing this boot's observed values (0x800C65EC etc.) into GameConfig would be a magic constant that a different allocation order silently invalidates — and 0x800C65EC is literally the heap's first block, the same address game_config.cpp's overlaySlots[0] still names. Feeding the framework's native frame loop therefore needs a POINTER-INDIRECTION shape (read the DB* then its +0x70/+0x74), which is a FRAMEWORK CHANGE and is NOT this step. The values above are also from ONE boot and ONE allocation order; they are cited as evidence that the pointers are written once and never move, not as constants to bake.
- notes: SUPERSEDES the previous gap text, which described the port not owning the frame loop rather than the OT layout this step is titled for. Frame-loop ownership is now tracked as RE-22, and the reachable render seam as RE-20. NOT a blocker for a native renderer any more: RE-19 measured that the game's own submitFrame FUN_80061308 runs 1761 times per 100 s headless run and is override-able game-side, so GameHooks::drawOTag's unreachability (C025, issue 0011) no longer gates RE-18.

### RE-13 — Scheduler task layout
- status: re-partial
- deps: RE-03
- evidence: RE'd 2026-08-13 from Spider-Man's own image with `tools/ghidra_query.py func 0x8002C354` and `xrefs 0x800B4F34`. The previous candidate, gp+0x740 = 0x800B4F34, is a MAIN-MODE SELECTOR, not a scheduler current-task pointer: boot init FUN_8006BF9C clears it; main FUN_8002C354 calls a mode loop, reads it on return, then jump-table-dispatches values 1..10. Ghidra found 18 references total — 11 direct writes and 7 reads — with no calls. Concrete transition writes include FUN_8006EE28 choosing 7, 8 or 10 from a menu choice (the actual store is 0x8006EFE8), and FUN_80049ED0 writing 7 when its sequence queue drains (0x8004AAD0). Exact instruction spot-checks came from `external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 8006EFC8 8006F000` and `... 8004AAA0 8004AAE8`. This establishes the outer lifecycle model without pretending it establishes an SDK-style task system.
- where: guest outer dispatcher FUN_8002C354; mode selector gp+0x740 = 0x800B4F34; reset writer FUN_8006BF9C (store 0x8006C3B8); transition writers FUN_8006EE28 and FUN_80049ED0. The mode jump table is at 0x80093C3C.
- gap: No task-slot table, task objects, current-task pointer, or WITHIN-LEVEL substate selector has been identified. The remaining question is whether the engine has a second, local state-machine layer for front-end page / cutscene / play; do not retrofit Tomba!2's cooperative scheduler shape onto this outer mode dispatcher. What HAS changed 2026-08-06: this step is no longer the thing blocking a native-renderer scene classifier. RE-23 found the game's own scene identity (the level-name string at 0x800A568C and its encoder FUN_8005A734, consumed by FUN_80062CE0's per-frame switch over 0x201..0x803), which is the datum RE-18 actually needed. The 2026-08-06 module-registry result remains true but is the wrong source to generalise from.
- notes: The re-verified frame loop in RE-19 is straight-line fixed-phase code, consistent with this result. RE-18 waits on RE-20 (the reachable seam) and RE-21 (producers), not RE-13.

### RE-06 — Pad driver
- status: re-verified
- deps: RE-05
- evidence: forced DOWN (PSXPORT_FORCE_BUTTONS=0040) moves the menu selection and the game transitions to the memory-card screen; 24.18% of pixels differ from the baseline frame at the same present index
- where: 
- gap: 
- notes: 

### RE-09 — Runtime-loaded code (CD.WAD)
- status: re-verified
- deps: RE-05
- evidence: Extraction + relocation re-verified byte-exact over 112912 bytes (CLAIM-08). The one-slot residency premise was FALSIFIED (CLAIM-09) and has been REPLACED, not patched: the modules are emitted base-relative and the guest allocator places them, exactly as the console does (HACK-02, resolved 2026-08-04). Verified on a real boot — 7176 frames, 0 recomp-MISS, three modules co-resident at three distinct bases.
- where: tools/extract_modules.py, game/core/module_loader.cpp, external/psxport/tools/recomp/emit.py, external/psxport/runtime/recomp/overlay_router.cpp
- gap: 
- notes: tools/check_reloc_model.py checks the relocation-shape assumptions the emission model rests on across all 30 modules (self-tested; --selftest).

### RE-10 — Tight guest spin loops starve the host turn
- status: re-verified
- deps: RE-05, RE-11
- evidence: field-wait 0x8005E748 owned natively; with the back-edge gate the main() spin completes and boot reaches a third module load
- where: 
- gap: 
- notes: 

### RE-11 — Branch-and-link was mistranslated
- status: re-verified
- deps: RE-00
- evidence: 0x8007C7CC is bltzal $t0,0x8007D160; 0x8007D160 ends in jr $ra; substrate contained 16 'goto L_8007D160' whose body ended in 'return;'. After the fix the third module load carries the correct name SHELL@0x800B4FD0
- where: 
- gap: 
- notes: 

### RE-14 — BIOS libc string leaves
- status: re-verified
- deps: RE-01
- evidence: 0x80097D10 holds 0x1C001D00 in the load image (never written); font entry 0's name field at 0x80097BBC is all-zero .bss; 0x80084E20 is the A(17h) stub. After implementing 0x17/0x18/0x19/0x1B: 0 FATAL, 0 UNIMPL, module rotation reaches COP, ~1560 frames in 40s
- where: 
- gap: 
- notes: 

### RE-15 — Memory card
- status: re-verified
- deps: RE-06
- evidence: card image created and formatted (scratch/saves/spiderman.mcr, 128 KB); the card check COMPLETES and the game advances through new-game into its name-entry screen
- where: 
- gap: 
- notes: 

### RE-16 — Unconditional intra-function branch emitted as call+return leaks stack
- status: re-verified
- deps: RE-15
- evidence: RESOLVED 2026-08-05. 0x8002A338 is libpress DecDCTvlc, hand-written asm using jal/jr-$ra as an INTERNAL coroutine in ONE stack frame. `jal 0x8002A478` (at 0x8002A41C) promoted that internal block to a function ENTRY, so the routine's own loop back-edge — the unconditional `bgez $zero, 0x8002A478` at 0x8002A43C — was emitted as `func_8002A478(c); return;`. It ran one block and unwound, epilogue unrun. MEASURED with a fntrace ABI check validated BOTH ways (PSXPORT_FNTRACE_SELFTEST=1 makes the control leaves 0x8008735C/0x8008710C report a violation; without it they report 0 over 4 and 49 calls): BEFORE 0x8002A338 = 4 calls / 8 violations (sp 807FFF00->807FFEFC, ra->8002A424) and 0x8002B430 = 44 calls / 12 violations (pure downstream damage from the leaked 4 bytes — its epilogue reloaded ra/s0 from the wrong stack words); AFTER 423 calls / 0 and 4653 / 0, DecDCTin 2 -> 421. THE FIX WAS ALREADY WRITTEN AND NEVER WIRED: demote_internal_labels (psxport 77386b68) was defined, unit-tested and never called from emit_module. Wiring it needs two companions, both added: an intra-function `jal` emits `$ra = <ret>; goto L_<target>;` (its target is no longer a function), and the matching `jr $ra` becomes a SWITCH over this body's own link constants — which is what makes the reverted d2d99ff7 safe to restore, since its router dispatch (the recomp-MISS on 0x03FF03FF that caused the revert at 88f58d7f) is gone. On the real executable: MAIN demotes exactly {0x8002A478} and emits exactly one computed jr $ra (0x8002A460, cases 0x8002A424/0x8002A708/0x8002A774). Hermetic gate: tools/recomp/test_emit.py::test_exec_coroutine_internal_label_runs_its_loop_and_its_epilogue, which runs the WHOLE pipeline through emit_module (an emit_func-level test cannot see a wiring bug) — shown RED "the coroutine's loop ran 2 of 3 passes (v0=20)". 33/33 test_emit, 8/8 test_decode, 8/8 framework ctest.
- where: external/psxport/tools/recomp/emit.py (emit_module wiring, emit_func intra_links/ra_conts, emit_control jal + computed jr $ra, RECOMP_VERSION 2026-08-05.1); guest: FUN_8002A338 = DecDCTvlc, internal block 0x8002A478, internal bal sites 0x8002A41C/0x8002A700/0x8002A76C
- gap: 
- notes: The 88f58d7f revert's own precondition ("land the pending-channel DMA fix first") was already met — RE-03c is re-verified. 12 overlay modules demote 1-5 internal labels each by the same criterion; the boot is clean with them (0 recomp-MISS over a 180 s run).

### RE-07 — Intro FMV / front-end
- status: re-partial
- deps: RE-04
- evidence: DELIVERY is byte-correct and re-verified. CdlSetloc 28:32:54 -> LBA 128304 == CINEMAS/ATVILOGO.STR and 62:15:25 -> LBA 280000 == CINEMAS/LOGO.STR (discdump list); MDEC DMA0 word counts 1824 and 1440 equal those files' BS-header word counts read off the disc. THE RING DEADLOCK IS FIXED (2026-08-05, claim C017). Root cause was NOT what the first write-up said: DPCR (0x1F8010F0) and DICR (0x1F8010F4) were unmodelled in the framework, so the runtime signalled the guest's DMA-completion callback 0x8008DB44 on EVERY DMA3 transfer. libcd DMACallback (0x8009152C) arms DICR bit 16+ch plus the master bit 23; libstr's DMA starter FUN_80085948 read-modify-writes the BYTE at DICR+2 per transfer; FUN_80085000 arms it for the LAST chunk of an STR frame only. So the frame-ready marker was written once per SECTOR, slots 1..7 carried a spurious status 2, and the producer refused to overwrite slot 7. A/B on ONE binary toggling only the gate: BEFORE the ring wedges at frameStart=7 cons=9 writeIdx=7 status "0 2 2 2 2 2 2 2 0..." over 42 decimated samples and intro presents are 0.00% non-black / 1 colour; AFTER the ring never wedges (1 sample, writeIdx=1 status "3 0 0..."), dmairq shows 9 chunks armed=0 then the 10th armed=1, TWO full STR frames arrive per movie, and intro presents are 40.62% / 140 colours. Menu unaffected: f2400 99.44% / 1120 colours, f4000 83.70% / 1315, 0 abort, 0 recomp-MISS. The ring has 48 slots (DAT_800C1520 read at runtime) — the earlier "cons=9 is past the end" was a bad inference and C016 is corrected. Logs: scratch/logs/re07_before.log, re07_after.log, re07_gate.log. Hermetic gate: external/psxport/tests/test_dma_irq_gate.cpp, 7 cases / 30 checks, shown RED 0/7.
- where: external/psxport/runtime/recomp/dma_irq.h (NEW, DPCR/DICR rules), mem.cpp (register state + the DMA3 completion gate), hle.cpp (flag acknowledge at the dispatch that stands in for the BIOS DMA handler); game/core/cd_stream.cpp (ring diagnostic only — its pump was NOT the fault); guest: StGetNext=FUN_80086B10, StFreeRing=FUN_800872AC, producer=FUN_80085000, DMA starter=FUN_80085948, frame-ready cb=FUN_8008DB44, DMACallback=0x8009152C; ring state DAT_800C1510 base / 0x800C1514 writeIdx / 0x800C1518 frameStart / 0x800C151C cons / 0x800C1520 slot count (=48). PER-CHANNEL DMA COMPLETIONS (was a separate 'where-2:' line — MERGED HERE 2026-08-05 because re_frontier.py's FIELDS list is status/area/deps/evidence/where/gap/notes and a 'set' on this step SILENTLY DELETED the extra field; do not re-add non-schema fields): external/psxport/runtime/recomp/dma_irq.h (DmaDone per-channel pending set + dma_callback_slot), mem.cpp (all six completion points), hle.cpp (irqPoll scans 7 channels; a completion is consumed only when delivered), game_iface.h (cdDmaDoneCbPtr -> dmaCallbackTable), game/core/game_config.cpp (0x800B4388); guest: FMV player init FUN_8002B1FC, MDEC-out callback 0x8002B28C, DecDCTout 0x80085BA0 -> FUN_80085D64, BIOS DMACallback thunk 0x8008B89C. WINDOWED BLACK SCREEN (issue 0005, C021) is owned by external/psxport/runtime/recomp/gpu_vk.cpp:498 (missing SDL_SetGPUSwapchainParameters) and :1007 (the blocking acquire), reached from game/core/sync_native.cpp:202
- gap: DOWNGRADED 2026-08-05 from re-verified. The previous gap line said RESOLVED and 'BOTH intro logo movies play'. That is TRUE HEADLESS and FALSE WINDOWED, and the USER runs a window. All of its numbers (f120 99.95% non-black / 11395 colours, f300 25.70% / 8773, menu f4000 99.44%) were taken under PSXPORT_VK_HEADLESS=1 and are re-issued, scoped, as C020; the headless evidence is not withdrawn and the three fixes behind it (DICR gate C017, RE-16 DecDCTvlc, per-channel DMA completions) are real. WHAT IS STILL BROKEN: windowed, the same build is 0.00% non-black / 1 colour at every present index out to f2400, and PSXPORT_DEBUG=presentskip reports presents=4027 reuse_last=4027 rebuild_geom=0 rebuild_vram=0 vram_writes=0 — the guest wrote NOTHING to VRAM over 4027 presents (headless, same build: 4106 / 2165 / 1511 / 430 / 12812). Windowed in 68 s never reached '[disc] opened', '[sync] VSyncCallback: guest registered' or '[gpu] display depth -> 24-BIT', all of which headless passes before present index 60. ROOT CAUSE (C021): gpu_vk.cpp:498 claims the window with SDL_ClaimWindowForGPUDevice and never calls SDL_SetGPUSwapchainParameters, so the swapchain keeps SDL's default present mode VSYNC, and SDL_WaitAndAcquireGPUSwapchainTexture (gpu_vk.cpp:1007, in GpuVkState::show_composite) blocks the CALLING thread until vblank. That caller is the GUEST thread — game/core/sync_native.cpp:202 vblank_advance() -> gpu_present(c), reached from every VSync(), every FUN_8005E748 field wait and every host turn (sync_native.cpp:287/297/326) — and there is no I/O thread, so the CD pump, MDEC and DMA completions starve with it. Headless never blocks because s_win is null and the acquire fails instantly (gpu_vk.cpp:1007-1009). Blocking backtrace captured: show_composite -> SDL3 -> libvulkan_radeon -> drmSyncobjTimelineWait -> ioctl, via rec_irq_poll -> rec_host_turn (scratch/logs/windowed_run1.log). DECISIVE CONTROL, ZERO CODE CHANGED: windowed + MESA_VK_WSI_PRESENT_MODE=immediate gives presents=4083 reuse_last=2933 rebuild_geom=654 rebuild_vram=496 vram_writes=11076, all three milestones reached, and the movie appears (f120 17.97% / 2197, f200 50.76% / 6279, f300 36.35% / 6035). NO HACK WAS ADDED and the hack list stays empty — this is a missing framework call, and the fix is in flight in psxport. RESOLVED 2026-08-05 and issue 0005 CLOSED. The framework call landed (gpu-vk-swapchain-pacing: SDL_SetGPUSwapchainParameters asking MAILBOX/IMMEDIATE, with a RED-first hermetic test). Re-measured windowed on one build: "swapchain present mode: MAILBOX", all three milestones reached, presentskip presents=4100 reuse_last=3194 rebuild_geom=447 rebuild_vram=459 vram_writes=11114 (was 4027/4027/0/0/0), present shot f1200 31.55% non-black / 9314 colours showing the Neversoft logo. The same instrument in the same windowed mode produced the failing answer before the fix, so it could have shown it again. The old "the USER must confirm" gate is superseded by the USER's amendment of 2026-08-05 ("you can close bugs you think are solved, I will reopen if not solved") — see external/psxport/docs/workspace/PROTOCOL.md. STILL NOT VERIFIED, and the reason this stays re-partial rather than re-verified: nothing samples the swapchain image (impossible under SDL_GPU — INST-20), so a failed acquire/blit/compositor is invisible to us; and the FMV output has never been compared frame-for-frame against the real PSX, which is what this file's re-verified bar actually demands. Logs: scratch/logs/ps_head_full.log, win_upload.log, win_immediate.log, windowed_run1.log. Full write-up: issue 0005. Instruments that certified the false negative: INST-18 (PSXPORT_SHOT_AT reads guest VRAM, never the swapchain) and INST-19 (the watchdog is petted from gpu_present_ex, so it cannot see guest starvation)
- notes: GameConfig::bootFmv is empty and PSXPORT_NO_FMV is a no-op here; playback is entirely the guest path. C036 accounts statically for all five calls to the single STR player and now adds bounded runtime evidence: Start and Cross each produced a post-30 edge for boot ID0 and reached the common full teardown with active state cleared. Cross reached boot ID1 but did not create its edge. Queued and dispatcher calls were not reached, so those paths have no runtime verdict. Boot's held-Start test suppressing ID1 was observed. No alternate player or behavior patch was introduced. This establishes neither ID1/queued responsiveness nor non-STR in-engine sequence skipping; the within-level state owner remains the RE-13 gap. Instruments remain PSXPORT_DEBUG=dmairq (I010), corrected PSXPORT_DEBUG=ring (I011), and wired PSXPORT_FNTRACE.

**2026-08-14 runtime skip discriminator:** `PSXPORT_STR_SKIP_ORACLE=start|cross|held` drives only the framework pad seam and observes the shipping `FUN_8002AA0C` body at qualified PCs. Start on boot ID0 produced a guest edge after the 30-tick guard and reached the common teardown with active state cleared (1/1); the resulting held byte made the retail boot owner suppress ID1. Cross reached both boot IDs (2/2), with an edge-driven teardown on ID0 (1/2); ID1 edge remained absent (0/1). The checkpoint at `0x8002AEF8` is reported honestly as `guard_arm`: it is reached before the two edge-clearing stores, not after them. Queued movies reported **MISSING CORPUS: 0 invocations; no verdict**. This advances C036 only for the one measured ID0 paths; it does not certify queued movies, ID1 responsiveness, menu arrival, or in-engine sequences. Fresh current-source logs: `scratch/logs/str_skip_start_current.log`, `scratch/logs/str_skip_cross_current.log`. `PSXPORT_SELFTEST=strskip` exercises the same event classifier as shipping observation across tick 29/30, early suppression, late exit, held non-retrigger, Start/Cross mode selection, owner/context validation, and missing-queued-corpus semantics.

### RE-08 — Render: native per-vertex depth from the guest's own swc2/mfc2 store (store hook, NOT a GTE tap)
- status: todo
- deps: RE-09
- evidence: 
- where: 
- gap: 
- notes: 

### RE-17 — Native camera projection constants (libgte SetGeomOffset / SetGeomScreen)
- status: re-verified
- deps: 
- evidence: MEASURED 2026-08-06. The two libgte leaves were located by the ONLY thing that marks them, the instruction they execute: 'python3 tools/ghidra_query.py scan ctc2' (new mode) walked 130,588 disassembled instructions across 1,564 defined functions, found 1,404 ctc2 sites, of which exactly 10 target cop2 control regs 24/25/26. SetGeomScreen = 0x8008BF14 (ctc2 $a0,$26 ; jr $ra ; nop). SetGeomOffset = 0x8008BF24 (sll $a0,$a0,0x10 ; sll $a1,$a1,0x10 ; ctc2 $a0,$24 ; ctc2 $a1,$25 ; jr $ra ; nop). 0x8008BE5C is InitGeom (seeds ZSF3=0x155 ZSF4=0x100 H=0x3E8 DQA=-0x1062 DQB=0x1400000, zeroes OFX/OFY) and is NOT registered. COVERAGE NEGATIVE CONTROL: Ghidra had disassembled only 24.9% of the 2MB image, so a raw word scan of ALL 524,288 words of ram.bin for every ctc2 rX,CR24/25/26 encoding and for jal 0x8008BF14 / 0x8008BF24 / 0x8008BE5C returned the SAME 10 sites and the SAME single caller each - 0 sites hidden by the undisassembled 75%. RUNTIME GATE, on the real boot with the real disc: PSXPORT_DEBUG=geomwatch prints 'GATE PASSED: requireGeom() returned OFX=256 OFY=120 H=276 without aborting' (scratch/logs/g7/run_gate.log). NEGATIVE CONTROL, same binary path, same instrument, only the two GameConfig addresses zeroed: the same probe prints 'valid=0 OFX=0 OFY=0 H=0' and requireGeom ABORTS with 'SetGeomOffset (OFX/OFY) NEVER RAN - SetGeomScreen (H) NEVER RAN' (scratch/logs/g7/run_gate_control.log, run_control.log); plat-hle installs 8 primitives with the fix vs 6 without. BOOT-PROGRESS GATE: 70 s headless, with fix 4187 presents / reuse_last 2842 / rebuild_geom 917 / rebuild_vram 428 / vram_writes 12554; without 4189 / 2843 / 918 / 428 / 12554. The ONLY differing line in the entire non-presentskip log stream across both runs is the plat-hle count (8 vs 6) - zero behavioural change.
- where: game/core/game_config.cpp hle.setGeomOffset=0x8008BF24 / hle.setGeomScreen=0x8008BF14; the recording is framework (external/psxport/runtime/recomp/proj_params.cpp libgte_set_geom_offset/_screen, registered by sync_overrides.cpp:213-214). Probe: game/core/diag_overrides.cpp diag_geom_setup on 0x80075D0C (PSXPORT_DEBUG=geomwatch). Guest: FUN_80075D0C is the SOLE caller (jal at 0x80076180 SetGeomScreen, 0x80076190 SetGeomOffset); it computes vp[7]=H, vp[8]=OFX, vp[9]=OFY into a u16 viewport descriptor whose pointer it also stores at gp+0x1124 = 0x800B5918. FUN_8007C2AC zeroes CR24/25 for its inner loop (0x8007C0B4/B8) and restores them (0x8007C268/6C) from *(*0x800B5918+0x10), i.e. the same vp[8]/vp[9] - so no second recording path is needed.
- gap: H is only observed for the boot/front-end viewport reached in a 70 s headless run (OFX=256 OFY=120 H=276, constant over all sampled calls). Gameplay/cutscene viewports are NOT observed, and since the game DERIVES H per view (vp[7] = ((((vp[0]-vp[2])>>1)<<12)/vp[6]<<12)/*(int*)(gp+0x1140)) the value WILL differ elsewhere. That is why nothing is baked into GameConfig; the recording is per-call and stays correct, but no one has yet watched a gameplay viewport switch. Not compared against real PSX hardware.
- notes: Tomba!2's pattern TRANSFERS as a mechanism but NOT as data: Tomba!2 passes literal constants (OFX 160 / OFY 120 / H 350) and its config comment records them; Spider-Man passes computed viewport fields and has no constants to record. OFX=256 is 512/2, the centre of this game's 512-wide framebuffer - a further datum against this workspace's hardcoded-320 assumptions. Also repaired a latent defect found while editing: the hle POSITIONAL initialiser had the entry commented /* vsyncTrap */ sitting in the setGeomOffset SLOT, so setting vsyncTrap would have installed the VSync abort at the GTE setter. Harmless only because all three fields were 0.

### RE-18 — Native renderer (pc_render): scene classifier + fail-fast drawOTag skeleton — Tomba!2's pattern
- status: re-partial
- deps: RE-20,RE-21,RE-23
- evidence: THE SKELETON FAILS FAST AND THE FIRST PRODUCER IS BEHIND IT, 2026-08-06. MEASURED: PSXPORT_RENDER_PSX=0, headless, PSXPORT_NOPACE=1 -> scene '....' (the boot-init submit) is now RENDERED NATIVELY by the frame-envelope producer, and the run aborts at submitFrame CALL #2 / frame 2252 naming scene 'dem1'. So the abort has MOVED — the dependency order advanced by exactly one scene, which is what this step's crash-list-as-backlog design predicts. The abort prints the scene name and code, the encoder's address and consumer, the frame and call number, the full DB lens, the projection state, the envelope's own produced=/clears= counters, and dumps the whole 1024x512 CPU VRAM to scratch/raw/native_abort_vram.ppm. The '....' case is NOT a hardcoded pass: renderScene() re-walks the OT with the RE-21 census and aborts if that frame ever carries a pixel-writing primitive (measured pixelWriters=0 there, 59 on the very next call). There is no OT-walk fallback and no env escape hatch on that leg.
- where: game/render/render_seam.cpp — RenderSeam::renderScene() is the ONE native-renderer dispatch and RenderSeam::abortUnimplemented() is the fail-fast; game/render/scene_id.cpp is the classifier's data source.
- gap: WHAT IS MISSING IS THE DISPLAY-LIST PRODUCERS, i.e. RE-21 — renderScene() has exactly one case ('....', envelope-only) because there is nothing else to dispatch TO. Three honest gaps. (1) The abort still names the SCENE and not the unhandled DISPLAY-OBJECT TYPE, because no display-object type is decoded (RE-21). (2) The producer that HAS landed cannot be gated on pixels in its own window: A/B against a producer-disabled build measured 0 of 524288 pixels differing, because at the abort both framebuffer pages are entirely black (claim C034 records the measurement and the exact condition that will make the gate meaningful — the first 'dem1' producer). Its correctness rests instead on a word-exactness gate against libgpu, 2560 checks / 0 mismatches with a perturbation control (C033). (3) The DEFAULT leg has NOT flipped, and the condition is not 'a producer exists' but 'every scene a boot passes through has one'.
- notes: The mechanism transfers, the identity data does not — the same shape RE-17 found for the projection constants, and the same shape found again here for the frame seam. Do NOT implement GameHooks::drawOTag for this game; it stays a fail-fast stub. The seam is the guest's own submitFrame.

### RE-19 — The guest's frame driver and the engine's render seam (beginFrame / submitFrame)
- status: re-verified
- deps: RE-00
- evidence: RE'd 2026-08-06 with Ghidra headless (tools/ghidra_query.py func/xrefs), never by hand-walking disassembly. FRAME DRIVER: FUN_8002C174, the SOLE callee of main()'s mode switch (xrefs 0x8002C174 -> 1 caller, FUN_8002C354 @0x8002C420). Its body is a per-frame loop 'while (*(gp+0x740) == 0)' whose iteration is: FUN_8002BBCC (empty stub) ; vbl0 = 0x800B5468 ; FUN_800612B8 (BEGIN FRAME) ; FUN_8002BB9C (second pool cursor = ctx->pool + 0x16F00) ; FUN_8002BBD4 (game logic: pad read FUN_8006B514 + actor updates) ; FUN_80062CE0 (per-frame audio state machine) ; FUN_8002BD5C (THE RENDER WALK) ; FUN_8002B184 (empty stub) ; FUN_8007F930(ot, 0x1000) (in-place OT relink pass) ; if (0x800B5468 == vbl0) FUN_8005E748(1) — guarantees at least one field per frame ; a DrawSync(1) spin ; FUN_80061308 (SUBMIT) ; FUN_80059664 exit test. THE TWO SEAM FUNCTIONS, both GAME addresses in [recMainLo, recMainHi): FUN_800612B8 = beginFrame — flips the double-buffer context gp+0xCB4 (0x800B54A8) between 0x8009A6E4 and 0x8009A75C, calls ClearOTagR(ctx->ot, 0x1000), sets the pool cursor gp+0xCBC (0x800B54B0) = ctx->pool & 0x7FFFFFFF. FUN_80061308 = submitFrame — ResetGraph(1) ; PutDispEnv(ctx+0x5C) ; PutDrawEnv(ctx+0x00) ; DrawOTag(ctx->ot + 0x3FFC). THE libgpu LEAVES ARE GROUND TRUTH, identified by the diagnostic strings the library itself emits at verbosity DAT_800B0E2A>1, not by inference: 0x80081ED0 DrawOTag ('DrawOTag(%08x)' @0x80095EA0), 0x80081DC8 ClearOTagR ('ClearOTagR(%08x,%d)' @0x80095E88), 0x80082000 PutDispEnv (@0x80095EE8), 0x80081F40 PutDrawEnv (@0x80095EB4), 0x800819A4 DrawSync (@0x80095DFC), 0x8008173C ResetGraph (@0x80095D78); 0x80081E74 = DrawPrim (driver slot +0x14, no string). The libgpu driver vtable is *0x800B0E20 (DrawOTag=+8, PutDispEnv/GP1=+0x10, DrawPrim=+0x14, ClearOTagR=+0x2C, ResetGraph=+0x34, DrawSync=+0x3C). DrawOTag HAS EXACTLY 2 CALLERS in the whole image (xrefs 0x80081ED0): FUN_80061308 and library-internal FUN_8008A614 — so FUN_80061308 is the ONE game-side OT submit. RUNTIME MEASUREMENT, 100 s headless, no input, PSXPORT_FNTRACE on 8 addresses (scratch/re12/logs/fntrace1.log): 0x80061308 = 1761 calls (first at frame 2, ra=80061218), 0x800612B8 = 1765, 0x80061140 = 1, 0x80061230 = 1, 0x8002C174 = 2 (first at frame 2261), 0x800604CC = 1 (frame 3714), 0x800160EC and 0x8006F294 NEVER CALLED. ABI violations 0 on every site. fntrace installs a real override at each address and the body still runs, so those 1761 hits ARE the proof that an override at 0x80061308 executes.
- where: guest: frame driver FUN_8002C174; beginFrame FUN_800612B8; submitFrame FUN_80061308; render walk FUN_8002BD5C; gfx init FUN_80061140; OT/pool alloc FUN_80061230; mode switch main FUN_8002C354. Guest state: DB* current = 0x800B54A8 (gp+0xCB4), pool cursor 0x800B54B0 (gp+0xCBC), second pool cursor 0x800B4FE8 (gp+0x7F4), mode-exit code 0x800B4F34 (gp+0x740), in-mode frame counter 0x800B4F38 (gp+0x744).
- gap: The 100 s headless run reached the engine frame loop only at frame 2261 and produced 1761 engine frames with NO pad input, so this covers boot + attract only. FUN_800160EC and FUN_8006F294 — two further mode loops that also call the beginFrame/submitFrame pair — were never entered, so their per-frame shape is INFERRED FROM THE CALL GRAPH, not observed. Nothing here has been compared against real PSX hardware.
- notes: The FMV/boot phase does NOT use this pair: over frames 2..2261 the port presented ~2261 frames while beginFrame/submitFrame were entered ~4 times (init only). So a producer hung on 0x80061308 covers ENGINE-rendered frames (front-end 3D, gameplay) and NOT intro FMV — which is the correct scope.

### RE-20 — Native render seam: a game-side override on the engine's submitFrame (0x80061308)
- status: re-verified
- deps: RE-19, RE-17
- evidence: DONE 2026-08-06. INSTALL PROVED: install refuses unless RecompRegistry::rec_func_index(0x80061308) >= 0 — i.e. it is a MAIN-module function ENTRY the override table can reach — and logs an unconditional line either way. RUN PROVED: '[rseam] submitFrame override REACHED — call #1 at frame 2, ra=80061218' appears in EVERY run this session, on BOTH legs; frame 2 and ra=0x80061218 match RE-19's independent PSXPORT_FNTRACE measurement to the digit. Reach then accumulates on an unconditional line every 512 calls (calls=512 at frame 1678, calls=1024 at frame 2720 in the 200 s census run) — not an atexit summary, deliberately, because the watchdog owns SIGINT/SIGTERM and _exit(130)s so atexit never fires here (instrument I027). REFERENCE LEG UNCHANGED, measured back-to-back in a quiet window: pre-change 3579 presents / rebuild_geom 674 / vram_writes 11164 over 60 s headless NOPACE, post-change 3585 / 677 / 11176 — i.e. both pinned at the ~59.94 Hz real-time field-clock ceiling this port's vblank_advance imposes. NATIVE LEG FAILS FAST AS SPECIFIED: PSXPORT_RENDER_PSX=0 aborts at submitFrame call #1 / frame 2 printing the scene identity, the DB lens (db=8009A75C ot=800CA5F4 otHead=800CE5F0 pool=800E5604 — the exact addresses RE-12 measured with PSXPORT_WWATCH, so the lens is independently corroborated) and projection state (geomValid=0 at that instant: the first submit is a boot-init submit, before FUN_80075D0C has run any viewport). BREAK-FIRST NEGATIVE CONTROL, run with a THROWAWAY build and NOT shipped: submit suppressed and the run allowed to continue -> 1 distinct picture over 20 presents, 0 of 19 consecutive pairs differ, 0 of 13,132,800 pixel comparisons changed, rebuild_geom frozen at 11; the SAME instrument in the SAME mode on the reference leg gave 3 distinct pictures, 2 of 19 pairs differing, max 226377 px = 32.75% of frame (claim C031). tools/present_flicker.py --selftest passed the same session. READ-ONLY INVARIANT ENFORCED, not asserted: everything the seam does outside the super-call runs inside a framework DisplayPassGuard, so any guest store from this file aborts with a guest backtrace.
- where: game/render/render_seam.cpp (the seam: install + the two legs + the fail-fast) and game/render/scene_id.cpp (the scene-identity lens), wired in game/core/main.cpp before native_boot_run() and listed in cmake/spiderman_port.cmake. Installed with psxport_recomp()->shard_set_override(0x80061308, ...), exactly like game/core/sync_native.cpp's field-wait. NOT GameHooks::drawOTag (unreachable, C025) and NOT platform_hle (that seam refuses a game address by design, correctly).
- gap: The DEFAULT leg is psx_render (the reference), set in spiderman_install_render_seam() before native_boot_run() reads PSXPORT_RENDER_PSX so an explicit env value still wins. That is the opposite of Tomba!2's default and it is the honest state, not a fallback: this port has ZERO native producers, so a pc_render default would abort every run — including every other agent's boot gate — on its first engine frame. The default flips WITH the first producer, not before it. NOT COVERED: the native leg has only ever been exercised at submitFrame call #1, which RE-19 shows is a boot-INIT submit and not an engine-rendered frame, so nothing here says what the abort prints once the engine loop is really running; and the two other mode loops that also call this pair (FUN_800160EC, FUN_8006F294) have still never been entered in any run. METHOD CORRECTION to this step's own former text: it prescribed DISTINCT COLOUR COUNT for the break-first test, and that metric gives the WRONG SIGN here — the suppressed leg reads 5444 colours against the reference leg's 628, because the frozen last-FMV frame is a photograph and the live scene is flat-shaded 3D. Use FRAME IDENTITY / consecutive per-pixel diff (which tools/present_flicker.py already ranks as primary).
- notes: This step does NOT require RE-12's GameConfig fields, does NOT require native_step_frame, and does NOT require RE-13. It is game-side only. The framework's guest-driven path already calls c->game->rq.flush() at the end of its DMA2 linked-list walk (gpu_native.cpp ~2065), so a producer that emits into RenderQueue and then super-calls the guest submit is drained; a producer that SUPPRESSES the guest submit has to flush itself. That is a design detail of this step, not a framework change.

### RE-21 — Render-walk inventory: what a native producer has to reproduce (FUN_8002BD5C)
- status: todo
- deps: RE-19
- evidence: PARTIAL, from the Ghidra decompile of FUN_8002BD5C (the frame loop's render phase, 2026-08-06). Its shape is already legible and is the map of the porting backlog: (a) a fullscreen/backdrop path guarded on gp+0x734; (b) FUN_80075D0C(&DAT_800A5758, &DAT_80098B00, ctx->ot) — the WORLD RENDER entry, and the SOLE caller of libgte SetGeomOffset/SetGeomScreen, i.e. the function whose projection RE-17 already records natively; note it is handed the OT explicitly; (c) FUN_8007A764/FUN_8007ABDC on four conditional objects (DAT_800B4E24/28, DAT_800B4E44/40/64, gp+0x728/0x72C, DAT_800B4E5C/60); (d) FUN_80076480(obj) on a fixed list of engine object globals — DAT_800B566C, DAT_800B5394, DAT_800B5268 (also passed to FUN_800513C0 and to the loop's exit test FUN_80059664), DAT_800B526C, DAT_800B5398, DAT_800B5234, DAT_800B4E2C, DAT_800B53A0, DAT_800B4E6C — plus FUN_80076FC0(DAT_800B5764), FUN_8006AD74, FUN_800762D4; (e) TWO WALKS OF A TYPED DISPLAY-OBJECT LINKED LIST: heads DAT_800B5238 and DAT_800B5234, next pointer at obj+0x1C, a 16-bit TYPE CODE at obj+0x34, and a class/vtable pointer at obj+0x3C from which the walk takes a sub-object offset and a function pointer — {+0x30 offset, +0x34 fn} for types 0x141/0x142/0x143 and {+0x88 offset, +0x8C fn} for types 0x134/0x135/0x136. MEASURED INVENTORY ADDED 2026-08-06 (instrument I028, game/render/frame_census.cpp, PSXPORT_DEBUG=fcensus): the call-graph map above now has NUMBERS under it. Boot-init frame (scene '....', call #1): 4098 OT nodes, 8 primitive words, ZERO pixel-writing primitives — its only primitive is libgpu's own chain-terminator packet at 0x800B0EE8, a GP0(80) 2x1 VRAM copy of (0,0) onto (0,0), plus four GP0(00). That measurement is what made the frame ENVELOPE the first producer. First 'dem1' frame (call #2): 67 nodes, 59 pixel-writing primitives — 12 textured quads (GP0 0x2C, tpage 0x0028, clut 0x00E2) forming a 3x4 grid spanning screen x -176..626 and y -21..282 with only a ~4x6 TEXEL patch of texture stretched across it (a heavily zoomed backdrop plane), one GP0(0x2E) semi-transparent textured quad, and 45 flat GP0(0x28/0x20) quads clustered at x 116..123 / y 248..264 which fall OUTSIDE the 512x240 clip and are therefore invisible. Peak 'dem1' frame: ~400 polys + 115 lines over ~700 nodes; 'l1a1' gameplay: ~300 polys over 4400 nodes. EMITTER ATTRIBUTED, 2026-08-06, by PSXPORT_WWATCH=800cec08,800cec2c + PSXPORT_WWATCH_BT=1 on the backdrop packet (scratch/prod1/logs/wwatch_backdrop.log): the host backtrace names the chain FUN_8002C354 -> FUN_8002C174 -> FUN_8002BD5C -> FUN_80076480 -> FUN_80077D64 -> FUN_8007C4D8 -> FUN_8007D978, where FUN_8007D978 is the POLY_FT4 writer and its a0 is the SCRATCHPAD (0x1F8001B0) — i.e. the vertices are GTE-projected into scratchpad staging before the packet is built, so a native producer for this layer must resolve from FUN_8007C4D8's inputs, not from the packet. THAT IS THE NEXT STEP AND IT IS NOT DONE.
- where: guest: render walk FUN_8002BD5C; world render + projection setter FUN_80075D0C; per-object renderers FUN_80076480 / FUN_80076FC0 / FUN_800762D4 / FUN_8007A764 / FUN_8007ABDC; display-object list heads 0x800B5238 and 0x800B5234 (next +0x1C, type +0x34, class +0x3C).
- gap: STILL NOT DONE BELOW THE PACKET LEVEL. The 2026-08-06 census says WHAT each frame contains and the wwatch backtrace says WHICH function built the first 'dem1' layer, but no object struct is decoded, no type code is named, and no vertex/texture source is identified — FUN_8007C4D8 (the backdrop layer's builder) has not been decompiled at all. This is the step that actually costs the work. It must be done type by type, each with a fail-fast for the types not yet handled (RE-18), and each verified BREAK-FIRST. Do NOT resolve geometry from the census: it is a DIAGNOSTIC that answers 'how big is the backlog', and reading the OT, GP0 packets or GTE registers to BUILD a picture is what external/psxport/docs/workspace/PROTOCOL.md bans absolutely. The FIRST concrete target is FUN_8007C4D8 + its SetRotMatrix/SetTransMatrix site, and its verification window is dem1 calls #2..#728.
- notes: The typed list at 0x800B5238 with a per-class vtable is the closest thing this port has to Tomba!2's node walk, and it is a far better producer source than the guest module registry that issue 0011 measured as useless. Two of the loop's phases are EMPTY STUBS in the retail build (FUN_8002BBCC and FUN_8002B184 are 'jr ra; nop'), so they cost nothing to own.

### RE-22 — Own the frame loop: a native FUN_8002C174, and what it is actually worth
- status: todo
- deps: RE-19, RE-21
- evidence: 
- where: guest: FUN_8002C174 (the loop), FUN_8002C354 (main's mode switch above it), FUN_800604CC / FUN_800160EC / FUN_8006F294 (the other mode loops that use the same beginFrame/submitFrame pair).
- gap: NOT DONE, AND DELIBERATELY LAST. Owning the frame loop means overriding FUN_8002C174 game-side and running its iteration natively while rec_dispatching each phase that is not yet owned — NOT standing up the framework's native_step_frame, which cannot be fed for this game (RE-12's gap: the OT and pool are heap-allocated, and GameConfig's fields are literal-shaped). WHAT IT IS WORTH, honestly: it does NOT unlock native rendering — RE-20 does that, one step and no framework change. What it unlocks is (a) presents aligned to the game's FRAME BOUNDARY instead of to the real-time field clock, which is the two-clock defect in issue 0012 (gpu_pace_subframe's literal 60.000 Hz against vblank_advance's 59.940 Hz), and (b) the framework's fps60 path — which is separately unavailable here because has_xyf is set only by RenderQueue::drawWorldQuad and this repo has zero callers of it, so (b) is worth nothing until a native producer uses that API. That is the entire case for this step; if presents are already acceptable, it can wait behind RE-21 indefinitely. COST: the iteration has 12 callees and 4 of them (FUN_8002BBD4 game logic, FUN_80062CE0 audio state machine, FUN_8002BD5C render walk, FUN_8007F930 OT relink) are large; the exit test FUN_80059664(DAT_800B5268) and the mode-exit code at gp+0x740 (0x800B4F34) must be reproduced exactly or main()'s mode switch above it takes the wrong branch. It also has to reproduce the loop's own pacing rule — 'if the vblank counter did not move this iteration, wait one field' — which today is satisfied by the port's field-clock HLE (game/core/sync_native.cpp) and would move into the native loop.
- notes: There are FOUR mode loops that drive the same pair, not one: FUN_8002C174 (gameplay/attract, measured 2 entries / ~1761 frames in a 100 s headless run), FUN_800604CC (entered once at frame 3714), FUN_800160EC and FUN_8006F294 (never entered in that run). Owning only FUN_8002C174 therefore leaves the other three on the substrate — which is another reason RE-20's submit-level seam is the better place to hang a renderer: all four go through it.

### RE-23 — Scene identity: the game's own level-name lens (0x800A568C / FUN_8005A734)
- status: re-verified
- deps: RE-19
- evidence: STATIC RE 2026-08-06 (unchanged, see below) PLUS THE RUNTIME CENSUS this step's gap demanded, taken 2026-08-06 through the render seam's own scene census (game/render/render_seam.cpp censusTick, instrument I027, claim C032). ~200 s headless run, PSXPORT_NOPACE=1 PSXPORT_FORCE_BUTTONS=4000, denominator 1024+ submitFrame calls / 3300 presents (scratch/re20/logs/census_200s.log). OBSERVED IN ORDER: call #1 frame 2 raw=00,00,00,00 -> code 0xFFFFFFD0, name NOT YET WRITTEN (an honest 'unset', printable=0, not a failure); call #2 frame 379 name='dem1' -> code 0x9901 (the 'd'/'D' scheme); call #9 frame 513 name='l1a1' -> code 0x0101, which lies inside the 0x100..0x105 range FUN_80062CE0's own switch tests; held through call 1024 / frame 2720. A second run with NO input reached unset -> 'dem1' only, and at frame 2248 instead of 379 (the intro FMVs not being skipped) — so the lens tracks the MODE, not the clock. So the encoder's output is real, is written by the mode switch, and takes at least three distinct values in one run, against the module registry's effectively one (C026). THE ENCODER IS PORTED, byte-faithful, in game/render/scene_id.cpp from the disassembly at 0x8005A734..0x8005A7B8: name[0] 'd'/'D' -> level 0x99; else name[1] '0'-'9' -> level = c-0x30; 'A'-'Z' -> c-0x31; 'a'-'z' -> c-0x51 (so letters are case-insensitive and land on 0x10..0x29, NOT 10..35); else the raw byte; result = (level<<8) | (name[3]-0x30) in WRAPPING 32-bit arithmetic — which is why the unset buffer encodes as 0xFFFFFFD0 rather than 0, and the port reproduces that rather than tidying it.
- where: guest: FUN_8005A734 (the encoder), 0x800A568C (current level-name string), 0x800A5688 (the adjacent buffer main() writes via FUN_8001895C), consumer FUN_80062CE0.
- gap: COVERED NOW: the lens exists at runtime, changes with the mode, and discriminates. STILL NOT COVERED, and this is the honest limit: nothing correlates a code with a PICTURE. In particular 'l1a1' has NOT been shown to differ between the attract fly-through and live gameplay — C026's present shots proved those two are visually different scenes inside one constant module set, and the same question has not been asked of this lens. Settling it needs a run that captures present shots alongside the census. And it still says nothing about WITHIN-LEVEL substate (front-end page, cutscene vs play), which remains RE-13.
- notes: This is the datum RE-18's classifyScene needs and issue 0011 concluded did not exist. Issue 0011 was not wrong about what it measured — the guest MODULE REGISTRY really is a useless discriminator (8 events / 3 names over 13757 presents, 94.3% of the run on one constant set) — it was wrong to generalise from that one source to 'there is nothing to classify on'. Correct issue 0011 rather than leaving it standing.


## module-loader

### HACK-02 — One-slot module pinning: all 30 CD.WAD modules share base 0x800C65EC
- status: re-verified
- deps: 
- evidence: RESOLVED 2026-08-04. The 30 CD.WAD modules are emitted BASE-RELATIVE and the guest allocator places each body, as the console does. Boot: before = SIGSEGV at the first multi-module load with recomp-MISS 0x800C6684, 0 frames presented; after = 7176 frames in 120s (~60fps), 0 misses, with L5A5LSC (0x8014A6D0), LIZMAN (0x801BDA30) and VENOM (0x801C6238) simultaneously live at three distinct bases (scratch/logs/boot_after.log, scratch/logs/frames_after.log). Emitter gate: emit.py's HI16-consumer check examined 911 sites / ~800 uses across all 30 modules and found 0 escapes of a raw high half. Framework gate: external/psxport/tests/test_overlay_reloc.cpp, 6/6 tests, 41 checks — shown RED first (5/6 failing) with the old static-range routing.
- where: game/core/module_loader.cpp, external/psxport/tools/recomp/emit.py (ModuleReloc), external/psxport/runtime/recomp/overlay_router.cpp
- gap: 
- notes: The slot reservation, free()'s ownership refusal, warn_if_coresident and overlay_set_resident are DELETED, not disabled. The invariant that replaced the co-residency warning is a real gate: overlay_place() aborts if two live modules' ranges overlap. KNOWN CONSEQUENCE, recorded rather than discovered later: a HI16-relocated lui's INTERMEDIATE register value is hi(link)+delta where the console holds hi(live) — identical once a low half is added (which the consumer gate proves always happens), but not bit-identical in the register itself, so an SBS register-level compare over module code would report that as a divergence.
