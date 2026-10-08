// The CdInit body SLUS_008.75's boot reaches.
#pragma once

#include "spider1_platform_facts.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// The retail CdInit body's two halves, split so a hermetic test can drive the slots against a bare
// Core; the registration writes a hardware register.
class Spider1CdInitialization final {
public:
  static void install(Core &core);

  // The slot half alone; guest RAM only.
  static void installCallbackSlots(Core &core);
};

} // namespace spider::spider1
