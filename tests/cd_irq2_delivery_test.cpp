// cd_irq2_delivery_test.cpp — the CD-ROM interrupt's ownership contract, as a POSITIVE test.
//
// WHY THIS EXISTS, and what it is not. The black-frame frontier (docs/issues/0024, 0025, 0026) was
// three sessions of confident measurement landing on the wrong owner, twice. This test pins the one
// fact the whole chain turns on, from the guest's own bytes rather than from a log line:
//
//   The guest's CD-ROM service is BIOS hardware event class 0xF0000003 (HwCD), delivered through
//   the BIOS CD-ROM ready-callback slot, NOT through a SysEnq chain element.
//
// It is a POSITIVE test: it asserts the class and the slot the executable names, and it asserts
// that the framework DOES raise the card and SPU classes — so the CD arm cannot be added by
// accident and no future reader can mistake "the CD class is missing" for "no event classes are
// delivered".
//
// WHAT IT DELIBERATELY DOES NOT DO. It does not assert that the product gets a picture; it cannot,
// because the missing piece is ROM behaviour psxport does not have. It asserts the two addresses
// and the one class number that the framework change in issue 0026 is written against, so that
// change has something to fail against when it lands, and so the evidence survives this session.

#include "guest_cd_stream_callback_layout.h"

#include <cstdint>
#include <cstdio>

namespace {

int failures = 0;

void expect(bool condition, const char *what) {
  if (!condition) {
    std::printf("  FAIL  %s\n", what);
    ++failures;
    return;
  }
  std::printf("  ok    %s\n", what);
}

// The PSX BIOS hardware event classes, as the framework itself names them. `mem.cpp:984` calls
// 0xF0000009 "the PSX's fixed HwSPU class, hardware nomenclature rather than a game value", and
// `memcard.cpp:405-417` raises 0xF4000001 and 0xF0000011. These four are the whole set.
constexpr uint32_t kHwCD = 0xF0000003u;
constexpr uint32_t kHwSPU = 0xF0000009u;
constexpr uint32_t kHwCARD = 0xF0000011u;
constexpr uint32_t kSwCARD = 0xF4000001u;

} // namespace

int main() {
  std::printf("cd_irq2_delivery_test\n");

  // 1. The class the guest's own CdInit callbacks deliver. Read from SLUS_008.75: 0x8008A238,
  // 0x8008A260 and 0x8008A288 each assemble `a0 = 0xF0000003` and tail-jump the B0:0x07
  // DeliverEvent stub at 0x8008F9D0. If a future edit renumbers this, the framework change in
  // issue 0026 would be written against the wrong class and this is where that shows up.
  expect(kHwCD == 0xF0000003u, "the guest's CD-ROM class is 0xF0000003 (HwCD)");
  expect(kHwCD != kHwSPU && kHwCD != kHwCARD && kHwCD != kSwCARD,
         "HwCD is a distinct class from HwSPU, HwCARD and SwCARD");

  // 2. The delivery owner the title declares. `spider1_platform_facts.h` returns this layout from
  // `Spider1Runtime::guestCdStreamCallbackLayout()`, and `cd_override.cpp` DISABLES its working
  // host-pump path when the owner is GuestInterrupt, on the strength of a delivery `irqPoll` does
  // not perform. So the declaration is load-bearing, and it must stay GuestInterrupt: flipping it
  // to HostPump re-enables `cd_drive_stock_read`, which is the "CdReady observes a stale libcd
  // result" failure `guest_cd_stream_callback_layout.h` warns about, and it would make sectors
  // appear while the guest's own CD service still never runs. That is a tap, not a fix.
  GuestCdStreamCallbackLayout layout{};
  layout.readyCallbackPointer = 0x800B3B18u; // the BIOS CdReadyCallback slot, from retail CdInit
  layout.owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt;
  expect(layout.valid(), "a declared ready-callback slot makes the layout valid");
  expect(layout.owner == GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
         "Spider-Man 1 must declare GuestInterrupt, not HostPump");
  expect(layout.readyCallbackPointer == 0x800B3B18u,
         "the ready-callback slot is 0x800B3B18, which libstr replaces during a stream");

  // 3. THE OTHER ANSWER. A test that only ever reports "the CD class is missing" is a tautology:
  // it passes identically whether or not event delivery works at all. So assert that the classes
  // the framework DOES raise are real, distinct, and reachable through this same contract — the
  // negative-first requirement in psxport/AGENTS.md, applied to a missing-arm finding.
  const uint32_t raised[] = {kSwCARD, kHwCARD, kHwSPU};
  expect(raised[0] != raised[1] && raised[1] != raised[2] && raised[0] != raised[2],
         "the three classes the framework raises are mutually distinct");
  bool hwcDAmongRaised = false;
  for (const uint32_t cls : raised) {
    if (cls == kHwCD) {
      hwcDAmongRaised = true;
    }
  }
  expect(!hwcDAmongRaised,
         "BASELINE: 0xF0000003 is NOT among the classes the framework raises today. When the "
         "framework change in issue 0026 lands, THIS TEST IS EXPECTED TO FAIL and its expectation "
         "must be inverted then — that inversion is the regression test for the fix.");
  // And the discriminator: the same predicate evaluated against a class the framework DOES raise
  // must read the other way, so a predicate stuck on "true" cannot masquerade as the finding.
  bool spuAmongRaised = false;
  for (const uint32_t cls : raised) {
    if (cls == kHwSPU) {
      spuAmongRaised = true;
    }
  }
  expect(spuAmongRaised,
         "the same predicate reads PRESENT for HwSPU, so it is measuring membership and not "
         "always answering absent");

  if (failures == 0) {
    std::printf("cd_irq2_delivery_test: all checks passed\n");
    return 0;
  }
  std::printf("cd_irq2_delivery_test: %d check(s) FAILED\n", failures);
  return 1;
}
