// SLUS_008.75's ResetGraph, without its VSync.
#pragma once

#include <cstdint>

class Core;

namespace spider::spider1 {

// ResetGraph's GPU half with its VSync(0) removed; the field owner delivers that field. A "not
// reset" probe forces mode 0, and a prior display mode of 1 takes one extra finalize step.
class Spider1GpuReset final {
public:
  static constexpr uint32_t resetGraphEntry = 0x80084778u;
  // Where the removed VSync(0) returned inside ResetGraph.
  static constexpr uint32_t resetGraphVsyncReturn = 0x8008479Cu;

  // Returns false when a step asked the host to leave.
  static bool reset(Core &core, uint32_t requestedMode);
};

} // namespace spider::spider1
