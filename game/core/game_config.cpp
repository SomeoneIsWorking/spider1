// game_config.cpp — measured Spider-Man (SLUS_008.75, USA) compatibility facts.
//
// SpiderRuntime is the title's ownership seam. This legacy GameConfig remains only because generic
// psxport algorithms still read `c->cfg->field`; each typed framework extraction must delete its
// corresponding fields here rather than growing this bag.
//
// EVERY value here is REVERSE-ENGINEERED from the retail executable and cited with the instruction
// it came from. Nothing is guessed. Fields whose RE has NOT been done are left ZERO and are tracked
// as open steps in docs/re-frontier.md — a zero here means "not yet RE'd", never "not needed". Do
// not fill one in to make something run; that is exactly the jump-ahead the frontier tracker exists
// to catch.
//
// Provenance: disassembly of the retail US executable, entry 0x8008739C. Reproduce with
//   python3 tools/redump_ram.py            # SLUS_008.75 -> a 2 MB RAM image
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8008739C 0x80087440
#include "game_iface.h"
#include "legacy_game_interface.h"
#include "overlay_table.h" // generated: REC_MAIN_LO / REC_MAIN_HI (this game's recompiled .text range)

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The crt0 (0x8008739C) — the standard Sony crt0, and the ONE group that is fully RE-verified.
//
// The framework's generic crt0_setup (runtime/recomp/native_boot.cpp) reproduces this shape
// exactly: BSS-zero loop, sp from a stack-top global minus 8, heap base = end-of-BSS masked to a
// physical address, heap size = (sp - <size global>) - heapBase, then InitHeap with a0 =
// heapBase|KSEG0 + 4. Spider-Man's crt0 is that same sequence instruction for instruction, so the
// mapping below is a structural match, not an approximation:
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
    // --- crt0 / boot ---------------------------------------------------------------- RE-VERIFIED
    // --
    /* bssZeroLo      */ 0x800B5994u,
    /* bssZeroHi      */ 0x800C65D4u,
    /* stackTopBase   */ 0x800B3E70u,
    /* stackTopBase2  */ 0x800B3E6Cu,
    /* heapBase       */ 0x800C65D4u,
    /* heapSizePtr    */ 0x800B1240u,
    /* heapBasePtr    */ 0x800B123Cu,
    /* gp             */ 0x800B47F4u,
    /* libcInit       */ 0x8008DC98u, // BIOS A(39h) InitHeap stub
    /* gameMain       */ 0x8002C354u,
    /* crt0           */ 0x8008739Cu, // PS-X EXE header entry pc

    // --- recompiled MAIN .text range (physical) ------------------------- from the recompiler run
    // --
    // Sourced from the generated header so these can never drift from the substrate they describe.
    /* recMainLo      */ REC_MAIN_LO, // 0x00010000  (PS-X EXE load address)
    // recMainHi is DELIBERATELY the heap base, not REC_MAIN_HI (= load + text size = 0x000C6800).
    //
    // The PS-X EXE loads 0xB6800 bytes, but crt0 sets heapBase to the end of BSS, 0x000C65D4 — so
    // the
    // last 0x22C bytes of the loaded image ARE THE HEAP at runtime and are overwritten by the first
    // allocation. They are file padding, not code: the recompiler found 36 "functions" up there
    // (0xC661C..0xC67FC) purely by scanning bytes it had no reason to believe were text.
    //
    // Leaving the range at 0x000C6800 makes rec_dispatch claim heap addresses for MAIN. That is not
    // hypothetical — it broke RE-09: the module slot is the heap's first block (0x000C65EC), so
    // every
    // call into a runtime-loaded module was routed to MAIN's switch, found no function, and failed
    // as
    // a dispatch miss WITHOUT ever consulting the overlay router. The router was correct and the
    // residency was right; the address never reached it.
    //
    // A call that genuinely lands in the padding now fails loudly rather than silently resolving to
    // a
    // padding-derived phantom, which is the better failure of the two.
    /* recMainHi      */ 0x000C65D4u, // = heapBase; see above. NOT REC_MAIN_HI (0x000C6800).

    // --- disc key ------------------------------------------------------- this port's own env name
    // --
    // The framework's disc resolver used to hardcode the FIRST consumer's variable, so run.sh set
    // PSXPORT_SPIDERMAN_DISC, nothing read it, and every boot ran with NO MEDIA behind an ordinary
    // -looking log. Not RE — a port fact — but it belongs here because the framework must not know
    // it.
    /* discEnvVar     */ "PSXPORT_SPIDERMAN_DISC",

    // --- boot intro movies ------------------------------------------ deliberately EMPTY, and why
    // --
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
    // What is NOT established is which ID the boot plays: both callers of 0x8002B0F4 (0x8002AAB8
    // and
    // 0x8003D2EC) pass the index in a REGISTER, not as a constant, and the port stalls in CdInit
    // long before any movie is reached, so it cannot be observed either. Naming one here would be a
    // guess wearing a citation. That is RE-07.
    /* bootFmv        */ {nullptr, nullptr, nullptr, nullptr},

    // --- everything below: NOT YET REVERSE-ENGINEERED --------------------------------------------
    // Zero is the honest value. Each group is an open step in docs/re-frontier.md; the framework
    // consumers of these fields (native_step_frame's OT/packet-pool dance, PcScheduler, the CD
    // chokepoint overrides, the pad driver) are correspondingly NOT wired for this game yet — the
    // Phase-0 boot runs the guest's own main() on the substrate instead. See docs/codemap.md.

    // --- per-frame OT / packet pool (re-frontier: RE-02) ---
    /* otRegionBase   */ 0,
    /* otRegionStride */ 0,
    /* packetPoolBase */ 0,
    /* packetPoolStride */ 0,
    /* otBasePtr      */ 0,
    /* dwellCounter   */ 0,
    /* poolPtrCur     */ 0,
    /* poolPtrLast */ 0,
    /* clearOtagR     */ 0,
    /* putDrawEnv */ 0,
    /* drawSync */ 0,
    /* irqEventClasses*/ {0, 0, 0},
    /* dualviewRenderOrch */ 0,
    /* dualviewSubmit */ 0,

    // --- scheduler task layout (re-frontier: RE-03) ---
    /* taskTableBase  */ 0,
    /* taskSlotStride */ 0,
    /* taskCount */ 0,
    /* curTaskPtr     */ 0,
    /* stageStart     */ 0,
    /* stageDemo */ 0,
    /* stageGame */ 0,

    // --- overlay router slots -------------------------------------------------- N/A for this game
    // --
    // ONE overlay slot, and it is the port's own (RE-09). The disc carries no per-stage .BIN images
    // — `discdump list` shows only SLUS_008.75 plus the packed archive CD.WAD, which is what the
    // comment here used to cite as proof that this game "has no overlay modules". That was a claim
    // about the ISO layout being read as a claim about code coverage: the game loads 30 further
    // code
    // modules OUT of CD.WAD as <name>.bin + <name>.rel pairs at runtime.
    //
    // Those modules are relocatable, and the port pins every one of them to a single reserved slot
    // (game/core/module_loader.cpp) so the recompiler can emit them. Only one is ever resident. The
    // base is the heap's first block, 0x800C65EC — reserved before any guest code runs, and
    // re-checked
    // at runtime against the base the substrate was actually emitted for.
    //
    // Residency is recorded EXACTLY, by name, from the loader — not by content signature. Pinning
    // the
    // modules to one base makes their signatures collide (30 modules -> 14 distinct 32-byte
    // signatures; 12 share one), because the entry prologues are identical boilerplate and
    // relocating
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
    // The framework's CDC register model, DMA3 channel and BIOS interrupt delivery now model the
    // route needed to serve that directly, without a game address in the framework. This consumer
    // has not yet removed the cdCommand override below, so the stock route is not claimed as
    // end-to-end verified here.
    //
    // STATE OF PLAY, 2026-07-28. cdCommand = 0x8008CE8C (CD_cw) is set because it is the BEST KNOWN
    // state: with it, CdInit passes and the boot reaches the CD read path. Without it the boot
    // regresses to CdInit failing outright.
    //
    // It is nonetheless the WRONG long-term answer and is tracked as such: an ACK moves no data, so
    // reads stall one stage later. The right design is to let the guest's own register-level libcd
    // run against the framework's CDC model (DMA3 + per-sector INT1 now exist for exactly that).
    // The former interrupt-delivery blocker was resolved upstream: psxport walks SysEnqIntRP, then
    // restores the measured HookEntryInt continuation. For this binary that continuation is
    // 0x8008B990, declared under main_reentry in game/recomp_seeds.json. Removing cdCommand still
    // requires its own end-to-end stock-libcd proof; do not infer that proof from the delivery
    // seam.
    // Public stock-libcd CdInit. The host CD service owns this complete boundary: its success path
    // only installs the four callback values below and returns 1. Letting the inner controller
    // reset run would enter register-level polling whose timeout clock is the forbidden guest
    // VSync query, despite all shipping reads already completing synchronously on the host.
    /* cdInit */ 0x8008A16Cu,
    /* cdCommand */ 0x8008CE8Cu,
    // cdSync: 0x80086C60, stock libcd's CdSync(mode, result) — a thin wrapper over 0x8008CBC4. The
    // CD streaming poller at 0x80085000 calls it in a loop and only proceeds when the result is not
    // 5 (disk error); the framework handler reports complete, which is true here because every CD
    // operation this port performs has already finished synchronously by the time it is asked.
    /* cdSync */ 0x80086C60u,
    /* cdReadPrim */ 0,
    /* cdFileLoad     */ 0,
    /* cdAsyncRead */ 0,
    /* voicePlay      */ 0,
    /* voiceStop */ 0,
    /* lastSectorTracker */ 0,
    /* cdInlineLoad   */ 0,
    /* cdCmdStream    */ 0,
    // Exact stores at 0x8008A190..0x8008A1C4 on the authenticated SLUS_008.75 success path.
    /* cdCallbackTable */ {0x800B3B14u, 0x800B3B18u, 0x800B1C7Cu, 0x800B1C80u},
    /* cdCallbackFn */ {0x8008A238u, 0x8008A260u, 0x8008A288u, 0},

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
    // This is what lets the PC own the read outright. The game's handler (0x800899A0) is a
    // per-sector
    // driver: it calls CdGetSector, advances its own destination, decrements its remaining count,
    // and
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

    // cdReadStock / cdReadSync: 0x80089ECC and 0x8008A068 — stock libcd's CdRead(sectors, buf,
    // mode)
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

    // cdSearchFile: 0x80086170, stock libcd's CdSearchFile(CdlFILE*, name). Identified from the
    // boot
    // loop at 0x800649E4, which calls it with the literal "\CD_HED;1" and loops re-running CdInit
    // for as long as it returns 0 — that loop is the boot stall. Its result then feeds
    // CdControl(CdlSetloc, &loc) and CdRead(ceil(size/2048), buf, 0x80), which confirms the CdlFILE
    // layout: position at +0, size at +4.
    /* cdSearchFile   */ 0x80086170u,

    // dmaCallbackTable: 0x800B4388 — libcd's per-channel DMA-callback table. Read out of the
    // registrar at 0x8009152C, which computes `0x800B4388 + index*4` and stores its argument there.
    // Everything the guest registers goes through it, via the BIOS thunk 0x8008B89C:
    //
    //   slot 3 (CD)       <- 0x8008DB44, from the streaming setup at 0x80086030. Promotes a ring
    //   slot
    //                        from "DMA in flight" (3) to "ready" (2). Measured: the ring sat with
    //                        ten
    //                        slots at 3 while the consumer, which only accepts 2, spun on a full
    //                        ring.
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
    //   tools/redump_ram.py && external/psxport/tools/disasm.py scratch/bin/ram.bin 0x8006AE34
    //   0x8006AE90
    //
    // The per-frame consumer 0x8006B27C confirms the same layout INDEPENDENTLY: it walks 2 slots at
    // stride 0x22 from 0x800A50EC, tests byte +1 against 0x80 (libpad's multitap type nibble), and
    // on
    // the ordinary-controller path copies 8 bytes from the buffer BASE — i.e. {status, type,
    // btn_lo,
    // btn_hi, …}, which is precisely the framework's fillBuffer packet. On the multitap path it
    // instead copies four sub-pads from +2/+10/+18/+26, so +2 is a sub-record, NOT the slot base.
    //   external/psxport/tools/disasm.py scratch/bin/ram.bin 0x8006B27C 0x8006B3C8
    //
    // That resolves the base-vs-+2 ambiguity recorded against CLAIM-07: the framework's buf[0] is
    // the
    // status byte, so the buffer base is 0x800A50EC. 0x800A50EE is only its button halfword, and
    // 0x800A5130 is the game's own per-frame mirror (the 62,114 writes from 0x8006B3C8 that
    // falsified
    // the earlier guess) — writing either from the port would be wrong.
    //
    // padDriverFn is left at zero because the FRAMEWORK NEVER READS IT: the field is declared in
    // game_iface.h and consumed nowhere; its intended handler (pad_input.cpp `pad_read`) is a
    // static
    // function that overridesInit() does not register. Recorded as a wart, not a gap in this port's
    // RE — see docs/issues/framework-agnosticism-warts.md.
    /* padSlot0Buf    */ 0x800A50ECu,
    /* padSlot1Buf */ 0x800A510Eu,
    /* padDriverFn */ 0,
    /* padSlotPtrTable*/ 0,
    // Byte distance between consecutive slots' pointers. 0 is read as 4 by the framework, which is
    // the correct default; stated explicitly so this initialiser lists every field the struct has.
    /* padSlotPtrStride*/ 0,

    // --- platform HLE: the PSX hardware-sync primitives
    // -------------------------------------------
    /* hle */
    {
        // The address window PlatformHle::register_() accepts. Its job is to keep GAME/engine logic
        // out of the hardware-sync table, so both bounds are evidence-based:
        //
        //   LOWER 0x80083000 — below the lowest observed SDK kernel-call stub (0x80083EC8) and well
        //     above the highest observed game-logic address (main 0x8002C354, 0x800649E4,
        //     0x8006BF9C).
        //     Evidence: scanning the text for the SDK's BIOS-call stub idiom
        //     (`addiu $t2,$zero,0xA0/0xB0/0xC0 ; jr $t2`) finds 41 of them, ALL within
        //     0x80083EC8..0x80091730 — that cluster is the SCEI library text.
        //   UPPER 0x80096000 — the end of library text: the first address known to be .rodata is
        //     0x80096020, the "VSync: timeout" string that identified VSync in the first place.
        //
        // Provisional in the sense that it may need widening as more of the library is RE'd — but
        // it
        // fails LOUDLY (a REFUSED diagnostic) rather than silently, so widening is evidence-driven.
        /* windowLo */ {0x80083000u, 0},
        /* windowHi */ {0x80096000u, 0},

        // codeScanLo/Hi left zero on purpose: the framework then falls back to [recMainLo,
        // recMainHi),
        // which is exactly right here — this game has no overlays, so the recompiled MAIN text IS
        // the
        // entire resident code range.
        /* codeScanLo */ 0,
        /* codeScanHi */ 0,

        // NOT YET REVERSE-ENGINEERED — zero means the framework installs no handler, and the guest
        // will spin in the real primitive if it reaches one. That is the honest signal, and it is
        // precisely what currently stops the boot (docs/issues/boot-stalls.md STALL-03).
        /* decDctInSync    */ 0,
        /* decDctOutSync */ 0, // libmdec       (re-frontier: RE-07)
        /* cdReadSync      */ 0,
        /* cdDataSync    */ 0,   // libcd         (re-frontier: RE-03)
        /* cdInitHandshake */ 0, // libcd         (re-frontier: RE-03)
        /* gpuTimeoutArm   */ 0,
        /* gpuTimeoutCheck */ 0, // libgpu        (re-frontier: RE-04)
        /* gpuTimeoutDeadlineVar */ 0,
        /* gpuTimeoutFlagVar */ 0,
        /* changeThread    */ 0, // kernel yield  (re-frontier: RE-05)

        // --- libgte SetGeomOffset / SetGeomScreen: the camera projection
        // ---------------------------
        // The two leaves through which the game STATES its projection. Owning them natively makes
        // the
        // port RECORD (OFX, OFY, H) where the game sets them, instead of reading CR24/25/26 back
        // out of
        // the GTE at draw time — that read-back is the banned tap. Only the ADDRESSES are per-game;
        // the
        // recording lives in psxport (proj_params.cpp libgte_set_geom_offset/_screen), which also
        // performs the ctc2 writes the real bodies would have done, so the GTE is unchanged.
        //
        // RE PROVENANCE. Located by the only thing that marks them — the instruction they execute —
        // not by a name, a string or a caller:
        //     python3 tools/ghidra_query.py scan ctc2
        // walks every disassembled instruction and prints its denominator. Over 130,588
        // instructions in
        // 1,564 defined functions it found 1,404 `ctc2` sites, of which exactly 10 target cop2
        // control
        // registers 24/25/26 (Ghidra renders the operand pre-shifted: CR24=0xc000, CR25=0xc800,
        // CR26=0xd000). Reproduce either leaf with
        //     python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8008BF14
        //     0x8008BF3C
        // (capstone stops AT the ctc2 — it does not decode cop2 control moves — so the ctc2 words
        // are
        // cited by encoding below; that limitation is why Ghidra, not disasm.py, is the authority
        // here.)
        //
        //   0x8008BF14  SetGeomScreen(h)                     word 48C4D000 = ctc2 $a0,$26   ; H =
        //   a0
        //               0x8008BF18  jr $ra / 0x8008BF1C nop   -- a 3-instruction leaf, nothing else
        //   0x8008BF24  SetGeomOffset(ofx, ofy)
        //               0x8008BF24  00240400 = sll  $a0,$a0,0x10   ; ofx << 16 (CR24/25 are 16.16)
        //               0x8008BF28  002c0500 = sll  $a1,$a1,0x10   ; ofy << 16
        //               0x8008BF2C  word 48C4C000 = ctc2 $a0,$24   ; OFX
        //               0x8008BF30  word 48C5C800 = ctc2 $a1,$25   ; OFY
        //               0x8008BF34  jr $ra / 0x8008BF38 nop
        // Both sit inside the [0x80083000, 0x80096000) SCEI library window declared above, so
        // register_() accepts them. 0x8008BE5C is libgte InitGeom (it seeds ZSF3=0x155, ZSF4=0x100,
        // H=0x3E8, DQA=-0x1062, DQB=0x1400000 and zeroes OFX/OFY, then returns) — NOT registered,
        // because its recompiled body already does exactly that and the game overwrites all three
        // values before it draws.
        //
        // NO VALUES ARE BAKED IN HERE, AND THAT IS NOT LAZINESS — SPIDER-MAN HAS NONE TO BAKE.
        // Tomba!2 states literal constants (OFX 160 / OFY 120 / H 350). Spider-Man does not: the
        // sole
        // call site of each leaf is inside FUN_80075D0C (jal at 0x80076180 and 0x80076190, one
        // caller
        // each — `tools/ghidra_query.py xrefs 0x8008BF14` / `0x8008BF24`), and it passes fields it
        // has
        // just COMPUTED from a viewport descriptor `param_2` (u16 array; the caller loads them back
        // as
        //     8007617C  lhu $a0,0xE($s4)          -> H     = vp[7]
        //     80076188  lhu $a0,0x10($s4)         -> OFX   = vp[8]
        //     8007618C  lhu $a1,0x12($s4)         -> OFY   = vp[9] ):
        //     vp[8] = (vp[2] + vp[0]) >> 1                              ; viewport centre X
        //     vp[9] = (vp[3] + vp[1]) >> 1                              ; viewport centre Y
        //     vp[7] = ((((vp[0] - vp[2]) >> 1) << 12) / vp[6] << 12) / *(int*)(gp + 0x1140)
        // i.e. the projection is a FUNCTION OF THE ACTIVE VIEWPORT AND FOV, re-derived per view. A
        // constant copied from one frame would be wrong in the next. Capturing at the setter is
        // therefore not merely the tidy option here, it is the only correct one.
        //
        // ONE OTHER PATH WRITES CR24/25 AND IT IS ACCOUNTED FOR. FUN_8007C2AC (a hand-written GTE
        // routine) zeroes OFX/OFY for its inner loop (0x8007C0B4/0x8007C0B8, ctc2 $zero) and
        // RESTORES
        // them at 0x8007C268/0x8007C26C from `*(u32*)(*(void**)0x800B5918 + 0x10)`, split into two
        // 16.16 halves. 0x800B5918 is gp+0x1124 (gp = 0x800B47F4, line 44 above), which
        // FUN_80075D0C
        // writes with that same `param_2`; +0x10 is vp[8]/vp[9]. So the restore replays exactly the
        // pair SetGeomOffset was given — the recorded ProjParams cannot drift from the GTE, and no
        // handler is needed there. H is never written outside SetGeomScreen.
        //
        // COVERAGE / NEGATIVE CONTROL. Ghidra had disassembled only 24.9% of the 2 MB image, so the
        // scan's own blind-spot line was checked by a raw word scan of ALL 524,288 words of
        // scratch/bin/spiderman/ram.bin for every `ctc2 rX,CR24/25/26` encoding and for
        // `jal 0x8008BF14` / `jal 0x8008BF24` / `jal 0x8008BE5C`. It returned the SAME 10 ctc2
        // sites
        // and the same one caller each — 0 additional sites hidden by the undisassembled 75%.
        /* setGeomOffset */ 0x8008BF24u,
        /* setGeomScreen */ 0x8008BF14u,

        // The native Spider1FrameDriver owns every display field and presentation fence. Any guest
        // call to libetc VSync — query or wait — is therefore a second cadence owner and aborts.
        // The framework protects this binding from later title registration; the title declares
        // only this measured address and never supplies a successful VSync handler.
        /* vsyncTrap */ 0x80084BE0u,
    },

    // --- present policy -------------------------------------------------------------------------
    // preserveVramBackdrop = 1, because THIS PORT STILL RUNS THE GUEST'S OWN DRAWING CODE, which is
    // exactly the condition the field's own documentation names ("Set to 1 while the guest still
    // owns
    // drawing"): an upload into the display area IS visible on hardware, so upload-only screens —
    // loading screens, fades, static art that submits no primitives — must not be cleared away.
    //
    // HONESTY NOTE, because this was tried as a fix and is NOT one: it does not address the 30 Hz
    // full-scene/black flicker. That flicker came from presents being paced by the display field
    // clock while this game builds one ordering table per TWO fields, so every other present
    // rebuilt
    // the composite from an empty batch. Setting this flag only skips render_geom's CLEAR, while
    // upload_vram still overwrites the composite with guest VRAM — which for a natively-compositing
    // port is empty, so the frame came out black regardless. Measured: unchanged at 0.0/99.4/0.0/
    // 99.4/0.0/99.4% across six consecutive presents.
    //
    // The real fix is in the framework: a present carrying no new geometry now re-shows the last
    // composite instead of rebuilding one (gpu_vk.cpp, geom_batch_empty), which is what hardware
    // does
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
    // Vblanks one gpu_pace_frame call represents. 0 used to fall through to a scratchpad read of
    // 0x1F800235 — Tomba!2's engine field, ordinary working memory here — so this port slept on
    // garbage and ran ~2.3x slower windowed than headless. That fallback is deleted (psxport
    // gpu_native.cpp).
    //
    // DERIVATION — why 1, and why the 2 that stood here was wrong. paceQuota is NOT the game's
    // frame
    // rate. game_iface.h states its semantics explicitly: "by CALLING CADENCE, not the game's
    // display
    // rate", and it is the number of vblanks that ONE gpu_pace_frame call represents. The framework
    // then sleeps exactly `quota/60 s` per call (gpu_native.cpp gpu_pace_subframe).
    //
    // HISTORICAL DERIVATION. Before the native driver, the port had exactly two calls, both the
    // same blocking-VSync / FUN_8005E748 field-wait shape:
    //
    //     while (<guest vblank counter> < target) { gpu_pace_frame(c); vblank_advance(c); }
    //
    // vblank_advance recomputes that counter from REAL elapsed time at the NTSC field rate, so the
    // loop is a real-time wait and one gpu_pace_frame call is its WAIT QUANTUM — the granularity at
    // which the wait can notice it is done. The counter it tests is denominated in VBLANKS. A
    // quantum
    // therefore has to be ONE vblank, or every wait is rounded up to a multiple of the quantum.
    // That
    // is precisely the case game_iface.h names: one pace quantum equals one display field. The
    // current Spider1FrameDriver passes its explicit delivered-field count to FramePresenter, and
    // this legacy compatibility value retains the same one-field quantum.
    //
    // The 2 was a guess at the game's display rate, and the guess also mis-modelled the game: the
    // guest does NOT ask for two fields at a time. MEASURED windowed with `PSXPORT_DEBUG=pace`
    // (the retired field-clock instrument), 260 s, 7449 field-wait entries: 7327 of them (98.4%)
    // are
    // FUN_8005E748(n=1) — a request for ONE field — and the remaining 122 are one n=240 loading
    // delay. With quota=2, 578 of 599 consecutive entries advanced the guest counter by 2 while it
    // had asked for 1, at a median spacing of 33.30 ms (= 2/60 s). Every one-field wait was served
    // with a two-field sleep, and the game issues ~2 of them per rendered frame, so the frame
    // budget
    // was 4 fields instead of 2 and the port ran at half speed.
    //
    // GATE, WINDOWED (headless is never paced — gpu_pace_subframe early-returns without a window,
    // so
    // it cannot measure this at all), one build apart, same instrument, same 215 s steady window,
    // same PSXPORT_PAD_REPLAY (scratch/g1_pace/logs/{before_quota2_paced,after_quota1}.log):
    //   quota=2  presents 59.94/s  rebuild_geom 15.57/s  presents/geom 3.85  pace entries 30.00/s
    //   quota=1  presents 59.94/s  rebuild_geom 29.66/s  presents/geom 2.02  pace entries 60.00/s
    // Pace entries per RENDERED frame is 1.93 before and 2.02 after — unchanged. The game's loop
    // structure did not move; only the length of the quantum did, which is what makes this a
    // derivation rather than a tuned number. The pace-entry rate is exactly 60/quota in both legs,
    // i.e. the pacer was the throttle and nothing else changed.
    /* paceQuota       */ 1u,
    /* windowTitle     */ "Spider-Man",
    // crt0 stack-top bias, MEASURED by psxport tools/crt0_extract over SLUS_008.75 (entry
    // 0x8008739C). POSITIONAL, so it must stay LAST and match stackBias's position at the end of
    // GameConfig. declared = 1 is mandatory: crt0_plan REFUSES a boot when it is 0, because 0 is a
    // REAL measured answer for some crt0s (X4, Toy Story 2) and cannot double as "unset".
    /* stackBias       */ {1, -8},
};

const GameConfig &spider::legacy::measuredConfig = g_spiderman_cfg;
