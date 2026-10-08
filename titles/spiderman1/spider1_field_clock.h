// spider1_field_clock.h — SLUS_008.75 libetc VSync(0) field-clock arithmetic.
#pragma once

#include <cstdint>

class Core;

namespace spider::spider1 {

// libetc VSync(0) returns the horizontal counter's advance since the previous field, truncated to
// 16 bits, and rebases the counter.
class Spider1FieldClock final {
public:
  // Title field counter, advanced by the display-field owner.
  static uint32_t fieldCount(Core &core);

  // Horizontal counter read through the title's pointer; a null pointer reads as zero like the
  // retail load.
  static uint32_t horizontalCounter(Core &core);

  // What libetc VSync(0) leaves in `v0`.
  static uint32_t vsyncReturnValue(Core &core);

  // Rebase the counter and publish the delivered field; must run after the return value is read.
  static void recordDeliveredField(Core &core);
};

} // namespace spider::spider1
