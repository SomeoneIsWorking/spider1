#pragma once

#include "core.h"
#include "native_dispatch.h"

namespace spider::spider1 {

// Enter `entry` as if the guest had `jal`'d it from `returnPc`, which is the return address the
// guest recorded at that call site. A bounded-exit return counts as a completed call, unlike the
// frame driver's.
inline void enterGuestCall(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  psx::cpu::dispatchGuestToReturn(core,
                                  entry,
                                  psx::cpu::ExecutionBudget::currentTurn(core),
                                  "Spider-Man mode synchronous guest call");
}

} // namespace spider::spider1
