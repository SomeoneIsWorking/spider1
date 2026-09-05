# RE Frontier — the ordered RE dependency chain toward a faithful BL2

Tracked by `tools/re_frontier.py` (consult it FIRST; update it in the SAME commit
that changes a step). This is the fine-grained companion to `docs/codemap.md`:
the codemap says *what subsystem exists*, this says *which ordered RE step is
real reverse-engineering vs a hack that jumped ahead*.

**Hard rule (no hacks / no behavior fallbacks):** a `⛔ hack` status is DEBT, never an
acceptable resting state. It marks a shortcut standing in for absent RE and MUST
be removed as its real mechanism lands. `re_frontier.py hacks` is the debt list;
`re_frontier.py next` tells you the next RE-ready step. This does not prohibit the
dynamic executor's classified, bounded automatic block fallback after a JIT refusal; that is CPU
backend policy, not a substitute game implementation or selectable gameplay mode.

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

### RE-00 — Authenticate the Spider-Man executable as a runtime image
- status: re-verified
- deps:
- evidence: PS-X EXE header: entry 0x8008739C, load 0x80010000, text 0xB6800; title selection authenticates serial, size, PS-X EXE identity, and SHA-256 before guest execution. CdInit's HookEntryInt continuation is binary-measured at 0x8008B990.
- where: titles/spiderman1/title.json; game/core/executable_identity.*; runtime-image provisioner target in docs/migration.md
- gap:
- notes: This step proves the image identity and binary facts only. It does not claim Lightrec execution; the old emitted-corpus counts are not part of the target product contract.

### DYN-00 — Integrate per-Core Lightrec execution in psxport
- status: re-partial
- deps: RE-00
- evidence: psxport now exposes per-Core dispatchGuest/callOriginal, typed exits, image identity, override dispatch, and invalidation; the sole Spider product calls dispatchGuest at authenticated crt0.
- where: external/psxport/runtime/cpu; game/core/guest_execution.*; game/core/spider_port.cpp
- gap: The exact maintained Lightrec backend is linked and its Linux x86-64 synthetic runtime contract proves nonzero translated blocks. Prove the same denominator on an authenticated Spider image, with classified bounded fallback and no selectable interpreter mode.
- notes: Lightrec owns translation/cache memory; psxport owns Core synchronization, bounded exits, runtime dispatch, original calls, and invalidation.

### DYN-01 — Route image-aware Spider calls and module invalidation through the executor
- status: re-partial
- deps: DYN-00, RE-09
- evidence: RE-09 proves that CD.WAD modules coexist at guest-allocated bases, so address-only cache and override keys are insufficient.
- where: external/psxport/runtime/cpu/native_dispatch.*; game/core/guest_execution.*; titles/spiderman1/spider1_movie_execution.*
- gap: Generated-symbol registration is removed and scoped-original wiring exists. Prove a native override plus positive/negative invalidation across live executable/module generations.
- notes:

### DYN-02 — Reach dem1 and resume the retail movie body dynamically
- status: re-partial
- deps: DYN-01, RE-04, RE-22
- evidence: The retired product completed both logo movies and reached dem1. FUN_8002AA0C reaches libetc VSync from 0x8002AC8C, 0x8002AE1C, and 0x8002AFEC; issue 0021 preserves the observed cadence and ordering failures.
- where: titles/spiderman1/spider1_movie_execution.*; titles/spiderman1/spider1_frame_driver.*
- gap: The generated movie body is absent and the retail callOriginal boundary exists; run nonzero Lightrec blocks, complete both logos, and reach early dem1 with typed exits.
- notes: This is the first implementation discriminator, not representative gameplay evidence.

### DYN-03 — Representative gameplay conformance
- status: todo
- deps: DYN-02
- evidence: docs/migration.md defines the bounded acceptance surface; the central portfolio plan explicitly says boot, logos, demos, and FMV are not representative gameplay.
- where: product launcher/build and runtime verification in docs/migration.md
- gap: The offline pipeline is already deleted break-first. Prove a representative interactive route, independent-oracle state comparison, native/original dispatch, module invalidation, host correctness/performance, and bounded reason-coded fallback with no selectable interpreter mode.
- notes: No compatibility selector, unbounded interpreter mode, or pre-populated cache may remain.

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
- evidence: movie descriptor table at 0x80097DEC (24-byte stride) read from the indexing routine 0x8002B0F4; entry 0 frameBytes 0x25800 = 320*240*2 is self-consistent. `scratch/logs/spider1-postlogo-owned-live.log` visibly renders and completes both real-disc logo calls (204 and cumulative 423 STR fields), then continues through finite boot into `dem1`; inspected `present_4400` contains the second logo image
- where: 
- gap: Playback now reaches the display and both logos complete under native-owned cadence. Faithfulness remains partial: no pacing comparison against the STRs' real duration and no user-audible/windowed XA/ADPCM comparison against the console exist. Headless WAV data proves sample production, not perceived A/V synchronization.
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
- gap: Spider-Man heap-allocates both OTs and packet pools, so the runtime/native boundary must read the active DB pointer and then its +0x70/+0x74 members. The observed heap values are evidence from one allocation order, never constants to bake. Re-establish the pointer-indirection contract after dynamic gameplay reaches this frame loop.
- notes: Frame-loop ownership is tracked as RE-22 and the image-aware render seam as RE-20.

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
- evidence: The retired product established byte-exact extraction over 112912 bytes and falsified its one-slot residency premise: the guest allocator placed three simultaneously live modules at distinct bases during a real boot. The dynamic runtime must preserve this observed image-lifecycle fact without carrying forward the offline relocation implementation.
- where: external/psxport/runtime/cpu; authenticated CD.WAD module images
- gap: 
- notes: Historical relocation-shape tooling was removed with the static product. Runtime image identity, load/unload, and invalidation now own this requirement.

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

### RE-07 — Intro FMV / front-end
- status: re-partial
- deps: RE-04
- evidence: DELIVERY is byte-correct and re-verified. CdlSetloc 28:32:54 -> LBA 128304 == CINEMAS/ATVILOGO.STR and 62:15:25 -> LBA 280000 == CINEMAS/LOGO.STR (discdump list); MDEC DMA0 word counts 1824 and 1440 equal those files' BS-header word counts read off the disc. THE RING DEADLOCK IS FIXED (2026-08-05, claim C017). Root cause was NOT what the first write-up said: DPCR (0x1F8010F0) and DICR (0x1F8010F4) were unmodelled in the framework, so the runtime signalled the guest's DMA-completion callback 0x8008DB44 on EVERY DMA3 transfer. libcd DMACallback (0x8009152C) arms DICR bit 16+ch plus the master bit 23; libstr's DMA starter FUN_80085948 read-modify-writes the BYTE at DICR+2 per transfer; FUN_80085000 arms it for the LAST chunk of an STR frame only. So the frame-ready marker was written once per SECTOR, slots 1..7 carried a spurious status 2, and the producer refused to overwrite slot 7. A/B on ONE binary toggling only the gate: BEFORE the ring wedges at frameStart=7 cons=9 writeIdx=7 status "0 2 2 2 2 2 2 2 0..." over 42 decimated samples and intro presents are 0.00% non-black / 1 colour; AFTER the ring never wedges (1 sample, writeIdx=1 status "3 0 0..."), dmairq shows 9 chunks armed=0 then the 10th armed=1, TWO full STR frames arrive per movie, and intro presents are 40.62% / 140 colours. Menu unaffected: f2400 99.44% / 1120 colours, f4000 83.70% / 1315, 0 abort, 0 recomp-MISS. The ring has 48 slots (DAT_800C1520 read at runtime) — the earlier "cons=9 is past the end" was a bad inference and C016 is corrected. Logs: scratch/logs/re07_before.log, re07_after.log, re07_gate.log. Hermetic gate: external/psxport/tests/test_dma_irq_gate.cpp, 7 cases / 30 checks, shown RED 0/7.
- where: external/psxport/runtime/psx/dma_irq.h (DPCR/DICR rules), mem.cpp (register state and DMA3 completion gate), and hle.cpp (BIOS-handler acknowledgement); guest StGetNext=FUN_80086B10, StFreeRing=FUN_800872AC, producer=FUN_80085000, DMA starter=FUN_80085948, frame-ready callback=FUN_8008DB44, DMACallback=0x8009152C; ring state DAT_800C1510..DAT_800C1520. Per-channel completion delivery is owned by the same runtime/psx files. The historical windowed starvation defect was in external/psxport/runtime/psx/gpu_vk.cpp and is resolved.
- gap: Historical headless and windowed runs established the CD/MDEC/DMA and swapchain-pacing facts, but the current Lightrec product has not replayed the route. Re-run both logo movies through authenticated dynamic execution, capture translation/fallback denominators, and compare A/V timing against an independent PSX reference.
- notes: GameConfig::bootFmv is empty and PSXPORT_NO_FMV is a no-op here; playback is entirely the guest path. C036 accounts statically for all five calls to the single STR player and now adds bounded runtime evidence: Start and Cross each produced a post-30 edge for boot ID0 and reached the common full teardown with active state cleared. After correcting the diagnostic's headless release lifecycle, Cross did the same for boot ID1 (2/2 boot calls); the earlier ID1 negative was an injection defect, because `Pad::driveRelease()` relinquishes replacement ownership without changing the last sampled button state and headless mode has no SDL poll to do so. Queued and dispatcher calls were not reached, so those paths have no runtime verdict. Boot's held-Start test suppressing ID1 was observed. No alternate player or behavior patch was introduced. This establishes neither queued responsiveness nor non-STR in-engine sequence skipping; the within-level state owner remains the RE-13 gap. Instruments remain PSXPORT_DEBUG=dmairq (I010), corrected PSXPORT_DEBUG=ring (I011), and wired PSXPORT_FNTRACE.

**2026-08-14 runtime skip discriminator:** `PSXPORT_STR_SKIP_ORACLE=start|cross|held` drives only the framework pad seam and observes the shipping `FUN_8002AA0C` body at qualified PCs. Start on boot ID0 produced a guest edge after the 30-tick guard and reached the common teardown with active state cleared (1/1); the resulting held byte made the retail boot owner suppress ID1. Cross reached both boot IDs and, after an explicit idle replacement frame between them, each observed release before injection and produced an edge-driven common teardown at tick 31 (2/2). The earlier ID1 negative was an instrument defect: headless `Pad::driveRelease()` does not reset the sampled buttons and no SDL poll replaces them. The checkpoint at `0x8002AEF8` is reported honestly as `guard_arm`: it is reached before the two edge-clearing stores, not after them. Queued movies reported **MISSING CORPUS: 0 invocations; no verdict**. This advances C036 for both measured boot paths; it does not certify queued movies, menu arrival, or in-engine sequences. Fresh current-source logs: `scratch/logs/str_skip_start_current.log`, `scratch/logs/str_skip_cross_release_current.log`. `PSXPORT_SELFTEST=strskip` exercises the same event classifier as shipping observation across tick 29/30, early suppression, late exit, held non-retrigger, Start/Cross mode selection, owner/context validation, and missing-queued-corpus semantics.

### RE-08 — Render: native per-vertex depth from the guest's own swc2/mfc2 store (store hook, NOT a GTE tap)
- status: re-partial
- deps: RE-09
- evidence: The retired product's depth measurement is preserved as binary-behavior evidence, not as an implementation to restore. Two independent 240 s runs reaching dem1/dem2/dem3/dem4/l1a1 measured 1,354,377 of 2,223,704 primitives (60.91%) with per-vertex depth and an 84.60% vertex-depth lookup hit rate. The current dynamic product has not re-established this coverage.
- where: Guest tap carriers identified by Ghidra: FUN_8007C4D8; FUN_8007B628/B798/B9CC/BE90/C2AC; FUN_80080988; FUN_80028030/FUN_800301E4/FUN_80029334. The dynamic renderer boundary must consume equivalent provenance without restoring offline source rewriting.
- gap: COVERAGE, not mechanism. 39% of prims still fall to the 2D order band and 15% of lookups miss, ABSENT-dominated — the tap never fired at those packet words. Root shape identified by disassembly (disasm.py spot-check after Ghidra): Spider-Man's unrolled builders read projected XY via mfc2 into a register, SPILL it as a stack word (`sw t0,0x90($sp)`), reload it (`lw v1,0x90($sp)`), and store HALFWORDS into the packet (`sh v1,($t7)` / `sh v0,-4($s0)` — e.g. FUN_80028304..8002831C). Neither tap form sees this: _track_value collects `sw` only, dies at the spill because it does not track through memory, and halfword stores are invisible besides. A direct mfc2->sh scan over the whole corpus finds ZERO pairs, so a naive sh extension closes NOTHING — the fix is spill-through tracking (one memory hop) plus sh targets, with gte_record_pz keying the CONTAINING aligned word (addr & ~3): safe for existing word-aligned sites (masking is identity there) and order-independent for halfwords because the second half-store always snapshots the completed word as its guard. Also counted and deliberately untouched: 593 swc2 stores of non-screen-XY cop2 registers (SZ FIFO et al.) — a future SZ-direct form if coverage analysis demands it. This extension is a FRAMEWORK emitter change in the recomp-emitter claim area. The current 9c2e3f1c pin still lacks spill-memory and halfword-target tracking; declared there, not attempted in this game-only milestone.
- notes: The per-frame PSXPORT_DEBUG=ndepth line stays DISTRUSTED (INST-26); everything cited above comes from render_depth_coverage_report's whole-run totals, whose zero case prints "NO PRIMITIVES WERE CLASSIFIED AT ALL" instead of a percentage. Whether 61% 3D-prim coverage is near this scene mix's ceiling or far from it is UNKNOWN — dem* attract scenes are 2D-heavy, so per-scene breakdown is the next measurement before judging the remaining gap's size.

### RE-17 — Native camera projection constants (libgte SetGeomOffset / SetGeomScreen)
- status: re-verified
- deps: 
- evidence: MEASURED 2026-08-06. The two libgte leaves were located by the ONLY thing that marks them, the instruction they execute: 'python3 tools/ghidra_query.py scan ctc2' (new mode) walked 130,588 disassembled instructions across 1,564 defined functions, found 1,404 ctc2 sites, of which exactly 10 target cop2 control regs 24/25/26. SetGeomScreen = 0x8008BF14 (ctc2 $a0,$26 ; jr $ra ; nop). SetGeomOffset = 0x8008BF24 (sll $a0,$a0,0x10 ; sll $a1,$a1,0x10 ; ctc2 $a0,$24 ; ctc2 $a1,$25 ; jr $ra ; nop). 0x8008BE5C is InitGeom (seeds ZSF3=0x155 ZSF4=0x100 H=0x3E8 DQA=-0x1062 DQB=0x1400000, zeroes OFX/OFY) and is NOT registered. COVERAGE NEGATIVE CONTROL: Ghidra had disassembled only 24.9% of the 2MB image, so a raw word scan of ALL 524,288 words of ram.bin for every ctc2 rX,CR24/25/26 encoding and for jal 0x8008BF14 / 0x8008BF24 / 0x8008BE5C returned the SAME 10 sites and the SAME single caller each - 0 sites hidden by the undisassembled 75%. RUNTIME GATE, on the real boot with the real disc: PSXPORT_DEBUG=geomwatch prints 'GATE PASSED: requireGeom() returned OFX=256 OFY=120 H=276 without aborting' (scratch/logs/g7/run_gate.log). NEGATIVE CONTROL, same binary path, same instrument, only the two GameConfig addresses zeroed: the same probe prints 'valid=0 OFX=0 OFY=0 H=0' and requireGeom ABORTS with 'SetGeomOffset (OFX/OFY) NEVER RAN - SetGeomScreen (H) NEVER RAN' (scratch/logs/g7/run_gate_control.log, run_control.log); plat-hle installs 8 primitives with the fix vs 6 without. BOOT-PROGRESS GATE: 70 s headless, with fix 4187 presents / reuse_last 2842 / rebuild_geom 917 / rebuild_vram 428 / vram_writes 12554; without 4189 / 2843 / 918 / 428 / 12554. The ONLY differing line in the entire non-presentskip log stream across both runs is the plat-hle count (8 vs 6) - zero behavioural change.
- where: external/psxport/runtime/psx/proj_params.cpp records libgte SetGeomOffset=0x8008BF24 and SetGeomScreen=0x8008BF14. Guest FUN_80075D0C is the sole caller; it computes vp[7]=H, vp[8]=OFX, and vp[9]=OFY in the viewport descriptor at gp+0x1124 = 0x800B5918. FUN_8007C2AC restores CR24/25 from the same descriptor, so no second recording path is needed.
- gap: H is only observed for the boot/front-end viewport reached in a 70 s headless run (OFX=256 OFY=120 H=276, constant over all sampled calls). Gameplay/cutscene viewports are NOT observed, and since the game DERIVES H per view (vp[7] = ((((vp[0]-vp[2])>>1)<<12)/vp[6]<<12)/*(int*)(gp+0x1140)) the value WILL differ elsewhere. That is why nothing is baked into GameConfig; the recording is per-call and stays correct, but no one has yet watched a gameplay viewport switch. Not compared against real PSX hardware.
- notes: Tomba!2's pattern TRANSFERS as a mechanism but NOT as data: Tomba!2 passes literal constants (OFX 160 / OFY 120 / H 350) and its config comment records them; Spider-Man passes computed viewport fields and has no constants to record. OFX=256 is 512/2, the centre of this game's 512-wide framebuffer - a further datum against this workspace's hardcoded-320 assumptions. Also repaired a latent defect found while editing: the hle POSITIONAL initialiser had the entry commented /* vsyncTrap */ sitting in the setGeomOffset SLOT, so setting vsyncTrap would have installed the VSync abort at the GTE setter. Harmless only because all three fields were 0.

### RE-18 — Native renderer: scene classifier and cohesive display-list producers
- status: re-partial
- deps: RE-20,RE-21,RE-23
- evidence: Historical runs proved that the empty boot-init frame can be represented by the retained frame-envelope owner and that the next `dem1` frame contains real pixel-writing primitives. The former static dispatch seam and whole-frame compatibility path are removed; this evidence does not claim a current native renderer.
- where: retained source contracts in `game/render/`; scene identity in `game/render/scene_id.*`; target image-aware title seam beneath `titles/spiderman1/`
- gap: Attach a new image-aware seam only after JIT conformance, then implement complete source-state producers for each reached display-object class. Never restore whole-frame compatibility selection or consume guest OT/GTE output as native-producer input.
- notes: The title's submitFrame `FUN_80061308` is the measured game-side boundary. Generic `GameHooks::drawOTag` is not its owner.

**Preserved widescreen evidence:** `FUN_80075D0C` owns the active `u16` viewport descriptor:
[0..3] horizontal/vertical bounds, [6] lens divisor, output [7]=H and [8..9]=OFX/OFY. A historical
real-disc run held one stable `512x240 -> 684x240` and lens `2365 -> 3159` mapping through `dem1`.
The old guest-call attachment is removed. `Spider1Widescreen` retains only the pure aspect policy
until a new image-aware boundary is attached and verified through the dynamic product.

### RE-19 — The guest's frame driver and the engine's render seam (beginFrame / submitFrame)
- status: re-verified
- deps: RE-00
- evidence: RE'd 2026-08-06 with Ghidra headless (tools/ghidra_query.py func/xrefs), never by hand-walking disassembly. FRAME DRIVER: FUN_8002C174, the SOLE callee of main()'s mode switch (xrefs 0x8002C174 -> 1 caller, FUN_8002C354 @0x8002C420). Its body is a per-frame loop 'while (*(gp+0x740) == 0)' whose iteration is: FUN_8002BBCC (empty stub) ; vbl0 = 0x800B5468 ; FUN_800612B8 (BEGIN FRAME) ; FUN_8002BB9C (second pool cursor = ctx->pool + 0x16F00) ; FUN_8002BBD4 (game logic: pad read FUN_8006B514 + actor updates) ; FUN_80062CE0 (per-frame audio state machine) ; FUN_8002BD5C (THE RENDER WALK) ; FUN_8002B184 (empty stub) ; FUN_8007F930(ot, 0x1000) (in-place OT relink pass) ; if (0x800B5468 == vbl0) FUN_8005E748(1) — guarantees at least one field per frame ; a DrawSync(1) spin ; FUN_80061308 (SUBMIT) ; FUN_80059664 exit test. THE TWO SEAM FUNCTIONS, both GAME addresses in [recMainLo, recMainHi): FUN_800612B8 = beginFrame — flips the double-buffer context gp+0xCB4 (0x800B54A8) between 0x8009A6E4 and 0x8009A75C, calls ClearOTagR(ctx->ot, 0x1000), sets the pool cursor gp+0xCBC (0x800B54B0) = ctx->pool & 0x7FFFFFFF. FUN_80061308 = submitFrame — ResetGraph(1) ; PutDispEnv(ctx+0x5C) ; PutDrawEnv(ctx+0x00) ; DrawOTag(ctx->ot + 0x3FFC). THE libgpu LEAVES ARE GROUND TRUTH, identified by the diagnostic strings the library itself emits at verbosity DAT_800B0E2A>1, not by inference: 0x80081ED0 DrawOTag ('DrawOTag(%08x)' @0x80095EA0), 0x80081DC8 ClearOTagR ('ClearOTagR(%08x,%d)' @0x80095E88), 0x80082000 PutDispEnv (@0x80095EE8), 0x80081F40 PutDrawEnv (@0x80095EB4), 0x800819A4 DrawSync (@0x80095DFC), 0x8008173C ResetGraph (@0x80095D78); 0x80081E74 = DrawPrim (driver slot +0x14, no string). The libgpu driver vtable is *0x800B0E20 (DrawOTag=+8, PutDispEnv/GP1=+0x10, DrawPrim=+0x14, ClearOTagR=+0x2C, ResetGraph=+0x34, DrawSync=+0x3C). DrawOTag HAS EXACTLY 2 CALLERS in the whole image (xrefs 0x80081ED0): FUN_80061308 and library-internal FUN_8008A614 — so FUN_80061308 is the ONE game-side OT submit. RUNTIME MEASUREMENT, 100 s headless, no input, PSXPORT_FNTRACE on 8 addresses (scratch/re12/logs/fntrace1.log): 0x80061308 = 1761 calls (first at frame 2, ra=80061218), 0x800612B8 = 1765, 0x80061140 = 1, 0x80061230 = 1, 0x8002C174 = 2 (first at frame 2261), 0x800604CC = 1 (frame 3714), 0x800160EC and 0x8006F294 NEVER CALLED. ABI violations 0 on every site. fntrace installs a real override at each address and the body still runs, so those 1761 hits ARE the proof that an override at 0x80061308 executes.
- where: guest: frame driver FUN_8002C174; beginFrame FUN_800612B8; submitFrame FUN_80061308; render walk FUN_8002BD5C; gfx init FUN_80061140; OT/pool alloc FUN_80061230; mode switch main FUN_8002C354. Guest state: DB* current = 0x800B54A8 (gp+0xCB4), pool cursor 0x800B54B0 (gp+0xCBC), second pool cursor 0x800B4FE8 (gp+0x7F4), mode-exit code 0x800B4F34 (gp+0x740), in-mode frame counter 0x800B4F38 (gp+0x744).
- gap: The 100 s headless run reached the engine frame loop only at frame 2261 and produced 1761 engine frames with NO pad input, so this covers boot + attract only. FUN_800160EC and FUN_8006F294 — two further mode loops that also call the beginFrame/submitFrame pair — were never entered, so their per-frame shape is INFERRED FROM THE CALL GRAPH, not observed. Nothing here has been compared against real PSX hardware.
- notes: The FMV/boot phase does NOT use this pair: over frames 2..2261 the port presented ~2261 frames while beginFrame/submitFrame were entered ~4 times (init only). So a producer hung on 0x80061308 covers ENGINE-rendered frames (front-end 3D, gameplay) and NOT intro FMV — which is the correct scope.

### RE-20 — Native render seam: a game-side override on the engine's submitFrame (0x80061308)
- status: re-partial
- deps: RE-19, RE-17
- evidence: Historical function tracing proved `FUN_80061308` is reached as the sole game-side OT submit, first at frame 2 from ra=0x80061218, and that suppressing it freezes presentation. The old static override implementation has been deleted; only the binary seam and its measurements remain authoritative.
- where: guest submit `FUN_80061308`; target image-aware registration beneath `titles/spiderman1/`; classifier `game/render/scene_id.*`
- gap: Install and exercise this seam through `NativeDispatcher` after authenticated Lightrec execution reaches it. Prove reference behavior and native ownership without a renderer selector, whole-frame compatibility path, or double draw.
- notes: Frame identity and consecutive per-pixel differences are the visual discriminator; distinct-colour count alone gives the wrong sign on a frozen colourful frame.

**Preserved frame-fence evidence:** historical execution showed that the unified queue needs exactly
one frame commit after each complete retail `FUN_80061308` boundary. The compile-checked
`Spider1FrameDriver` retains that phase model, but it is not attached to the current product and has
not run around Lightrec.

### RE-21 — Render-walk inventory: what a native producer has to reproduce (FUN_8002BD5C)
- status: re-partial
- deps: RE-19
- evidence: PARTIAL, from the Ghidra decompile of FUN_8002BD5C (the frame loop's render phase, 2026-08-06). Its shape is already legible and is the map of the porting backlog: (a) a fullscreen/backdrop path guarded on gp+0x734; (b) FUN_80075D0C(&DAT_800A5758, &DAT_80098B00, ctx->ot) — the WORLD RENDER entry, and the SOLE caller of libgte SetGeomOffset/SetGeomScreen, i.e. the function whose projection RE-17 already records natively; note it is handed the OT explicitly; (c) FUN_8007A764/FUN_8007ABDC on four conditional objects (DAT_800B4E24/28, DAT_800B4E44/40/64, gp+0x728/0x72C, DAT_800B4E5C/60); (d) FUN_80076480(obj) on a fixed list of engine object globals — DAT_800B566C, DAT_800B5394, DAT_800B5268 (also passed to FUN_800513C0 and to the loop's exit test FUN_80059664), DAT_800B526C, DAT_800B5398, DAT_800B5234, DAT_800B4E2C, DAT_800B53A0, DAT_800B4E6C — plus FUN_80076FC0(DAT_800B5764), FUN_8006AD74, FUN_800762D4; (e) TWO WALKS OF A TYPED DISPLAY-OBJECT LINKED LIST: heads DAT_800B5238 and DAT_800B5234, next pointer at obj+0x1C, a 16-bit TYPE CODE at obj+0x34, and a class/vtable pointer at obj+0x3C from which the walk takes a sub-object offset and a function pointer — {+0x30 offset, +0x34 fn} for types 0x141/0x142/0x143 and {+0x88 offset, +0x8C fn} for types 0x134/0x135/0x136. MEASURED INVENTORY ADDED 2026-08-06 (instrument I028, game/render/frame_census.cpp, PSXPORT_DEBUG=fcensus): the call-graph map above now has NUMBERS under it. Boot-init frame (scene '....', call #1): 4098 OT nodes, 8 primitive words, ZERO pixel-writing primitives — its only primitive is libgpu's own chain-terminator packet at 0x800B0EE8, a GP0(80) 2x1 VRAM copy of (0,0) onto (0,0), plus four GP0(00). That measurement is what made the frame ENVELOPE the first producer. First 'dem1' frame (call #2): 67 nodes, 59 pixel-writing primitives — 12 textured quads (GP0 0x2C, tpage 0x0028, clut 0x00E2) forming a 3x4 grid spanning screen x -176..626 and y -21..282 with only a ~4x6 TEXEL patch of texture stretched across it (a heavily zoomed backdrop plane), one GP0(0x2E) semi-transparent textured quad, and 45 flat GP0(0x28/0x20) quads clustered at x 116..123 / y 248..264 which fall OUTSIDE the 512x240 clip and are therefore invisible. Peak 'dem1' frame: ~400 polys + 115 lines over ~700 nodes; 'l1a1' gameplay: ~300 polys over 4400 nodes. EMITTER ATTRIBUTED, 2026-08-06, by PSXPORT_WWATCH=800cec08,800cec2c + PSXPORT_WWATCH_BT=1 on the backdrop packet (scratch/prod1/logs/wwatch_backdrop.log): the host backtrace names the chain FUN_8002C354 -> FUN_8002C174 -> FUN_8002BD5C -> FUN_80076480 -> FUN_80077D64 -> FUN_8007C4D8 -> FUN_8007D978, where FUN_8007D978 is the POLY_FT4 writer and its a0 is the SCRATCHPAD (0x1F8001B0). ADVANCED 2026-08-20: Ghidra decompiles of FUN_80076480/FUN_80077D64/FUN_8007C2AC/FUN_8007C4D8/FUN_8007D978 decoded the first allowed producer inputs. FUN_80077D64's mesh header is `{u16 vertexCount@+2, u16 secondaryCount@+4, u16 faceCount@+6, records@+0x1C}`; source vertices and secondary records are both 8-byte arrays, followed by the variable-length face stream. `PSXPORT_DEBUG=meshprobe` wraps and super-calls the exact 80076480 -> 80077D64 -> 8007C4D8 chain (I031/C037). A forced-Cross reference-leg run reached unset -> dem1 -> l1a1 and counted 27,579 contextual face-builder calls with ZERO header/pointer/count mismatches before the gate supervisor refused on its own kill-path hang; at least 64 object/mesh pairs were sampled (the roster cap) (`scratch/logs/gate-boot-20260820-221812.log`). The first live tuple was initially labeled object 0x8018BB90, mesh 0x8018BC38; C040 later proved 0x8018BB90 was the outer list head and corrected the actual owner to 0x8018BBB4, counts 4/1/1, raw vertices 0x8018BC54, secondary 0x8018BC74, faces 0x8018BC7C, translation (-90,759,1377). A replay logged the instrument self-test PASS and a live MATCH before the same gate refusal (`scratch/logs/gate-boot-20260820-223032.log`); after null-safe diagnostic hardening, the final build repeated SELFTEST PASS in the four-second `scratch/logs/gate-boot-20260820-223543.log`, which ended before the first contextual face call (issue 0015). This resolves from the guest's object/mesh submission inputs, never OT/GTE output.
- where: guest render walk FUN_8002BD5C; world/projection FUN_80075D0C; display-object walk FUN_80076480; animated path FUN_80077198 -> FUN_80077C08 -> FUN_8007C4D8; direct path FUN_80077D64 -> FUN_8007C2AC -> FUN_8007C4D8; retained contracts in `game/render/mesh_face_format.*`, `mesh_transform.*`, `mesh_animated_vertex.*`, `face_builder_census.*`, `mesh_asset_cook.*`, and `asset_upload_ledger.*`.
- gap: No complete display-list producer exists. Re-establish an observe-only runtime corpus through the dynamic product, implement and diff the PC matrix composer against the measured inputs and retail outputs, reproduce fixed-point RTPS/outcodes, decode FUN_8007C4D8 cull/clip/light/colour behavior, then land cohesive producers. OT packets, rendered VRAM, scratchpad projection, and guest GTE output remain forbidden producer inputs.
- notes: The typed list at 0x800B5238 with a per-class vtable is the closest thing this port has to Tomba!2's node walk, and it is a far better producer source than the guest module registry that issue 0011 measured as useless. Two of the loop's phases are EMPTY STUBS in the retail build (FUN_8002BBCC and FUN_8002B184 are 'jr ra; nop'), so they cost nothing to own.

ADVANCED 2026-08-21 AT THE AUTHORED-ASSET DEPENDENCY (I035/C041): executable-derived wrappers
around the retail `.psx` loader/parser/unloader (`FUN_80069A60/80068BB0/800695D0`) and `LoadImage`
(`FUN_80081C50`) traced the exact first binding on a fresh run
(`scratch/logs/re21-asset-owner-live-final.log`). CD reads place `Dem1_L.psx` at `0x801539D4` and
`Dem1_G.psx` at `0x8018BB84`. At the first face, mesh `0x8018BC38` is retained at
`Dem1_G.psx+0xB4`; the texture target `(525,246 2x8)` and CLUT `(544,3 16x1)` were last uploaded by
`Dem1_L.psx` from offsets `0x2B30` and `0x38`. The parser shrinks `Dem1_L` from a 12,288-byte raw
allocation to 48 retained bytes before submission, after which `henchman.psx` starts at the former
CLUT source and covers the former texture source too. Both addresses already name different asset
bytes while slot 7 remains live. `Dem1_G` retains 7,836 bytes through submission and is then observed unloading with
the other level assets. The in-band and hermetic ledger selftests select the latest exact owner and
make a one-word target perturbation return MISSING, so a uniform owner result cannot pass silently.

ADVANCED 2026-08-21 AT THE NEXT DEPENDENCY-READY STEP: instruction-exact ranges establish that
scratchpad word `0x1F8003F4` masks/overrides the encoded header, its effective high half is the
face-record byte stride, flag `0x10` selects triangle versus quad, and flag `0x1` selects the direct
textured path that copies source offsets `+0x10/+0x14/+0x18` into a GP0(2C) FT4. The bounded live
probe decoded outer list head `0x8018BB90` / mesh `0x8018BC38`'s first face as a 28-byte
direct-textured quad (C040 later identified actual owner `0x8018BBB4`):
indices `[1,0,3,2]`; source vertices `(1970,-1,-78)`, `(-1967,-1,-78)`, `(1970,-1774,-78)`,
`(-1967,-1774,-78)`; UVs `[(59,253),(52,253),(59,246),(52,246)]`; CLUT `0x00E2` at `(544,3)`;
stored TPAGE `0x0008`; and, after the executable folds face flag `0x80` into it, effective 4-bpp
TPAGE `0x0028` at `(512,0)`, blend mode 1. All indices were in range
(`scratch/logs/gate-boot-20260821-025743.log`, I031/I033, C039). The decoder lives in
`game/render/mesh_face_format.cpp`; the probe derives the stride before reading optional fields.

ADVANCED 2026-08-21 AT THE OBJECT/MATRIX DEPENDENCY: exact instructions at 0x800767B0..0x80076824
load the camera rotation from `camera+0x74`, zero GTE translation, and compute the vector passed to
`FUN_80077D64` as `(objectPosition20p12 sra 12)-cameraPosition`. The only two direct calls are guarded
by zero object XYZ rotation and clear scale flag 0x0200; `FUN_8007C2AC` then adds that relative vector
to each ordinary source vertex before RTPS. The pure source-contract validator accepts the exact tuple
and rejects relative, rotation, scale, and callsite perturbations. Live log
`scratch/logs/gate-boot-20260821-032403.log` matched both return addresses 0x80076E78/0x80076E88 under
distinct camera matrices. It also corrected the old attribution: list head 0x8018BB90 carried 0x9000,
but actual first owner 0x8018BBB4 carries 0x0000 (C040/I031/I034). No GTE register or output is used as
input, and no native producer was guessed from this observation. The final Clang/tidy-clean binary
repeated the self-test and first MATCH with zero transform mismatches in the captured-PID bounded
`scratch/logs/re21-transform-final.log` run.

ADVANCED 2026-08-21 AT THE RETAINED FACE-COOK DEPENDENCY (I036/C043): exact disassembly of
`FUN_80068BB0 -> FUN_80074C98` shows the retail loader walking every mesh face in place. It toggles
header bit `0x80` when bit `0x40` is clear, strips word 2's high byte when bit `0x800` is clear,
shifts word 3's low half by three, and resolves direct-texture descriptor data into the retained
face's UV/CLUT/TPAGE words. `game/render/mesh_asset_cook.cpp` keeps bounded copies of the pre- and
post-cook first face of each supported mesh; it never retains guest pointers. The mesh probe later
compares all copied words with the record handed to `FUN_8007C4D8`. The final captured-PID run
`scratch/logs/re21-mesh-cook-live-final.log` recorded `Dem1_G.psx` raw=2/2, cooked=2/2,
structuralExact=2,
refused=0 and then matched all 28 bytes at face `0x8018BC7C`; unload invalidated the same slot.
The hermetic and in-band selftests mutate one cooked word to produce MISMATCH, query an absent face
to produce MISSING, and unload the exact face to produce UNLOADED. This closes the raw-source
lifetime ambiguity but does not justify a draw: the first face's `0x1000` lighting/projection path
is the next dependency.

CORRECTED 2026-08-22 BY THE COMPLETE FACE-BUILDER OWNERSHIP CENSUS (I038/C047): Ghidra reports
exactly twelve direct `jal FUN_8007C4D8` instructions in `SLUS_008.75`. The new pure census maps the
corresponding MIPS `ra=call+8` values, has known/unknown and valid/invalid-output opposite answers,
and the live wrapper super-calls every guest body. In the bounded `dem1` run
`scratch/logs/gate-boot-20260822-174218.log`, all 24,576 calls matched a statically enumerated site.
Only four families ran: `FUN_80077C08` 18,355 calls / 306,027 faces / 6,065,852 emitted bytes;
`FUN_8002EED4` 5,308 / 5,308 / 212,320; direct `FUN_80077D64` 593 / 593 / 284,640; and
`FUN_80077A48` 320 / 2,310 / 69,032. The first supposedly simple direct face advanced the primitive
cursor `0x0CEA50 -> 0x0CEC30`, 480 bytes from one 28-byte source record, proving that a one-quad
producer would skip the retail clip/expansion semantics. Exact disassembly then established the
dominant upstream chain `FUN_80077198 -> FUN_80077C08`: the outer function builds animated object
matrices, `FUN_80077C08` transforms the mesh's vertex array through `FUN_8007B798/8007B9CC`, derives
the same header-relative secondary/face pointers, and calls the common face builder. The observe-only
wrapper now carries that source owner/mesh context into `FUN_8007C4D8`. A second bounded run
(`scratch/logs/gate-boot-20260822-174725.log`) classified 14,793 `FUN_80077C08` calls as animated
mesh context by call 20,480, with 0 unknown callsites, 0 layout mismatches, 0 transform mismatches,
and 0 invalid cursor deltas. This implements the missing upstream ownership stage; it does not claim
a native draw. The gate still REFUSED because its known shutdown path left a reported orphan after
signalling; `ps` immediately afterward found no surviving Spider process.

PIN REPLAY 2026-08-22: `psxport.pin` now records clean framework `57a17a14`, a completely regenerated
`2026-08-22.1` corpus contains all 1,672 main functions and 30 overlays, and the Clang build plus all
eight CTests (including format, tidy, and structure) pass against that exact recorded SHA. The
observe-only ownership result survives in `scratch/logs/gate-boot-20260822-181035.log`: by call
16,384, animated `FUN_80077C08` owns 11,432 calls / 192,419 faces; the census reports zero unknown
callsites, invalid output deltas, layout mismatches, and transform mismatches. This run still ends in
issue 0015's gate-supervisor refusal, with no exact-path process remaining afterward. Behavioral pin
verification is nevertheless NOT clean: the separate first replay
`scratch/logs/gate-boot-20260822-180954.log` aborted before `dem1` when guest allocator
`FUN_80064FA0` followed invalid traversal value `0x04010401`. The corrected allocator first-write
audit has now reproduced and owned that damage on the clean `57a17a14` replay:
`scratch/logs/gate-boot-20260822-190346.log` stops at guest `FUN_8002A338:0x8002A478` writing VLC
halfword `0x0401` to live free node `0x801664E4`. Its first intended decode output was
`gp+0x6DC[0] = 0x800FDAC4`, allocated for only `0x25800` bytes, so it had crossed its end by
`0x43220` bytes before the watch reached the first still-free header. Retail `0x80097D84` is the
intentional `0x00FFFFFF` decoder limit and has no writer, so the executable relies on a terminating
VLC stream. Issue 0018 remains investigating because the upstream discriminator still has to decide
malformed/misordered CDC input from incorrect saved coroutine state. The allocator, face census,
full-register poll wrapper, and gate supervisor are not the first writer. This does not change the
no-producer claim or permit retrying until green.

ADVANCED 2026-08-26 AT THE ANIMATED SOURCE-STAGING DEPENDENCY (C054): exact disassembly of
`FUN_80077C08`, `FUN_8007B798`, and `FUN_8007B9CC` resolves the source record contract before the
still-unported matrix/projection result. Vertex flag `0x0002` does not describe an ordinary XYZ
vertex: it makes the retail routine skip RTPS and subtract the record's entire first word from the
shared transformed-vertex cache endpoint `DAT_800B58F0 + 0x1F38`, copying the cached projected
record instead. Flag `0x0001` additionally retains a freshly projected record in that shared cache.
This is cross-mesh staging state, so a native producer that blindly transforms every eight-byte
record per mesh would corrupt the dominant animated path. The two distance modes also preserve
different fixed-point order: far `FUN_8007B798` arithmetic-shifts each signed XYZ by four before
RTPS and stores MAC1..3 directly, while near `FUN_8007B9CC` submits full XYZ and shifts MAC1..3 by
four afterward. Both compute the same six-bit outcode shape and `FUN_80077C08` only enters the face
builder when `(commonMask & 0x00BF) == 0` and the submission is not suppressed. The source contract
lives in `game/render/mesh_animated_vertex.cpp`; its hermetic test distinguishes projected/reused,
retained/unretained, near/far, accepted/rejected, and exact reuse-address cases. The retired runtime
probe established the first corpus shape; a new observe-only dynamic-runtime instrument is required
before implementation continues.

ADVANCED 2026-08-26 AT THE PRE-GTE POSE-COMPOSITION DEPENDENCY: exact disassembly of
`FUN_80077198`, `FUN_8007FB1C`, and `FUN_8007FD1C` fixes all three composer inputs before hardware:
an eight-word base transform containing a packed s16 3x3 rotation plus three s32 translations, a
five-word packed secondary rotation, and a 24-byte authored pose containing nine s16 rotation terms
plus three s16 translations. The two retail composer entries are instruction-identical except that
the far path arithmetic-shifts the three authored translation terms by four before its final MVMVA;
the near path submits them at full precision. `game/render/mesh_pose_contract.cpp` decodes those
records and defines the future temporal identity as display owner plus authored pose address, with
opposite-answer tests for signed packing, near/far scaling, changed identity, and same-frame refusal.
The deleted static-product probe had not earned trust: its final serialized run produced zero live
pose/face corpus rows. Recreate the observation at the dynamic runtime boundary and require valid
rows, both available distance paths, a changed temporal pair, and nonzero oracle comparisons with
zero mismatches. No producer consumes the historical oracle and no interpolation is enabled.

LIVE FALSIFIER 2026-08-26, CLEAN FRAMEWORK `99a42aa3`: the serialized native product run
`scratch/logs/gate-boot-20260826-235605.log` exited 139 after 1.8 seconds. Meshprobe self-tested and
armed, and the render seam fired once at frame 2, but no `faceCall`, `POSE_CORPUS`, or meshprobe
`PROGRESS` row occurred. The run therefore supplied zero live pose/face corpus rows and does not
advance RE-21. After the first frame, `CdSearchFile` reported `/CINEMAS/TTSLOGO.STR;1` absent; the
process later aborted when `FUN_800651C8 -> FUN_80064FA0` traversed `0x04010401`. Issue 0018's prior
first-write evidence owns that allocator signature as downstream VLC free-list damage. This run did
not capture the first write again, and the ordering of the missing-file message before the allocator
fault proves no causal relationship between those events.

### RE-22 — Own the frame loop around resumable guest execution
- status: re-partial
- deps: RE-19, RE-21
- evidence: NATIVE-OWNER IMPLEMENTATION 2026-08-27 (C056), now compile-checked behind the dynamic guest-execution boundary. Historical execution established that `Spider1FrameDriver` installs the exact 72-byte persistent main frame and owns field/audio/pad/presentation service. `Spider1ModeDriver` owns the authenticated ten-entry selector table at 0x80093C3C and finite state for FUN_8002C174, FUN_800604CC, FUN_800160EC, and FUN_8006F294. It preserves retail phase order, splits FUN_800604CC's two submissions across two host steps, paces held-image countdown fields without rotating interpolation history, and maps every step to exactly one submitted, repeated-field, or measured unpresented fence. Authenticated RAM bytes at 0x80093C3C are `58c50280 7cc40280 f8c50280 9cc50280 9cc50280 00c50280 78c50280 e4c40280 7cc40280 74c50280`. The historical run visibly completed both logo movies, the exact 300-field post-logo input wait, and entered `dem1`; Lightrec has not re-verified that behavior.
- where: `titles/spiderman1/spider1_frame_driver.*`, `titles/spiderman1/spider1_mode_driver.*`, `titles/spiderman1/spider1_runtime.*`; guest FUN_8002C354, FUN_8002C174, FUN_800604CC, FUN_800160EC, FUN_8006F294.
- gap: The native owners compile against the dynamic APIs but are not attached and have not run around Lightrec. `Spider1MovieExecution` now expresses the unchanged retail movie body through scoped original execution; prove suspension/resumption at the three authenticated exits through `dem1`. Every finite mode transition and representative gameplay remain unverified. Enter Electro needs its own addresses and driver.
- notes: VSync 0x80084BE0 remains the protected all-mode abort. The retired generated movie body and generator are absent.

### RE-23 — Scene identity: the game's own level-name lens (0x800A568C / FUN_8005A734)
- status: re-verified
- deps: RE-19
- evidence: A historical 1024-submit/3300-present runtime census observed unset `0xFFFFFFD0`, then `dem1` `0x9901`, then `l1a1` `0x0101`; a no-input control reached only unset then `dem1`, proving the lens tracks mode rather than clock. `game/render/scene_id.cpp` preserves the binary-derived encoder from 0x8005A734..0x8005A7B8, including wrapping behavior for the unset buffer.
- where: guest: FUN_8005A734 (the encoder), 0x800A568C (current level-name string), 0x800A5688 (the adjacent buffer main() writes via FUN_8001895C), consumer FUN_80062CE0.
- gap: COVERED NOW: the lens exists at runtime, changes with the mode, and discriminates. STILL NOT COVERED, and this is the honest limit: nothing correlates a code with a PICTURE. In particular 'l1a1' has NOT been shown to differ between the attract fly-through and live gameplay — C026's present shots proved those two are visually different scenes inside one constant module set, and the same question has not been asked of this lens. Settling it needs a run that captures present shots alongside the census. And it still says nothing about WITHIN-LEVEL substate (front-end page, cutscene vs play), which remains RE-13.
- notes: This is the datum RE-18's classifyScene needs and issue 0011 concluded did not exist. Issue 0011 was not wrong about what it measured — the guest MODULE REGISTRY really is a useless discriminator (8 events / 3 names over 13757 presents, 94.3% of the run on one constant set) — it was wrong to generalise from that one source to 'there is nothing to classify on'. Correct issue 0011 rather than leaving it standing.


## module-loader

## boot


### EE-00 — Enter Electro serial-bound runtime image
- status: re-verified
- deps:
- evidence: USA disc SYSTEM.CNF boots SLUS_013.78;1; selected-media inspection rejects SLUS_008.75 before cached executable use. PS-X EXE is 786432 bytes, entry 0x80093C68, load 0x80010000, text 0xBF800 (loaded end 0x800CF800). Binary analysis found the highest resident entry at 0x800C28C8; the independently derived BSS starts at 0x800C2AF4 and executable bytes from there through 0x800CF0DC are zero. The runtime image must therefore route the pre-BSS resident span [0x00010000,0x000C2AF4), not the payload extent or heap boundary. No copyrighted output is tracked.
- where: tools/title_catalog.py; titles/spiderman2/title.json; target runtime-image provisioner
- gap: Resident executable only. The 28 CD.WAD runtime modules are deliberately not claimed before EE-02 reaches the game loader.
- notes: Both-answer tests accept SLUS_008.75 and SLUS_013.78 and reject missing, unknown, ambiguous, and mismatched media identities. The old generated registry established entry metadata only; it is not a product dependency.

### EE-01 — Enter Electro crt0 and direct derived runtime
- status: re-verified
- deps: EE-00
- evidence: Independent crt0 extraction over the real USA SLUS_013.78 resolves BSS 0x800C2AF4..0x800CF0DC, stack globals 0x800C0D10/0x800C0D0C with bias -8, heap 0x800CF0DC, gp 0x800C1764, libcInit 0x800988B0, and first game call 0x80031F54. The current title runtime carries those facts and builds against the shared executor without borrowing Spider-Man addresses.
- where: titles/spiderman2/enter_electro_runtime.{h,cpp}; authenticated guest crt0 0x80093C68
- gap:
- notes: No Spider-Man 1 title address or title context is bound.

### EE-02 — Enter Electro first game-owned call
- status: todo
- deps: EE-01, DYN-03
- evidence: crt0 jal 0x80031F54 at 0x80093D04; the derived runtime names this exact boundary and aborts rather than dispatching unported behavior.
- where: guest FUN_80031F54; EnterElectroRuntime::bootInit
- gap: Reverse-engineer and execute the title own gameMain initialization without copying SLUS_008.75 addresses or hooks.
- notes: The abort is the current success condition. Rendering, widescreen, runtime modules, and gameplay remain downstream and unclaimed.
