// spider1_guest_call.h — how the title's mode drivers enter a retail guest function and come back.
#pragma once

#include "core.h"
#include "native_dispatch.h"

namespace spider {

// Enter `entry` as if the guest had `jal`'d it from `returnPc`, and return once that guest body
// returns. `returnPc` is the address the GUEST itself recorded in `r[31]` at that call site, read
// out of the executable; the driver installs it so the retail body sees its own return address when
// it comes back and continues at the instruction after its own `jal`.
//
// This is a mode-driver convention and it is deliberately NOT the frame driver's: a mode driver
// treats a bounded-exit return as a completed call and keeps driving, because a mode step must
// reach exactly one presentation fence before it returns to its host. The frame driver's boundary
// leaves
// (`dispatchGuestOrPropagate` in `spider1_frame_driver.cpp`) propagate the exit instead, because a
// host field boundary is the frame driver's own cadence, not a call the mode made.
inline void spider1CallGuest(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  psx::cpu::dispatchGuestToReturn(core,
                                  entry,
                                  psx::cpu::ExecutionBudget::currentTurn(core),
                                  "Spider-Man mode synchronous guest call");
}

} // namespace spider
