#pragma once

#include "guest_cd_stream_callback_layout.h"
#include "guest_pad_buffer_layout.h"
#include "guest_program_image.h"
#include "platform_hle.h"

namespace spider::spider1 {

// ResetGraph's pre-main VSync(0) return site.
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

// PSX I_MASK register and its CD-ROM bit (IRQ2). The CdInit callbacks are DeliverEvent
// (0xF0000003, 0x20|0x40) and need the guest's source registration, which the runtime cannot arm.
inline constexpr uint32_t interruptMaskRegister = 0x1F801074u;
inline constexpr uint32_t interruptMaskCdBit = 0x4u;

// Guest interrupt source registration. 0x8008BBD0 registers a source only while the u16 at
// 0x800B2884 is 1; the u16 at 0x800B2886 means the top-level handler 0x8008BA00 is running.
inline constexpr uint32_t guestInterruptSourceRegistration = 0x8008B86Cu;
inline constexpr uint32_t guestInterruptSourceCdRom = 2u;
inline constexpr uint32_t guestCdRomInterruptHandler = 0x8008DA24u;
inline constexpr uint32_t guestTopLevelInterruptHandler = 0x8008BA00u;
inline constexpr uint32_t guestInterruptHandlerTable = 0x800B2888u;
inline constexpr uint32_t guestInterruptEnableMask = 0x800B28B4u;
inline constexpr uint32_t guestInterruptInitialised = 0x800B2884u;
inline constexpr uint32_t guestServiceGate = 0x800B2886u;

// CdSyncCallback 0x80086C80 sets slot 0x800B3B14 and CdReadyCallback 0x80086C94 sets 0x800B3B18;
// libstr's 0x800860B4 replaces the latter during an STR stream.
inline constexpr uint32_t cdSyncCallbackSlot = 0x800B3B14u;
inline constexpr uint32_t cdSyncCallback = 0x8008A238u;
inline constexpr uint32_t cdReadyCallbackSlot = 0x800B3B18u;
inline constexpr uint32_t cdReadyCallback = 0x8008A260u;
inline constexpr uint32_t cdEventCallbackSlot = 0x800B1C7Cu;
inline constexpr uint32_t cdEventCallback = 0x8008A288u;
inline constexpr uint32_t cdEventUnusedSlot = 0x800B1C80u;

// libcd DMACallback 0x8009152C stores channel ch's callback at this base + 4*ch (DICR is
// 0x1F8010F4).
inline constexpr uint32_t guestDmaCallbackTable = 0x800B4388u;

// Heap 1 of the guest allocator FUN_800651C8, set up by FUN_8006BF9C from the descriptor at
// 0x8009C5B8: heap 0 is [0x800C65D4, 0x800C65E4), heap 1 runs from there to the stack guard
// 0x801FE000. The overlay loader FUN_8001B990 allocates NAME.bin from it and reads it with CdRead,
// so a read landing inside is code the title placed (physical addresses).
inline constexpr GuestAddressRange overlayHeapArena{0x000C65E4u, 0x001FE000u};

// SLUS_008.75 library entry points.
inline constexpr PlatformHlePlan platformServices{
    .setGeomOffset = 0x8008BF24u,
    .setGeomScreen = 0x8008BF14u,
    .cdReadAddress = 0x80089ECCu,
    .cdReadSyncAddress = 0x8008A068u,
    .cdCommandAddress = cdCommandAddress,
    .stockCdWorkArea = {cdLastPositionAddress, cdLastModeAddress},
    .dmaCallbackTable = guestDmaCallbackTable,
    .vsyncAddress = 0x80084BE0u,
    // libetc VSync(-1): 0x80084C44 loads the VBlank count directly.
    .vsyncQueryCounterAddress = libetcVblankCountAddress,
    .windowLo = {0x80083000u},
    .windowHi = {0x80096000u},
};

// PadInitDirect 0x8006AE34 supplies these bases; the consumer at 0x8006B27C walks 0x22-byte
// records.
inline constexpr GuestPadBufferLayout padBuffers{
    .slot0Buffer = 0x800A50ECu,
    .slot1Buffer = 0x800A510Eu,
};

} // namespace spider::spider1
