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
- status: in-progress
- deps: RE-15
- evidence: 
- where: 
- gap: 
- notes: 

### RE-07 — Intro FMV / front-end
- status: in-progress
- deps: RE-04
- evidence: The boot plays TWO movies and both are correctly located and correctly fed. CdlSetloc 28:32:54 -> LBA 128304 == CINEMAS/ATVILOGO.STR and 62:15:25 -> LBA 280000 == CINEMAS/LOGO.STR (discdump list). MDEC DMA0 word counts 1824 and 1440 equal the two files' BS-header word counts read off the disc. Each movie gets exactly ONE DecDCTout (2880 words = 15 MBs for 320x240, 2304 = 12 MBs for 320x192, i.e. one 16px macroblock column at 24bpp) and then stops; over a 20337-frame run there are exactly two MDEC decode attempts and no video ever reaches VRAM. PSXPORT_DEBUG=ring then shows the deadlock held for millions of calls: prod=7 cons=9 d1514=7, slots 0 2 2 2 2 2 2 2 0 0 0 0. StFreeRing sets cons = index + chunks-in-frame; LOGO.STR frame 1 spans 9 chunks so cons=9, and slot 9 is 0 (neither 2=ready nor 1=wrap), so StGetNext returns not-ready forever. Logs: scratch/logs/cd_probe.log, mdec_probe.log, ring_probe.log. Ghidra project: tools/redump_ram.py then tools/ghidra_import.sh.
- where: game/core/cd_stream.cpp (StGetNext override, 0x80086B10); guest libstr: StGetNext=FUN_80086b10, StFreeRing=FUN_800872ac, sector-arrival producer=FUN_80085000; ring state at DAT_800c1510 (base), 0x800C1514 (producer write idx), 0x800C1518 (prod), 0x800C151C (cons), 0x800C1520 (slot count)
- gap: The port pumps STR sectors from the CONSUMER side (cd_stream.cpp calls cd.pumpStream() inside the StGetNext override) instead of driving the guest's own sector-arrival handler FUN_80085000, which is the only writer of the ring's wrap marker (status 1) and of the producer index. So the ring's invariants are never maintained and the two indices desynchronise. Proper fix: deliver STR sectors through the guest's CD data-ready path at drive pace. NOT to be papered over by forcing cons to wrap, which would corrupt frame assembly and is a hack.
- notes: Issue #4 has the full write-up. GameConfig::bootFmv is empty and PSXPORT_NO_FMV is a provable no-op for this port (measured: identical MDEC trace with PSXPORT_NO_FMV=0), so the framework's own FMV player is not involved; this is entirely the guest's path.

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

