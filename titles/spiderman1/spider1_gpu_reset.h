// spider1_gpu_reset.h — SLUS_008.75's ResetGraph, without its VSync.
#pragma once

#include <cstdint>

class Core;

namespace spider {

// The GPU reset sequence ResetGraph owns, with the cadence call taken out.
//
// ResetGraph is the guest's whole-frame setup, and this port keeps its GPU half natively while
// removing its VSync(0) — the title's field owner delivers that field instead, because cadence is
// not a GPU's business. So what remains is genuinely one concept: the ordered GPU reset steps and
// the two decisions among them.
//
// The decisions are the reason this is a class rather than a helper. A probe that reports the GPU
// "not reset" forces the caller's requested mode to zero regardless of what it asked for, and a
// display mode that was 1 before the reset takes one extra finalize step. Both are the retail
// body's behaviour, and both are invisible unless you know to look for them.
class Spider1GpuReset final {
public:
  // The entry ResetGraph itself is reached through, for the native override that replaces it.
  static constexpr uint32_t resetGraphEntry = 0x80084778u;
  // Where the removed VSync(0) returned inside ResetGraph, so the field owner can recognise the
  // boundary the reset's caller is about to cross.
  static constexpr uint32_t resetGraphVsyncReturn = 0x8008479Cu;

  // Run the reset for `requestedMode`. Returns false when a step asked the host to leave, which is
  // the same propagation the rest of the frame driver uses: a bounded exit is not this owner's to
  // swallow.
  static bool reset(Core &core, uint32_t requestedMode);
};

} // namespace spider
