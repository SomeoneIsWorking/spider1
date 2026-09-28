// spider1_field_clock.h — the display-field clock facts SLUS_008.75's libetc VSync(0) is defined in
// terms of.
#pragma once

#include <cstdint>

class Core;

namespace spider {

// The one owner of the field-clock arithmetic, which the port previously wrote out three times.
//
// The rule is a VSync return VALUE, not a side effect, and it is easy to get subtly wrong: libetc's
// VSync(0) returns how far the horizontal counter has advanced since the PREVIOUS field, truncated
// to sixteen bits, and it rebases that counter as it goes. A caller that recomputes the subtraction
// itself is re-deriving a guest contract, and three copies of that derivation is two copies waiting
// to disagree with the guest. So the arithmetic lives here once, and the three call sites that need
// it call it.
class Spider1FieldClock final {
public:
  // The title's own field counter, which the display-field owner advances. Compared against a value
  // sampled at the start of a frame to answer "did a field pass during this frame's work?".
  static uint32_t fieldCount(Core &core);

  // The horizontal counter the return value is measured against, read through the title's own
  // pointer. A null pointer reads as zero rather than faulting, which is what the retail code's own
  // load would have produced before anything wrote the pointer.
  static uint32_t horizontalCounter(Core &core);

  // Exactly what libetc VSync(0) leaves in `v0`: the counter's advance since the last field, in
  // sixteen bits.
  static uint32_t vsyncReturnValue(Core &core);

  // Rebase the counter for a delivered field, and publish which field it was. This is the half of
  // the contract that WRITES, and it must run after the return value has been read.
  static void recordDeliveredField(Core &core);
};

} // namespace spider
