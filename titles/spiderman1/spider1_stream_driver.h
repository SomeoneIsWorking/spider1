#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace spider {

// Yield a retail StGetNext dry poll to Spider-Man 1's native field owner. The guest still receives
// its original "not ready" result; this only gives the host the display field during which the
// asynchronous drive and SPU would progress on hardware.
void spider1_stream_wait_field(Core *core);

namespace spider1 {

// Authenticated SLUS_008.75 libstr consumer entry. The override super-calls this guest body.
inline constexpr uint32_t stGetNextAddress = 0x80086B10u;
inline constexpr uint32_t ringBaseAddress = 0x800C1510u;
inline constexpr uint32_t ringWriteIndexAddress = 0x800C1514u;
inline constexpr uint32_t ringFrameStartAddress = 0x800C1518u;
inline constexpr uint32_t ringConsumerIndexAddress = 0x800C151Cu;
inline constexpr uint32_t ringSlotCountAddress = 0x800C1520u;
inline constexpr uint32_t ringSlotStride = 0x20u;
inline constexpr uint32_t ringSamplePeriod = 200000u;

// One owner per Core. The runtime's gameCtx owns its lifetime; the frame driver only delivers the
// display field requested by a dry StGetNext poll.
class Spider1StreamDriver {
public:
  explicit Spider1StreamDriver(Core &core);

  struct RingObservation {
    uint32_t base = 0;
    uint32_t slots = 0;
    uint32_t frameStart = 0;
    uint32_t consumer = 0;
    uint32_t writeIndex = 0;
    uint64_t cdIrqSequence = 0;
    uint32_t cdIrqType = 0;
    uint32_t cdDataRead = 0;
    uint16_t frameStartStatus = 0;
    uint16_t consumerStatus = 0;
    uint16_t writeStatus = 0;

    bool operator==(const RingObservation &) const = default;
  };

  void install();
  // First poll, a changed control/status/CDC response, or a periodic fallback returns a
  // snapshot for the existing ring logger. Unchanged hot polls do not walk or print all slots.
  std::optional<RingObservation> sampleRingIfChanged();
  static Spider1StreamDriver &from(Core &core);

private:
  static void stGetNext(Core *core);
  void poll(Core &core);
  RingObservation observeRing() const;

  Core &core_;
  uint64_t pollCount_ = 0;
  std::optional<RingObservation> lastObservation_;
};

} // namespace spider1

} // namespace spider
