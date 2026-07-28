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
    // Spider-Man has NO overlay modules — one executable (SLUS_008.75) plus the packed archive
    // CD.WAD. emit.py reports "0 overlay module(s)", so the generated table is empty and the two
    // overlay-specific setters have nothing to point at. Null here is the accurate value, not a gap.
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
