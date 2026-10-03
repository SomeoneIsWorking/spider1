#pragma once

#include "core.h"
#include "execution_control.h"
#include "native_dispatch.h"

namespace spider {

// The one override installer: psxport's, which resolves the active image identity and refuses an
// address no image owns or a key that already has an owner. Re-exported here so every spider1
// override site reads as one call rather than repeating the qualification.
using psx::cpu::installNativeOverride;

inline bool dispatchGuestOrPropagate(Core &core, std::uint32_t address) {
  return psx::cpu::completeOrPropagate(
      core, psx::cpu::dispatchGuest(core, address, psx::cpu::ExecutionBudget::currentTurn(core)));
}

inline bool callOriginalOrPropagate(Core &core, std::uint32_t address) {
  return psx::cpu::completeOrPropagate(
      core, psx::cpu::callOriginal(core, address, psx::cpu::ExecutionBudget::currentTurn(core)));
}

} // namespace spider
