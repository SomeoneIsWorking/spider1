// recomp_register.cpp — fills the framework↔generated-substrate seam (recomp_iface.h) from THIS
// game's recompiled symbols. This is the ONE file that names generated/ symbols; the framework
// reaches them only through psxport_recomp()->field.
#include "core.h"
#include "recomp_iface.h"
#include "overlay_table.h"   // generated: main_dispatch, g_rec_overlays, g_rec_overlay_count

extern void shard_set_override(uint32_t, void (*)(Core*));   // generated/shard_disp.c (MAIN module)

static const RecompRegistry g_spiderman_recomp = {
    /* main_dispatch        */ main_dispatch,
    /* rec_func_index       */ rec_func_index,
    // Spider-Man DOES have overlay modules — corrected 2026-07-29. This comment used to say it had
    // none, on the strength of the ISO tree carrying no overlay FILES. That was true and misleading:
    // the game loads further CODE at runtime out of CD.WAD as <name>.bin + <name>.rel pairs, which
    // tools/extract_modules.py now relocates offline so the recompiler can emit them (RE-09). The
    // table below is populated (currently: SHELL). The two overlay-specific setters stay null because
    // they are for games whose overlays share a FIXED slot and need per-slot override installation;
    // these modules load to distinct heap addresses and are routed by content signature instead.
    /* overlays             */ g_rec_overlays,
    /* overlay_count        */ g_rec_overlay_count,
    /* shard_set_override   */ shard_set_override,
    /* ov_a00_set_override  */ nullptr,
    /* ov_game_set_override */ nullptr,
    // guestMemset_gen: the framework's only consumer is guest_memset_install() (mem.cpp), which is
    // DEAD CODE — defined, never called, and it hardcodes Tomba!2's memset address 0x8009A420. This
    // port installs no such override, so null is correct and cannot be dereferenced. Recorded as a
    // framework agnosticism wart in docs/issues/.
    /* guestMemset_gen      */ nullptr,
};

void spiderman_install_recomp() { psxport_install_recomp(&g_spiderman_recomp); }
