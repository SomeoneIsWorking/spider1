#pragma once

#include "guest_cd_stream_callback_layout.h"
#include "guest_pad_buffer_layout.h"
#include "platform_hle.h"

namespace spider::spider1 {

// ResetGraph's pre-main VSync(0) return site, measured in SLUS_008.75.
inline constexpr uint32_t resetGraphVsyncReturn = 0x8008479Cu;
inline constexpr uint32_t movieInitialVsyncReturn = 0x8002AC8Cu;
inline constexpr uint32_t movieFrameVsyncReturn = 0x8002AE1Cu;
inline constexpr uint32_t movieTeardownVsyncReturn = 0x8002AFECu;
inline constexpr uint32_t libetcVblankCountAddress = 0x800B397Cu;

inline constexpr bool isMovieFieldReturn(uint32_t returnPc) {
  return returnPc == movieInitialVsyncReturn || returnPc == movieFrameVsyncReturn ||
         returnPc == movieTeardownVsyncReturn;
}
inline constexpr uint32_t cdInitAddress = 0x8008A16Cu;
inline constexpr uint32_t cdCommandAddress = 0x8008CE8Cu;
inline constexpr uint32_t cdLastPositionAddress = 0x800B3B2Cu;
inline constexpr uint32_t cdLastModeAddress = 0x800B3B30u;
// The authenticated get/set bodies establish slot ownership: CdSyncCallback at 0x80086C80
// replaces 0x800B3B14, and CdReadyCallback at 0x80086C94 replaces 0x800B3B18. CdInit's initial
// function at the latter is replaced by libstr's 0x800860B4 during an STR stream.
inline constexpr uint32_t cdSyncCallbackSlot = 0x800B3B14u;
inline constexpr uint32_t cdSyncCallback = 0x8008A238u;
inline constexpr uint32_t cdReadyCallbackSlot = 0x800B3B18u;
inline constexpr uint32_t cdReadyCallback = 0x8008A260u;
inline constexpr GuestCdStreamCallbackLayout cdStreamCallbacks{
    .readyCallbackPointer = cdReadyCallbackSlot,
    .owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
};
inline constexpr uint32_t cdEventCallbackSlot = 0x800B1C7Cu;
inline constexpr uint32_t cdEventCallback = 0x8008A288u;
inline constexpr uint32_t cdEventUnusedSlot = 0x800B1C80u;

// SLUS_008.75 library-entry evidence: RE-02/RE-03/RE-17 and the preserved measured
// configuration in commit 8950617. These are title addresses, not lineage defaults.
inline constexpr PlatformHlePlan platformServices{
    .setGeomOffset = 0x8008BF24u,
    .setGeomScreen = 0x8008BF14u,
    .cdReadAddress = 0x80089ECCu,
    .cdReadSyncAddress = 0x8008A068u,
    .cdCommandAddress = cdCommandAddress,
    .stockCdWorkArea = {cdLastPositionAddress, cdLastModeAddress},
    .vsyncAddress = 0x80084BE0u,
    // Authenticated libetc VSync(-1): 0x80084C44 loads the title's VBlank count directly.
    .vsyncQueryCounterAddress = libetcVblankCountAddress,
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
