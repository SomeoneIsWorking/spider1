// game_config.cpp — the Spider-Man (SLUS_008.75, USA) GameConfig: the guest-address literals the
// PSX-generic framework reads through `c->cfg->field`.
//
// EVERY value here is REVERSE-ENGINEERED from the retail executable and cited with the instruction it
// came from. Nothing is guessed. Fields whose RE has NOT been done are left ZERO and are tracked as
// open steps in docs/re-frontier.md — a zero here means "not yet RE'd", never "not needed". Do not
// fill one in to make something run; that is exactly the jump-ahead the frontier tracker exists to
// catch.
//
// Provenance: disassembly of the retail US executable, entry 0x8008739C. Reproduce with
//   python3 tools/redump_ram.py            # SLUS_008.75 -> a 2 MB RAM image
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8008739C 0x80087440
#include "game_iface.h"
#include "overlay_table.h"   // generated: REC_MAIN_LO / REC_MAIN_HI (this game's recompiled .text range)

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The crt0 (0x8008739C) — the standard Sony crt0, and the ONE group that is fully RE-verified.
//
// The framework's generic crt0_setup (runtime/recomp/native_boot.cpp) reproduces this shape exactly:
// BSS-zero loop, sp from a stack-top global minus 8, heap base = end-of-BSS masked to a physical
// address, heap size = (sp - <size global>) - heapBase, then InitHeap with a0 = heapBase|KSEG0 + 4.
// Spider-Man's crt0 is that same sequence instruction for instruction, so the mapping below is a
// structural match, not an approximation:
//
//   800873AC  sw   $zero,($v0)          BSS-zero  0x800B5994 .. 0x800C65D4   -> bssZeroLo/bssZeroHi
//   800873C4  lw   $v0,0x3E70($v0)      stack-top global                     -> stackTopBase
//   800873EC  lw   $v1,0x3E6C($v1)      stack-size global (subtracted)       -> stackTopBase2
//   800873DC  addiu $a0,$a0,0x65D4      heap base = end of BSS               -> heapBase
//   80087400  sw   $a1,0x1240($at)      heap SIZE store                      -> heapSizePtr
//   8008740C  sw   $a0,0x123C($at)      heap BASE store                      -> heapBasePtr
//   8008741C  addiu $gp,$gp,0x47F4      global pointer                       -> gp
//   80087424  jal  0x8008DC98           BIOS A(39h) InitHeap stub            -> libcInit
//   80087438  jal  0x8002C354           main()                               -> gameMain
// ─────────────────────────────────────────────────────────────────────────────────────────────────
static const GameConfig g_spiderman_cfg = {
    // --- crt0 / boot ---------------------------------------------------------------- RE-VERIFIED --
    /* bssZeroLo      */ 0x800B5994u,
    /* bssZeroHi      */ 0x800C65D4u,
    /* stackTopBase   */ 0x800B3E70u,
    /* stackTopBase2  */ 0x800B3E6Cu,
    /* heapBase       */ 0x800C65D4u,
    /* heapSizePtr    */ 0x800B1240u,
    /* heapBasePtr    */ 0x800B123Cu,
    /* gp             */ 0x800B47F4u,
    /* libcInit       */ 0x8008DC98u,   // BIOS A(39h) InitHeap stub
    /* gameMain       */ 0x8002C354u,
    /* crt0           */ 0x8008739Cu,   // PS-X EXE header entry pc

    // --- recompiled MAIN .text range (physical) ------------------------- from the recompiler run --
    // Sourced from the generated header so these can never drift from the substrate they describe.
    /* recMainLo      */ REC_MAIN_LO,   // 0x00010000  (PS-X EXE load address)
    // recMainHi is DELIBERATELY the heap base, not REC_MAIN_HI (= load + text size = 0x000C6800).
    //
    // The PS-X EXE loads 0xB6800 bytes, but crt0 sets heapBase to the end of BSS, 0x000C65D4 — so the
    // last 0x22C bytes of the loaded image ARE THE HEAP at runtime and are overwritten by the first
    // allocation. They are file padding, not code: the recompiler found 36 "functions" up there
    // (0xC661C..0xC67FC) purely by scanning bytes it had no reason to believe were text.
    //
    // Leaving the range at 0x000C6800 makes rec_dispatch claim heap addresses for MAIN. That is not
    // hypothetical — it broke RE-09: the module slot is the heap's first block (0x000C65EC), so every
    // call into a runtime-loaded module was routed to MAIN's switch, found no function, and failed as
    // a dispatch miss WITHOUT ever consulting the overlay router. The router was correct and the
    // residency was right; the address never reached it.
    //
    // A call that genuinely lands in the padding now fails loudly rather than silently resolving to a
    // padding-derived phantom, which is the better failure of the two.
    /* recMainHi      */ 0x000C65D4u,   // = heapBase; see above. NOT REC_MAIN_HI (0x000C6800).

    // --- disc key ------------------------------------------------------- this port's own env name --
    // The framework's disc resolver used to hardcode the FIRST consumer's variable, so run.sh set
    // PSXPORT_SPIDERMAN_DISC, nothing read it, and every boot ran with NO MEDIA behind an ordinary
    // -looking log. Not RE — a port fact — but it belongs here because the framework must not know it.
    /* discEnvVar     */ "PSXPORT_SPIDERMAN_DISC",

    // --- boot intro movies ------------------------------------------ deliberately EMPTY, and why --
    // Not a gap. This port's boot runs on the recompiled substrate, so movies are played by the
    // GUEST, not by the framework's boot sequencer. The framework used to hardcode the reference
    // consumer's MOVIE/LOGO.STR here, which does not exist on this disc.
    //
    // This game's movies ARE reverse-engineered: a 24-byte-stride descriptor table at 0x80097DEC,
    //   { +0 const char* path, +4 u16 width, +6 u16 height, +8 u16 frames, +0xC u32 frameBytes,
    //     +0x10 u8 flag }
    // indexed by a movie ID. Read from the indexing routine at 0x8002B0F4, which masks the ID to a
    // byte and computes ID*24 (`sll 1; addu; sll 3`) against that base, then loads +4/+6/+8/+0xC/
    // +0x10. Self-consistent: entry 0 is CINEMAS/ATVILOGO.STR at 320x240 with frameBytes 0x25800 =
    // 320*240*2, entry 1 CINEMAS/LOGO.STR at 320x192 with 0x1E000 = 320*192*2.
    //
    // What is NOT established is which ID the boot plays: both callers of 0x8002B0F4 (0x8002AAB8 and
    // 0x8003D2EC) pass the index in a REGISTER, not as a constant, and the port stalls in CdInit
    // long before any movie is reached, so it cannot be observed either. Naming one here would be a
    // guess wearing a citation. That is RE-07.
    /* bootFmv        */ { nullptr, nullptr, nullptr, nullptr },

    // --- everything below: NOT YET REVERSE-ENGINEERED --------------------------------------------
    // Zero is the honest value. Each group is an open step in docs/re-frontier.md; the framework
    // consumers of these fields (native_step_frame's OT/packet-pool dance, PcScheduler, the CD
    // chokepoint overrides, the pad driver) are correspondingly NOT wired for this game yet — the
    // Phase-0 boot runs the guest's own main() on the substrate instead. See docs/codemap.md.

    // --- per-frame OT / packet pool (re-frontier: RE-02) ---
    /* otRegionBase   */ 0, /* otRegionStride */ 0,
    /* packetPoolBase */ 0, /* packetPoolStride */ 0,
    /* otBasePtr      */ 0,
    /* dwellCounter   */ 0,
    /* poolPtrCur     */ 0, /* poolPtrLast */ 0,
    /* clearOtagR     */ 0, /* putDrawEnv */ 0, /* drawSync */ 0,
    /* irqEventClasses*/ {0, 0, 0},
    /* dualviewRenderOrch */ 0, /* dualviewSubmit */ 0,

    // --- scheduler task layout (re-frontier: RE-03) ---
    /* taskTableBase  */ 0, /* taskSlotStride */ 0, /* taskCount */ 0,
    /* curTaskPtr     */ 0,
    /* stageStart     */ 0, /* stageDemo */ 0, /* stageGame */ 0,

    // --- overlay router slots -------------------------------------------------- N/A for this game --
    // ONE overlay slot, and it is the port's own (RE-09). The disc carries no per-stage .BIN images
    // — `discdump list` shows only SLUS_008.75 plus the packed archive CD.WAD, which is what the
    // comment here used to cite as proof that this game "has no overlay modules". That was a claim
    // about the ISO layout being read as a claim about code coverage: the game loads 30 further code
    // modules OUT of CD.WAD as <name>.bin + <name>.rel pairs at runtime.
    //
    // Those modules are relocatable, and the port pins every one of them to a single reserved slot
    // (game/core/module_loader.cpp) so the recompiler can emit them. Only one is ever resident. The
    // base is the heap's first block, 0x800C65EC — reserved before any guest code runs, and re-checked
    // at runtime against the base the substrate was actually emitted for.
    //
    // Residency is recorded EXACTLY, by name, from the loader — not by content signature. Pinning the
    // modules to one base makes their signatures collide (30 modules -> 14 distinct 32-byte
    // signatures; 12 share one), because the entry prologues are identical boilerplate and relocating
    // them to the same address makes the words identical too. See overlay_router.h.
    /* overlaySlots   */ {{0x800C65ECu, "MODULE"}, {0, nullptr}, {0, nullptr}},

    // --- CD chokepoints ---------------------------------------- deliberately EMPTY, and why --
    // This game runs STOCK Sony libcd (rcsids in the binary: `bios.c` and `intr.c v1.75` at
    // 0x800966B8 / 0x80096450), which is REGISTER-LEVEL code, not a bespoke engine loader:
    //   CD_cw        0x8008CE8C  drives 0x1F801800-03 via its own pointer table at 0x800B3DD8
    //   CD_init      0x8008D4E4  claims IRQ2 through InterruptCallback (0x8008BBD0) and writes
    //                            I_MASK directly at 0x8008BCF0 (via *0x800B3914 = 0x1F801074)
    //   CD_getsector 0x8008D82C  fetches sector data by DMA3 (*0x800B3E14/18/1C =
    //                            0x1F8010B0/B4/B8 — verified in the load image)
    // The framework's CDC register model, DMA3 channel and interrupt delivery serve all of that
    // directly, so this port needs NO CD chokepoint and no game address in the framework.
    //
    // STATE OF PLAY, 2026-07-28. cdCommand = 0x8008CE8C (CD_cw) is set because it is the BEST KNOWN
    // state: with it, CdInit passes and the boot reaches the CD read path. Without it the boot
    // regresses to CdInit failing outright.
    //
    // It is nonetheless the WRONG long-term answer and is tracked as such: an ACK moves no data, so
    // reads stall one stage later. The right design is to let the guest's own register-level libcd
    // run against the framework's CDC model (DMA3 + per-sector INT1 now exist for exactly that).
    // That path is blocked on ONE measured thing: interrupt delivery declines with irq_enabled = 0,
    // i.e. the guest is left inside a critical section it never exits. Implementing COP0 Status
    // (psxport, same session) did not clear it, so the restore is happening by some route still not
    // modelled. Find that, then set this back to 0. See docs/re-frontier.md RE-03.
    /* cdInit         */ 0, /* cdCommand */ 0x8008CE8Cu,
    // cdSync: 0x80086C60, stock libcd's CdSync(mode, result) — a thin wrapper over 0x8008CBC4. The
    // CD streaming poller at 0x80085000 calls it in a loop and only proceeds when the result is not
    // 5 (disk error); the framework handler reports complete, which is true here because every CD
    // operation this port performs has already finished synchronously by the time it is asked.
    /* cdSync */ 0x80086C60u, /* cdReadPrim */ 0,
    /* cdFileLoad     */ 0, /* cdAsyncRead */ 0,
    /* voicePlay      */ 0, /* voiceStop */ 0, /* lastSectorTracker */ 0,
    /* cdInlineLoad   */ 0,
    /* cdCmdStream    */ 0,
    /* cdCallbackTable*/ {0, 0, 0, 0},
    /* cdCallbackFn   */ {0, 0, 0, 0},

    // cdGetSector: 0x8008D82C, stock libcd's CdGetSector(a0 = dest, a1 = words). RE'd from its
    // decompiled body (tools/ghidra_query.py func 0x8008D82C): it programs DMA3 — MADR from a0,
    // BCR from a1|0x10000, through the pointer globals *0x800B3E14/18/1C = 0x1F8010B0/B4/B8 —
    // spins until the CD status bit 0x40 says data is ready, writes CHCR 0x11000000 to start, then
    // spins until the busy bit clears. Its only caller is the wrapper 0x80087084, which returns
    // (result == 0).
    //
    // The PC owns this outright. Every instruction of it is hardware ceremony around one fact: move
    // N words of the current sector into this buffer. The native handler does the move, from the
    // real disc image, and skips the handshake, the DMA and the wait entirely.
    /* cdGetSector    */ 0x8008D82Cu,

    // cdReadyCbPtr: 0x800B3B18, the guest global that stock libcd's CdReadyCallback() writes.
    // Identified from its setter at 0x80086C94, which is the classic get-and-set shape
    // (`old = g; g = param; return old`) — the pair at 0x80086C80 does the same for the SYNC
    // callback at 0x800B3B14.
    //
    // This is what lets the PC own the read outright. The game's handler (0x800899A0) is a per-sector
    // driver: it calls CdGetSector, advances its own destination, decrements its remaining count, and
    // when the count hits zero restores the callbacks and issues Pause. So a read completes by
    // invoking that callback once per sector — no CD interrupt, no ISR chain, no busy-wait.
    /* cdReadyCbPtr   */ 0x800B3B18u,

    // cdLastPosBuf: 0x800B3B2C, the buffer CdLastPos() (0x80086C00) returns. Written in exactly ONE
    // place in the whole image — inside CD_cw at 0x8008CE8C, which is the routine cdCommand above
    // replaces. On Setloc it copies the 4-byte position parameter here; on Setmode it stores the
    // mode byte at +4 (0x800B3B30).
    //
    // That is why every read was being rejected: the read-setup path at 0x80089CE4 seeds its
    // expected-sector counter with CdPosToInt(CdLastPos()), so with the record skipped it compared
    // the sector header against stale bytes and reported "CdRead: sector error" forever.
    /* cdLastPosBuf   */ 0x800B3B2Cu,

    // cdReadStock / cdReadSync: 0x80089ECC and 0x8008A068 — stock libcd's CdRead(sectors, buf, mode)
    // and CdReadSync(mode, result). Identified from their decompiled bodies and their pairing: the
    // same three game routines (0x800649E4, 0x80064DA4, 0x80086A6C) call one then the other.
    // CdRead's argument mapping is explicit in its own code — param_1 -> sector count, param_2 ->
    // buffer, param_3 -> mode — and the mode picks 0x200/0x249/0x246 words per sector, which the
    // native handler mirrors.
    //
    // Overriding at this level is what makes the CD fully PC-owned: the per-sector callback loop,
    // the drive-position check, the vblank timeout and the retry path all stop running.
    /* cdReadStock    */ 0x80089ECCu,
    /* cdReadSync     */ 0x8008A068u,

    // cdSearchFile: 0x80086170, stock libcd's CdSearchFile(CdlFILE*, name). Identified from the boot
    // loop at 0x800649E4, which calls it with the literal "\CD_HED;1" and loops re-running CdInit
    // for as long as it returns 0 — that loop is the boot stall. Its result then feeds
    // CdControl(CdlSetloc, &loc) and CdRead(ceil(size/2048), buf, 0x80), which confirms the CdlFILE
    // layout: position at +0, size at +4.
    /* cdSearchFile   */ 0x80086170u,

    // dmaCallbackTable: 0x800B4388 — libcd's per-channel DMA-callback table. Read out of the
    // registrar at 0x8009152C, which computes `0x800B4388 + index*4` and stores its argument there.
    // Everything the guest registers goes through it, via the BIOS thunk 0x8008B89C:
    //
    //   slot 3 (CD)       <- 0x8008DB44, from the streaming setup at 0x80086030. Promotes a ring slot
    //                        from "DMA in flight" (3) to "ready" (2). Measured: the ring sat with ten
    //                        slots at 3 while the consumer, which only accepts 2, spun on a full ring.
    //   slot 1 (MDEC-out) <- 0x8002B28C, from the intro FMV player's init 0x8002B1FC
    //                        (FUN_80085BC0 -> DMACallback(1, ...)). This is what uploads a decoded
    //                        strip to VRAM. It was NEVER CALLED over a whole run while the port
    //                        signalled channel 3 only — measured with PSXPORT_FNTRACE, denominator
    //                        4653 movie-loop iterations, and that is why the movies decoded but
    //                        stayed invisible (RE-07).
    //
    // The guest's own DICR value confirms which channels it wants: 0x009A0000 = master + 1, 3, 4.
    /* dmaCallbackTable */ 0x800B4388u,

    // --- pad driver (re-frontier: RE-05) ---------------------------------------------------------
    // The game uses STOCK libpad in direct mode. Pad init 0x8006AE34 ends with
    //
    //     FUN_8008afbc(0x800A50EC, 0x800A510E);   // PadInitDirect(buf_slot0, buf_slot1)
    //     FUN_8008ad08();                          // PadStartCom
    //
    // and the two arguments are exactly 0x22 apart — the 34-byte libpad direct buffer. Disassembly:
    //   tools/redump_ram.py && external/psxport/tools/disasm.py scratch/bin/ram.bin 0x8006AE34 0x8006AE90
    //
    // The per-frame consumer 0x8006B27C confirms the same layout INDEPENDENTLY: it walks 2 slots at
    // stride 0x22 from 0x800A50EC, tests byte +1 against 0x80 (libpad's multitap type nibble), and on
    // the ordinary-controller path copies 8 bytes from the buffer BASE — i.e. {status, type, btn_lo,
    // btn_hi, …}, which is precisely the framework's fillBuffer packet. On the multitap path it
    // instead copies four sub-pads from +2/+10/+18/+26, so +2 is a sub-record, NOT the slot base.
    //   external/psxport/tools/disasm.py scratch/bin/ram.bin 0x8006B27C 0x8006B3C8
    //
    // That resolves the base-vs-+2 ambiguity recorded against CLAIM-07: the framework's buf[0] is the
    // status byte, so the buffer base is 0x800A50EC. 0x800A50EE is only its button halfword, and
    // 0x800A5130 is the game's own per-frame mirror (the 62,114 writes from 0x8006B3C8 that falsified
    // the earlier guess) — writing either from the port would be wrong.
    //
    // padDriverFn is left at zero because the FRAMEWORK NEVER READS IT: the field is declared in
    // game_iface.h and consumed nowhere; its intended handler (pad_input.cpp `pad_read`) is a static
    // function that overridesInit() does not register. Recorded as a wart, not a gap in this port's
    // RE — see docs/issues/framework-agnosticism-warts.md.
    /* padSlot0Buf    */ 0x800A50ECu, /* padSlot1Buf */ 0x800A510Eu, /* padDriverFn */ 0,
    /* padSlotPtrTable*/ 0,
    // Byte distance between consecutive slots' pointers. 0 is read as 4 by the framework, which is
    // the correct default; stated explicitly so this initialiser lists every field the struct has.
    /* padSlotPtrStride*/ 0,

    // --- platform HLE: the PSX hardware-sync primitives -------------------------------------------
    /* hle */ {
        // The address window PlatformHle::register_() accepts. Its job is to keep GAME/engine logic
        // out of the hardware-sync table, so both bounds are evidence-based:
        //
        //   LOWER 0x80083000 — below the lowest observed SDK kernel-call stub (0x80083EC8) and well
        //     above the highest observed game-logic address (main 0x8002C354, 0x800649E4, 0x8006BF9C).
        //     Evidence: scanning the text for the SDK's BIOS-call stub idiom
        //     (`addiu $t2,$zero,0xA0/0xB0/0xC0 ; jr $t2`) finds 41 of them, ALL within
        //     0x80083EC8..0x80091730 — that cluster is the SCEI library text.
        //   UPPER 0x80096000 — the end of library text: the first address known to be .rodata is
        //     0x80096020, the "VSync: timeout" string that identified VSync in the first place.
        //
        // Provisional in the sense that it may need widening as more of the library is RE'd — but it
        // fails LOUDLY (a REFUSED diagnostic) rather than silently, so widening is evidence-driven.
        /* windowLo */ {0x80083000u, 0},
        /* windowHi */ {0x80096000u, 0},

        // codeScanLo/Hi left zero on purpose: the framework then falls back to [recMainLo, recMainHi),
        // which is exactly right here — this game has no overlays, so the recompiled MAIN text IS the
        // entire resident code range.
        /* codeScanLo */ 0, /* codeScanHi */ 0,

        // NOT YET REVERSE-ENGINEERED — zero means the framework installs no handler, and the guest
        // will spin in the real primitive if it reaches one. That is the honest signal, and it is
        // precisely what currently stops the boot (docs/issues/boot-stalls.md STALL-03).
        /* decDctInSync    */ 0, /* decDctOutSync */ 0,   // libmdec       (re-frontier: RE-07)
        /* cdReadSync      */ 0, /* cdDataSync    */ 0,   // libcd         (re-frontier: RE-03)
        /* cdInitHandshake */ 0,                          // libcd         (re-frontier: RE-03)
        /* gpuTimeoutArm   */ 0, /* gpuTimeoutCheck */ 0, // libgpu        (re-frontier: RE-04)
        /* gpuTimeoutDeadlineVar */ 0, /* gpuTimeoutFlagVar */ 0,
        /* changeThread    */ 0,                          // kernel yield  (re-frontier: RE-05)

        // vsyncTrap stays ZERO, and that is a deliberate statement rather than a gap. The trap means
        // "nothing may reach VSync because the native frame loop owns all timing" — untrue here. This
        // port runs the guest's own loop on the substrate, so it REIMPLEMENTS VSync faithfully and
        // registers that handler itself (game/core/sync_native.cpp). Setting both would let
        // initBuiltins clobber the real implementation with an abort.
        /* vsyncTrap */ 0,
    },

    // --- present policy -------------------------------------------------------------------------
    // preserveVramBackdrop = 1, because THIS PORT STILL RUNS THE GUEST'S OWN DRAWING CODE, which is
    // exactly the condition the field's own documentation names ("Set to 1 while the guest still owns
    // drawing"): an upload into the display area IS visible on hardware, so upload-only screens —
    // loading screens, fades, static art that submits no primitives — must not be cleared away.
    //
    // HONESTY NOTE, because this was tried as a fix and is NOT one: it does not address the 30 Hz
    // full-scene/black flicker. That flicker came from presents being paced by the display field
    // clock while this game builds one ordering table per TWO fields, so every other present rebuilt
    // the composite from an empty batch. Setting this flag only skips render_geom's CLEAR, while
    // upload_vram still overwrites the composite with guest VRAM — which for a natively-compositing
    // port is empty, so the frame came out black regardless. Measured: unchanged at 0.0/99.4/0.0/
    // 99.4/0.0/99.4% across six consecutive presents.
    //
    // The real fix is in the framework: a present carrying no new geometry now re-shows the last
    // composite instead of rebuilding one (gpu_vk.cpp, geom_batch_empty), which is what hardware does
    // — the display re-scans the same persistent framebuffer every field. After that, the same six
    // presents are 99.4% each.
    /* preserveVramBackdrop */ 1,

    // --- memory card ----------------------------------------------------------------------------
    // This port's own env key and backing file. The framework's resolver used to hardcode the FIRST
    // consumer's key and filename, so without these a Spider-Man save would land in the reference
    // game's card file. Exactly the defect discEnvVar exists to prevent, in a second subsystem.
    /* cardEnvVar      */ "PSXPORT_SPIDERMAN_CARD",
    /* cardDefaultPath */ "scratch/saves/spiderman.mcr",

    // GameConfig is initialised POSITIONALLY here, so these trail in struct order. paceQuota 0 is
    // what the implicit zero-init already gave; it is spelled out so windowTitle can follow it.
    // The framework must not name a game — gpu_vk.cpp hardcoded Tomba!2's title, so this port
    // announced itself as Tomba!2 in the title bar (psxport 53916f0d).
    /* paceQuota       */ 0u,
    /* windowTitle     */ "Spider-Man",
};

extern void spiderman_install_game_hooks();   // game/core/game_hooks.cpp

void spiderman_install_game_config() {
  extern const GameHooks* spiderman_game_hooks();
  psxport_install_game(&g_spiderman_cfg, spiderman_game_hooks());
}
