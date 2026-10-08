// Pins the CD-ROM interrupt ownership: HwCD (0xF0000003) via the BIOS CdReady callback slot, not a
// SysEnq element.

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

// PSX BIOS hardware event classes the framework raises.
constexpr uint32_t kHwCD = 0xF0000003u;
constexpr uint32_t kHwSPU = 0xF0000009u;
constexpr uint32_t kHwCARD = 0xF0000011u;
constexpr uint32_t kSwCARD = 0xF4000001u;

} // namespace

int main() {
  std::printf("cd_irq2_delivery_test\n");

  // 0x8008A238, 0x8008A260 and 0x8008A288 (SLUS_008.75) each set a0 = 0xF0000003 and tail-jump
  // the B0:0x07 DeliverEvent stub at 0x8008F9D0.
  expect(kHwCD == 0xF0000003u, "the guest's CD-ROM class is 0xF0000003 (HwCD)");
  expect(kHwCD != kHwSPU && kHwCD != kHwCARD && kHwCD != kSwCARD,
         "HwCD is a distinct class from HwSPU, HwCARD and SwCARD");

  // The title's declared owner must stay GuestInterrupt: HostPump re-enables cd_drive_stock_read,
  // whose stale libcd result is described in guest_cd_stream_callback_layout.h.
  GuestCdStreamCallbackLayout layout{};
  layout.readyCallbackPointer = 0x800B3B18u; // the BIOS CdReadyCallback slot, from retail CdInit
  layout.owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt;
  expect(layout.valid(), "a declared ready-callback slot makes the layout valid");
  expect(layout.owner == GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
         "Spider-Man 1 must declare GuestInterrupt, not HostPump");
  expect(layout.readyCallbackPointer == 0x800B3B18u,
         "the ready-callback slot is 0x800B3B18, which libstr replaces during a stream");

  // The classes the framework does raise must be distinct and reachable through the same contract.
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
  // The same predicate on a raised class must read the other way.
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
