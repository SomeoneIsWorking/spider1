#pragma once

#include <cstdint>
#include <map>
#include <optional>

class Core;

namespace spider::spider1 {

// Authenticated SLUS_008.75 libstr consumer entry. The override super-calls this guest body.
inline constexpr uint32_t stGetNextAddress = 0x80086B10u;
inline constexpr uint32_t ringBaseAddress = 0x800C1510u;
inline constexpr uint32_t ringWriteIndexAddress = 0x800C1514u;
inline constexpr uint32_t ringFrameStartAddress = 0x800C1518u;
inline constexpr uint32_t ringConsumerIndexAddress = 0x800C151Cu;
inline constexpr uint32_t ringSlotCountAddress = 0x800C1520u;
inline constexpr uint32_t ringSlotStride = 0x20u;
inline constexpr uint32_t ringSamplePeriod = 200000u;

// 0x8008BA00 is the guest's top-level interrupt handler and the only code that opens the service
// gate 0x800B2886; CdReady 0x8008CBC4 skips its CD poll while the gate is closed.
inline constexpr uint32_t guestInterruptHandler = 0x8008BA00u;
inline constexpr uint32_t serviceGateAddress = 0x800B2886u;
inline constexpr uint32_t serviceGateReader = 0x8008B900u;
// libstr ring producer, reached only through the CdReadyCallback slot value 0x800860B4. Reason 10
// publishes, 3 refuses.
inline constexpr uint32_t strRingProducer = 0x80085000u;
inline constexpr uint32_t cdReadyService = 0x8008CBC4u;

// libcd response queue: producer 0x80085084 calls CdReady through 0x80086C60 and tests bit 0x04 of
// the first response byte at 0x800850B0; a set bit writes reason 3 to [0x800B1000] at 0x800850BC.
inline constexpr uint32_t cdResponseQueueByte0 = 0x800B3DF0u;
inline constexpr uint32_t cdResponseQueueByte1 = 0x800B3DF1u;
inline constexpr uint32_t cdResponseQueueByte2 = 0x800B3DF2u;
inline constexpr uint32_t cdStagedResponse = 0x800C6384u; // 8 bytes, staged by the guest libcd ISR
inline constexpr uint32_t cdStagedResponseAlt = 0x800C638Cu;    // the [0x800B3DF2] arm's buffer
inline constexpr uint32_t cdStreamProducerReason = 0x800B1000u; // written by producer 0x800850BC
inline constexpr uint32_t cdReadyResponsePort = 0x1F801801u;    // the cell [0x800B3DDC] names

// One owner per Core; the frame driver only delivers the display field a dry StGetNext poll
// requests.
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

  // What the guest's libstr producer decided, read back from guest memory. [0x800B1000] is written
  // only at 0x800850BC.
  struct ProducerObservation {
    uint32_t reason = 0;
    uint8_t queueByte0 = 0;
    uint8_t queueByte1 = 0;
    uint8_t queueByte2 = 0;
    uint8_t stagedFirst = 0;
    uint8_t stagedAltFirst = 0;
    uint32_t cdcIrqSequence = 0;
    uint32_t cdcIrqType = 0;
    uint32_t cdcIrqEdge = 0;
    uint32_t cdcDataRead = 0;
    uint32_t cdcDataAvailable = 0;
    uint32_t cdcMode = 0;
    // i_stat folds cdc.irq_edge (bit 2); the guest is only told when i_stat & i_mask delivers.
    uint32_t iStat = 0;
    uint32_t iMask = 0;
    uint32_t irqChainLength = 0;
    uint32_t irqHandler = 0;
    uint32_t pendingWork = 0;

    bool operator==(const ProducerObservation &) const = default;
  };

  void install();
  // Returns a snapshot on the first poll, a changed control/status/CDC response, or a periodic
  // fallback.
  std::optional<RingObservation> sampleRingIfChanged();
  static Spider1StreamDriver &from(Core &core);

  // Producer reason writes this run, and per distinct value.
  uint64_t producerObservationCount() const {
    return producerSamples_;
  }
  uint64_t producerReasonCount(uint32_t reason) const {
    const auto found = producerReasons_.find(reason);
    return found == producerReasons_.end() ? 0 : found->second;
  }

private:
  static void stGetNext(Core *core);
  void poll(Core &core);
  RingObservation observeRing() const;
  ProducerObservation observeProducer() const;
  void reportProducerIfChanged();

  Core &core_;
  uint64_t pollCount_ = 0;
  std::optional<RingObservation> lastObservation_;
  std::optional<ProducerObservation> lastProducer_;
  uint64_t producerSamples_ = 0;
  std::map<uint32_t, uint64_t> producerReasons_;
};

} // namespace spider::spider1
