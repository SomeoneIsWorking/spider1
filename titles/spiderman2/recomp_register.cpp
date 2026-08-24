// Enter Electro's generated-substrate seam. The title has no native overrides yet.
#include "core.h"
#include "overlay_table.h"
#include "recomp_iface.h"

extern void shard_set_override(uint32_t, void (*)(Core *));

static const RecompRegistry g_enter_electro_recomp = {
    .main_dispatch = main_dispatch,
    .rec_func_index = rec_func_index,
    .overlays = g_rec_overlays,
    .overlay_count = g_rec_overlay_count,
    .shard_set_override = shard_set_override,
    .ov_a00_set_override = nullptr,
    .ov_game_set_override = nullptr,
    .guestMemset_gen = nullptr,
};

void enter_electro_install_recomp() {
  psxport_install_recomp(&g_enter_electro_recomp);
}
