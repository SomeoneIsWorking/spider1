#pragma once

#include "core.h"
#include "execution_control.h"
#include "native_dispatch.h"

namespace spider {

// psxport's override installer, re-exported so call sites read as one call.
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
