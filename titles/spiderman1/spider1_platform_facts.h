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

// The PSX interrupt mask, and the bit that means CD-ROM. Both are framework device facts, not
// title ones — `IRQ_BIT_CD = 2` in external/psxport/runtime/psx/irq_edge.h and `kIrqMask` in
// runtime/psx/io_peripherals.cpp — and they are named here so the arm below is the device's
// contract rather than a literal this title invented.
//
// THE THREE CdInit CALLBACKS ARE `DeliverEvent(0xF0000003, 0x20|0x40)`, read from the executable:
// 0x8008A238 and 0x8008A260/0x8008A288 each set `a0 = 0xF0000003` and `jal 0x8008F9D0`, the B0:0x07
// DeliverEvent stub. 0xF0000003 is the PSX's HwCD class; the framework already names its neighbours
// in its own source (`mem.cpp:984` calls 0xF0000009 "the PSX's fixed HwSPU class").
// `Hle::deliverEvent` has arms for SwCARD, HwCARD and HwSPU and NONE for HwCD, and the BIOS's own
// CD-ROM interrupt handler — which on hardware reads the controller response and calls the function
// at 0x800B3B18 — is ROM code this port does not have. That is the root cause of the black frames;
// issue 0026 carries the evidence and the proposed framework change.
//
// WHY THE TITLE OWNS IT. The retail `CdInit` 0x8008A16C installs its four callback slots (which
// `Spider1FrameDriver::initializeCd` reproduces exactly) and ALSO calls 0x8008A1FC, whose
// 0x8008D4E4 reaches 0x8008D54C: `jal 0x8008B86C` with `a0 = 2` in its delay slot.
//
// CORRECTION 2026-09-28 (docs/issues/0026). The comment this replaces called 0x8008B86C "the
// B-vector thunk at ([0x800B390C] + 8)" and concluded "on a PSX B-vector entry 2 is the
// interrupt-enable call, so the retail body arms IRQ2". **0x8008B86C is not a BIOS call at all**,
// and the bytes say so:
//
//     0x8008B86C  lui $v0,0x800B ; lw $v0,0x390C($v0)   ; $v0 = [0x800B390C] = 0x800B38EC
//     0x8008B87C  lw $v0,8($v0)                          ; $v0 = [0x800B38EC+8] = 0x8008BBD0
//     0x8008B884  jalr $v0                               ; calls the GUEST's own 0x8008BBD0(2,
//     name)
//
// 0x800B390C is this title's HARDWARE-ADDRESS table, not a BIOS vector table: [+4] = 0x1F801070
// I_STAT, [+8] = 0x1F801074 I_MASK, [+0xC] = 0x1F8010F0 DPCR. 0x8008BBD0 is the guest's own
// interrupt-source registration, which records `{u16 length; char* name; ...}` at 0x800B2888 +
// index*4 — a table that is ALL ZEROS in the image and is filled at run time through the guest's
// B0:0x5B and C0:0x0A reaches. So the arm is a guest-level registration, and the step this comment
// used to assert does not exist.
//
// The CONCLUSION is unchanged, because the override below writes the device directly and the device
// agrees: I_MASK goes 0x009 -> 0x00D. The reason given for it was wrong, and 0x800B2888 has no
// walker in this port, which is part of why the CD-ROM interrupt has no owner at all (issue 0026).
inline constexpr uint32_t interruptMaskRegister = 0x1F801074u;
inline constexpr uint32_t interruptMaskCdBit = 0x4u;
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
