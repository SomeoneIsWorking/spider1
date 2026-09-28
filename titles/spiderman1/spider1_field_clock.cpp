#include "spider1_field_clock.h"

#include "spider1_guest_layout.h"
#include "spider1_platform_facts.h"

#include "core.h"

namespace spider {

uint32_t Spider1FieldClock::fieldCount(Core &core) {
  return core.mem_r32(spider1::libetcVblankCountAddress);
}

uint32_t Spider1FieldClock::horizontalCounter(Core &core) {
  const uint32_t pointer = core.mem_r32(spider1::horizontalCounterPointer);
  return pointer != 0 ? core.mem_r32(pointer) : 0u;
}

uint32_t Spider1FieldClock::vsyncReturnValue(Core &core) {
  return (horizontalCounter(core) - core.mem_r32(spider1::horizontalCounterBaseline)) & 0xFFFFu;
}

void Spider1FieldClock::recordDeliveredField(Core &core) {
  core.mem_w32(spider1::lastVsyncField, core.mem_r32(spider1::libetcVblankCountAddress));
  core.mem_w32(spider1::horizontalCounterBaseline, horizontalCounter(core));
  // The retail VSync tail reads the GPU's status word before it returns. The value is not used by
  // anything this port reproduces, but the READ is part of the observable tail: a guest that
  // installed a store observer, or a future reader asking what the tail touches, must see that this
  // word is read. It is read for its side effect of being read, and the result is discarded on
  // purpose rather than by accident.
  (void)core.mem_r32(spider1::gpuStatusWord);
}

} // namespace spider
