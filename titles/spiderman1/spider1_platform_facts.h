#pragma once

#include "guest_pad_buffer_layout.h"
#include "platform_hle.h"

namespace spider::spider1 {

// SLUS_008.75 library-entry evidence: RE-02/RE-03/RE-17 and the preserved measured
// configuration in commit 8950617. These are title addresses, not lineage defaults.
inline constexpr PlatformHlePlan platformServices{
    .setGeomOffset = 0x8008BF24u,
    .setGeomScreen = 0x8008BF14u,
    .cdReadAddress = 0x80089ECCu,
    .cdReadSyncAddress = 0x8008A068u,
    .vsyncAddress = 0x80084BE0u,
    .windowLo = {0x80083000u},
    .windowHi = {0x80096000u},
};

// RE-05: PadInitDirect at 0x8006AE34 supplies these bases; the independent
// 0x8006B27C consumer walks 0x22-byte records. Neither +2 nor the mirror is a base.
inline constexpr GuestPadBufferLayout padBuffers{
    .slot0Buffer = 0x800A50ECu,
    .slot1Buffer = 0x800A510Eu,
};

} // namespace spider::spider1
