#pragma once

#include <cstdint>
#include <map>
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

// ---- The libcd response queue, read out of SLUS_008.75
// --------------------------------------------
//
// Producer 0x80085084 calls the tail-call wrapper 0x80086C60 with `a1 = sp+0x30`, which is stock
// `CdReady` 0x8008CBC4, and then at 0x800850B0 tests bit 0x04 of that result's first byte
// (`lbu v0,0x30(sp)` at 0x80085098, `lhu v0,0x22(sp)` at 0x800850A8, `andi v0,v0,0x4` at
// 0x800850B0). A set bit writes reason 3 to [0x800B1000] at 0x800850BC and returns before any DMA
// or STR-header check, so the ring never publishes and no second movie field arrives.
//
// The byte is NOT a host-written status. CdReady fills the caller's buffer from the 8-byte response
// the guest's OWN libcd ISR staged: xrefs on 0x800C6384 are 3 writes inside FUN_8008C3E0 and 1 read
// inside FUN_8008CBC4, with zero writes anywhere in the framework. That ISR reads those bytes from
// the CD data port at 0x8008C45C..0x8008C498 (`lui/ lw [0x800B3DDC]` = 0x1F801801, looped to 8).
//
// The three bytes below are that ISR's queue discipline, every store of which is a literal:
//   0x8008C754 [0x800B3DF0] = 2 (5 when s1 != 0)   0x8008C7B8 [0x800B3DF1] = 1 (5 when s1 != 0)
//   0x8008C824 [0x800B3DF2] = 4, mirrored into [0x800B3DF1]
//   0x8008C8A4 [0x800B3DF1] = 5, mirrored into [0x800B3DF0]
//   0x8008D3AC..0x8008D3B8 (the sync path) clears [0x800B3DF2] and mirrors it into [0x800B3DF1]
// and CdReady picks WHICH staged buffer it copies by reading them (0x8008CDB4 and 0x8008CE04).
inline constexpr uint32_t cdResponseQueueByte0 = 0x800B3DF0u;
inline constexpr uint32_t cdResponseQueueByte1 = 0x800B3DF1u;
inline constexpr uint32_t cdResponseQueueByte2 = 0x800B3DF2u;
inline constexpr uint32_t cdStagedResponse = 0x800C6384u; // 8 bytes, staged by the guest libcd ISR
inline constexpr uint32_t cdStagedResponseAlt = 0x800C638Cu;    // the [0x800B3DF2] arm's buffer
inline constexpr uint32_t cdStreamProducerReason = 0x800B1000u; // written by producer 0x800850BC
inline constexpr uint32_t cdReadyResponsePort = 0x1F801801u;    // the cell [0x800B3DDC] names

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

  // What the guest's own libstr producer DECIDED, read back from guest memory rather than inferred
  // from the ring. `[0x800B1000]` is written only by producer 0x800850BC, and the guard's input is
  // the staged response's first byte, so these four words together say whether a sector was refused
  // by the 0x04 guard and what the guest had staged when it decided.
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
    // The delivery gate itself. `Core::irqStatLatch` folds `cdc.irq_edge` into `hle.i_stat` bit 2
    // and `Hle::irqPoll` delivers `i_stat & i_mask` to the guest's registered element chain, so
    // "the controller has an INT1" and "the guest has been told" are different facts and both are
    // reported rather than one standing in for the other.
    uint32_t iStat = 0;
    uint32_t iMask = 0;
    uint32_t irqChainLength = 0;
    uint32_t irqHandler = 0;
    uint32_t pendingWork = 0;

    bool operator==(const ProducerObservation &) const = default;
  };

  void install();
  // First poll, a changed control/status/CDC response, or a periodic fallback returns a
  // snapshot for the existing ring logger. Unchanged hot polls do not walk or print all slots.
  std::optional<RingObservation> sampleRingIfChanged();
  static Spider1StreamDriver &from(Core &core);

  // How many times the producer wrote a reason this run, and how many times each distinct value was
  // seen. The denominator a "the guard fired" line needs, because one line is one sample.
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

} // namespace spider1

} // namespace spider
