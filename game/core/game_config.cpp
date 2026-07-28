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
    /* recMainHi      */ REC_MAIN_HI,   // 0x000C6800  (load + text size 0x000B6800)

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
    // Spider-Man ships ONE executable and no overlay modules: the disc carries SLUS_008.75 plus the
    // packed archive CD.WAD, and `discdump list` shows no per-stage .BIN images (contrast Tomba!2's
    // BIN/START|DEMO|GAME|A00..A0L). The recompiler confirms it: "0 overlay module(s)". So there are
    // no overlay slots to describe — this is genuinely empty, not un-RE'd.
    /* overlaySlots   */ {{0, nullptr}, {0, nullptr}, {0, nullptr}},

    // --- CD chokepoints (re-frontier: RE-04) ---
    /* cdInit         */ 0, /* cdCommand */ 0, /* cdSync */ 0, /* cdReadPrim */ 0,
    /* cdFileLoad     */ 0, /* cdAsyncRead */ 0,
    /* voicePlay      */ 0, /* voiceStop */ 0, /* lastSectorTracker */ 0,
    /* cdInlineLoad   */ 0,
    /* cdCmdStream    */ 0,
    /* cdCallbackTable*/ {0, 0, 0, 0},
    /* cdCallbackFn   */ {0, 0, 0, 0},

    // --- pad driver (re-frontier: RE-05) ---
    /* padSlot0Buf    */ 0, /* padSlot1Buf */ 0, /* padDriverFn */ 0,
    /* padSlotPtrTable*/ 0,

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
};

extern void spiderman_install_game_hooks();   // game/core/game_hooks.cpp

void spiderman_install_game_config() {
  extern const GameHooks* spiderman_game_hooks();
  psxport_install_game(&g_spiderman_cfg, spiderman_game_hooks());
}
