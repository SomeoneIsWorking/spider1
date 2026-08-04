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
- status: re-verified
- deps: RE-03d
- evidence: movie descriptor table at 0x80097DEC (24-byte stride) read from the indexing routine 0x8002B0F4; entry 0 frameBytes 0x25800 = 320*240*2 is self-consistent
- where: 
- gap: 
- notes: 

### RE-05 — Input buffers + the host-turn seam
- status: re-verified
- deps: RE-03
- evidence: pad buffers confirmed 3 ways (PadInitDirect args 0x22 apart at 0x8006AE34; consumer 0x8006B27C walks stride 0x22; libpad itself inits that range at runtime). Host turn: title wait terminates, pad writes 4 -> 1660
- where: 
- gap: 
- notes: 

### RE-12 — Per-frame OT / packet-pool layout
- status: todo
- deps: RE-03
- evidence: 
- where: 
- gap: 
- notes: 

### RE-13 — Scheduler task layout
- status: todo
- deps: RE-03
- evidence: 
- where: 
- gap: 
- notes: 

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
- status: re-verified
- deps: RE-04
- evidence: DELIVERY is byte-correct and re-verified. CdlSetloc 28:32:54 -> LBA 128304 == CINEMAS/ATVILOGO.STR and 62:15:25 -> LBA 280000 == CINEMAS/LOGO.STR (discdump list); MDEC DMA0 word counts 1824 and 1440 equal those files' BS-header word counts read off the disc. THE RING DEADLOCK IS FIXED (2026-08-05, claim C017). Root cause was NOT what the first write-up said: DPCR (0x1F8010F0) and DICR (0x1F8010F4) were unmodelled in the framework, so the runtime signalled the guest's DMA-completion callback 0x8008DB44 on EVERY DMA3 transfer. libcd DMACallback (0x8009152C) arms DICR bit 16+ch plus the master bit 23; libstr's DMA starter FUN_80085948 read-modify-writes the BYTE at DICR+2 per transfer; FUN_80085000 arms it for the LAST chunk of an STR frame only. So the frame-ready marker was written once per SECTOR, slots 1..7 carried a spurious status 2, and the producer refused to overwrite slot 7. A/B on ONE binary toggling only the gate: BEFORE the ring wedges at frameStart=7 cons=9 writeIdx=7 status "0 2 2 2 2 2 2 2 0..." over 42 decimated samples and intro presents are 0.00% non-black / 1 colour; AFTER the ring never wedges (1 sample, writeIdx=1 status "3 0 0..."), dmairq shows 9 chunks armed=0 then the 10th armed=1, TWO full STR frames arrive per movie, and intro presents are 40.62% / 140 colours. Menu unaffected: f2400 99.44% / 1120 colours, f4000 83.70% / 1315, 0 abort, 0 recomp-MISS. The ring has 48 slots (DAT_800C1520 read at runtime) — the earlier "cons=9 is past the end" was a bad inference and C016 is corrected. Logs: scratch/logs/re07_before.log, re07_after.log, re07_gate.log. Hermetic gate: external/psxport/tests/test_dma_irq_gate.cpp, 7 cases / 30 checks, shown RED 0/7.
- where: external/psxport/runtime/recomp/dma_irq.h (NEW, DPCR/DICR rules), mem.cpp (register state + the DMA3 completion gate), hle.cpp (flag acknowledge at the dispatch that stands in for the BIOS DMA handler); game/core/cd_stream.cpp (ring diagnostic only — its pump was NOT the fault); guest: StGetNext=FUN_80086B10, StFreeRing=FUN_800872AC, producer=FUN_80085000, DMA starter=FUN_80085948, frame-ready cb=FUN_8008DB44, DMACallback=0x8009152C; ring state DAT_800C1510 base / 0x800C1514 writeIdx / 0x800C1518 frameStart / 0x800C151C cons / 0x800C1520 slot count (=48)
- gap: RESOLVED 2026-08-05 — BOTH intro logo movies play. Read off the PPMs of a 180 s headless run: frame 120 = 320x240 24-bit @0,256, 99.95% non-black / 11395 colours (the ACTIVISION logo, confirmed on the PNG); frame 300 = 25.70% / 8773 (the Neversoft eyeball); menu unchanged at f4000/6000/8000 = 99.44% / 1061-1105. 0 abort, 0 fatal, 0 recomp-MISS. Two further causes beyond the DICR gate, both real: (a) RE-16, the recompiler unwound DecDCTvlc after one block (see RE-16); (b) the framework announced DMA completions to the guest on CHANNEL 3 ONLY. The player registers an MDEC-OUT callback at init — 0x8002B1FC calls FUN_80085BC0(0x8002B28C) -> DMACallback(1,...) through the BIOS thunk 0x8008B89C, stored into libcd's per-channel table 0x800B4388+4*ch — and 0x8002B28C is what LoadImages each 16-px strip into VRAM and advances the destination x at gp+0x6D4, the value the player's own dsx wait at 0x8002AD64 spins on. PSXPORT_FNTRACE=0x8002B28C reported NEVER CALLED over a whole run (denominator: registrar reached 4 times, movie loop 4653 iterations) while the guest's DICR was 0x009A0000 = master + channels 1, 3 and 4. That also settles C018's two surviving suspects: the dsx wait was the exit, because nothing ever moved gp+0x6D4. A fourth cause sat behind it — these callbacks CHAIN (the handler starts the next strip, which completes while it is still running), and taking the pending flag before declining the dispatch under `in_irq` dropped it, ending the chain after two strips per movie. 0x8002B28C calls: 0 -> 2 -> 8420. Logs: scratch/logs/re07b_fix4.log, re07b_gate.log, re07b_fix3c.log; shots scratch/screenshots/re07_fmv_120.png, re07_fmv_300.png, re07_menu_4000.png.
- where-2: external/psxport/runtime/recomp/dma_irq.h (DmaDone per-channel pending set + dma_callback_slot), mem.cpp (all six completion points), hle.cpp (irqPoll scans 7 channels; a completion is consumed only when delivered), game_iface.h (cdDmaDoneCbPtr -> dmaCallbackTable), game/core/game_config.cpp (0x800B4388); guest: FMV player init FUN_8002B1FC, MDEC-out callback 0x8002B28C, DecDCTout 0x80085BA0 -> FUN_80085D64, BIOS DMACallback thunk 0x8008B89C
- notes: Issue #4 has the full write-up, with the superseded attribution kept visible. GameConfig::bootFmv is empty and PSXPORT_NO_FMV is a provable no-op for this port, so the framework's own FMV player is not involved; this is entirely the guest's path. No hack was added — the hack list stays empty. New instruments: PSXPORT_DEBUG=dmairq (I010) and the corrected PSXPORT_DEBUG=ring (I011, now sized by the guest's own slot count). fntrace_init() is now wired into game/core/game_hooks.cpp (it never was), so PSXPORT_FNTRACE works in this port.

### RE-08 — Render: GTE tap -> native depth
- status: todo
- deps: RE-09
- evidence: 
- where: 
- gap: 
- notes: 


## module-loader

### HACK-02 — One-slot module pinning: all 30 CD.WAD modules share base 0x800C65EC
- status: re-verified
- deps: 
- evidence: RESOLVED 2026-08-04. The 30 CD.WAD modules are emitted BASE-RELATIVE and the guest allocator places each body, as the console does. Boot: before = SIGSEGV at the first multi-module load with recomp-MISS 0x800C6684, 0 frames presented; after = 7176 frames in 120s (~60fps), 0 misses, with L5A5LSC (0x8014A6D0), LIZMAN (0x801BDA30) and VENOM (0x801C6238) simultaneously live at three distinct bases (scratch/logs/boot_after.log, scratch/logs/frames_after.log). Emitter gate: emit.py's HI16-consumer check examined 911 sites / ~800 uses across all 30 modules and found 0 escapes of a raw high half. Framework gate: external/psxport/tests/test_overlay_reloc.cpp, 6/6 tests, 41 checks — shown RED first (5/6 failing) with the old static-range routing.
- where: game/core/module_loader.cpp, external/psxport/tools/recomp/emit.py (ModuleReloc), external/psxport/runtime/recomp/overlay_router.cpp
- gap: 
- notes: The slot reservation, free()'s ownership refusal, warn_if_coresident and overlay_set_resident are DELETED, not disabled. The invariant that replaced the co-residency warning is a real gate: overlay_place() aborts if two live modules' ranges overlap. KNOWN CONSEQUENCE, recorded rather than discovered later: a HI16-relocated lui's INTERMEDIATE register value is hi(link)+delta where the console holds hi(live) — identical once a low half is added (which the consumer gate proves always happens), but not bit-identical in the register itself, so an SBS register-level compare over module code would report that as a divergence.

