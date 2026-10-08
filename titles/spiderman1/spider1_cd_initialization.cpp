#include "spider1_cd_initialization.h"

#include "spider1_platform_facts.h"

#include "core.h"
#include "execution_control.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spider::spider1 {
namespace {

// CdInit's CD-ROM interrupt registration, reproduced because CdInit runs at a display-field
// boundary that propagates out of a single-turn dispatch and cannot call the guest thunk.
void registerGuestCdRomHandler(Core &core) {
  const uint32_t slot = guestInterruptHandlerTable + guestInterruptSourceCdRom * 4u;
  const uint32_t bit = 1u << guestInterruptSourceCdRom;
  const uint32_t previous = core.mem_r32(slot);
  if (previous == guestCdRomInterruptHandler) {
    lucent::info("cd",
                 "Spider-Man 1 CdInit found the guest's CD-ROM interrupt handler already "
                 "registered at source {}",
                 guestInterruptSourceCdRom);
    return;
  }
  // 0x8008BC14/0x8008BC1C: the guest refuses to register before its interrupt init; fail loudly.
  const uint32_t initialised = core.mem_r16(guestInterruptInitialised);
  if (initialised == 0) {
    lucent::error(
        "cd",
        "Spider-Man 1 CdInit cannot register the guest's CD-ROM interrupt handler: the "
        "guest's interrupt-initialised flag at 0x{:08X} is {}. The guest's own 0x8008BBD0 "
        "returns without registering in that state, so this CdInit is one the guest could "
        "never have reached",
        guestInterruptInitialised,
        initialised);
    std::abort();
  }

  const uint32_t enableBefore = core.mem_r16(guestInterruptEnableMask);
  core.mem_w32(slot, guestCdRomInterruptHandler);
  core.mem_w16(guestInterruptEnableMask, static_cast<uint16_t>(enableBefore | bit));
  const uint32_t maskBefore = core.mem_r32(interruptMaskRegister);
  core.mem_w32(interruptMaskRegister, maskBefore | bit);

  lucent::info(
      "cd",
      "Spider-Man 1 CdInit registered the guest's own CD-ROM interrupt handler: source {} "
      "0x{:08X} -> 0x{:08X}, enable mask 0x{:04X} -> 0x{:04X}, I_MASK 0x{:03X} -> 0x{:03X}. "
      "0x8008BA00 dispatches it, with the gate at 0x{:08X} open for its extent. Retail "
      "CdInit performs exactly this at 0x8008D54C",
      guestInterruptSourceCdRom,
      previous,
      guestCdRomInterruptHandler,
      enableBefore,
      enableBefore | bit,
      maskBefore,
      maskBefore | bit,
      guestServiceGate);
}

} // namespace

void Spider1CdInitialization::installCallbackSlots(Core &core) {
  // The host owns CD operations synchronously, so the guest controller reset handshake (IntrWait
  // on I_STAT bit 0) is skipped and the event callbacks are installed directly.
  core.mem_w32(cdSyncCallbackSlot, cdSyncCallback);
  core.mem_w32(cdReadyCallbackSlot, cdReadyCallback);
  core.mem_w32(cdEventCallbackSlot, cdEventCallback);
  core.mem_w32(cdEventUnusedSlot, 0);
  core.r[2] = 1;
}

void Spider1CdInitialization::install(Core &core) {
  installCallbackSlots(core);
  registerGuestCdRomHandler(core);
}

} // namespace spider::spider1
