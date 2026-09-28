// spider1_cd_initialization.h — the CdInit body SLUS_008.75's boot reaches, as this port owes it.
#pragma once

#include "spider1_platform_facts.h"

#include <cstdint>

class Core;

namespace spider {

// Everything the retail `CdInit` body does that this port's replacement has to reproduce, and
// nothing else.
//
// It is a concept rather than two loose helpers because the two halves are one replacement: a
// native override that installs the four callback slots but drops the interrupt arm is not a
// faithful CdInit, it is a CdInit with a piece of it missing, and that is exactly the defect issue
// 0026's predecessor shipped. Keeping them under one name makes "did we reproduce all of it?" a
// question about this file rather than about a diff.
//
// The halves are separable for TESTING, not for ownership. The callback slots are plain guest RAM
// and a hermetic test can drive them against a bare `Core`; the arm writes a hardware register,
// which only the framework's memory path can do. So the arm's ARITHMETIC is a pure function here
// and the device write that applies it is not — and the shipping path through the real native
// override is exercised by `spider1_runtime_services`, which is the test that builds a `Game`.
class Spider1CdInitialization final {
public:
  // The whole replacement: install the four callback slots, report success, and arm the CD
  // interrupt.
  static void install(Core &core);

  // The slot half alone. Guest RAM only, so a hermetic test can drive it without a `Game`.
  static void installCallbackSlots(Core &core);

  // The arm's whole arithmetic, as a rule: the CD bit OR-ed into the mask the guest ALREADY holds.
  // Nothing else may change, because a literal write would clear whatever VBlank and DMA enables
  // the guest had already asked for. Whether the bit was already set is what decides whether the
  // device is written at all, and the caller is the one that reads the device back to find out.
  static constexpr uint32_t armedMask(uint32_t before) {
    return before | spider1::interruptMaskCdBit;
  }
};

} // namespace spider
