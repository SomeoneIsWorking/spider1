// Spider-Man 1's continuous CD read boundary. The guest polls StGetNext during STR playback, so a
// dry poll is the source-grounded point to service the controller. The guest libcd ISR consumes
// INT1 before invoking the registered ready callback at 0x800B3B18; libstr's 0x800860B4 handler
// owns DMA and ring state. This override keeps the original StGetNext result and never edits the
// ring.
#include "spider1_stream_driver.h"

#include "cdc_state.h"
#include "core.h"
#include "execution_control.h"
#include "game.h"
#include "native_execution.h"
#include "spider1_runtime.h"
#include <cstdlib>
#include <lucent/log.h>

namespace spider::spider1 {
namespace {

// Authenticated StGetNext(&addr, &header) returns 0 for a ready slot and nonzero for a dry poll.
// The libstr ring controls below were recovered from StGetNext, StFreeRing, and the 0x80085000
// producer. Slot statuses are 0 free, 1 wrap, 2 ready, 3 DMA, and 4 held by the consumer.

uint16_t
observedStatus(Core &core, const Spider1StreamDriver::RingObservation &ring, uint32_t index) {
  if (!ring.base || index >= ring.slots || index >= 64u) {
    return 0xffffu;
  }
  return core.mem_r16(ring.base + index * ringSlotStride);
}

} // namespace

Spider1StreamDriver::Spider1StreamDriver(Core &core) : core_(core) {}

Spider1StreamDriver &Spider1StreamDriver::from(Core &core) {
  if (!dynamic_cast<Spider1Runtime *>(core.runtime) || !core.gameCtx) {
    lucent::error("ring", "Spider-Man 1 stream callback has no matching per-Core owner");
    std::abort();
  }
  return *static_cast<Spider1StreamDriver *>(core.gameCtx);
}

void Spider1StreamDriver::install() {
  installNativeOverride(core_, stGetNextAddress, "Spider StGetNext stream pump", stGetNext);
  lucent::info("cd", "continuous-read pump installed on StGetNext (0x80086B10)");
}

Spider1StreamDriver::RingObservation Spider1StreamDriver::observeRing() const {
  RingObservation ring;
  ring.base = core_.mem_r32(ringBaseAddress);
  ring.slots = core_.mem_r32(ringSlotCountAddress);
  ring.frameStart = core_.mem_r32(ringFrameStartAddress);
  ring.consumer = core_.mem_r32(ringConsumerIndexAddress);
  ring.writeIndex = core_.mem_r32(ringWriteIndexAddress);
  ring.cdIrqSequence = core_.game->cdc.irq_sequence;
  ring.cdIrqType = cdc_current_irq_type(&core_.game->cdc);
  ring.cdDataRead = core_.game->cdc.data_rd;
  ring.frameStartStatus = observedStatus(core_, ring, ring.frameStart);
  ring.consumerStatus = observedStatus(core_, ring, ring.consumer);
  ring.writeStatus = observedStatus(core_, ring, ring.writeIndex);
  return ring;
}

std::optional<Spider1StreamDriver::RingObservation> Spider1StreamDriver::sampleRingIfChanged() {
  ++pollCount_;
  RingObservation current = observeRing();
  if (!lastObservation_ || *lastObservation_ != current || pollCount_ % ringSamplePeriod == 0) {
    lastObservation_ = current;
    return current;
  }
  return std::nullopt;
}

Spider1StreamDriver::ProducerObservation Spider1StreamDriver::observeProducer() const {
  ProducerObservation observation;
  observation.reason = core_.mem_r32(cdStreamProducerReason);
  observation.queueByte0 = core_.mem_r8(cdResponseQueueByte0);
  observation.queueByte1 = core_.mem_r8(cdResponseQueueByte1);
  observation.queueByte2 = core_.mem_r8(cdResponseQueueByte2);
  observation.stagedFirst = core_.mem_r8(cdStagedResponse);
  observation.stagedAltFirst = core_.mem_r8(cdStagedResponseAlt);
  observation.cdcIrqSequence = static_cast<uint32_t>(core_.game->cdc.irq_sequence);
  observation.cdcIrqType = cdc_current_irq_type(&core_.game->cdc);
  observation.cdcIrqEdge = core_.game->cdc.irq_edge;
  observation.cdcDataRead = core_.game->cdc.data_rd;
  observation.cdcDataAvailable = core_.game->cdc.data_n;
  observation.cdcMode = core_.game->cdc.mode;
  observation.iStat = core_.game->hle.i_stat;
  observation.iMask = core_.game->hle.i_mask;
  observation.irqChainLength = static_cast<uint32_t>(core_.game->hle.irq_n);
  observation.irqHandler =
      core_.game->hle.irq_n > 0 ? core_.mem_r32(core_.game->hle.irq_elem[0] + 4) : 0u;
  observation.pendingWork = core_.pending_work;
  return observation;
}

void Spider1StreamDriver::reportProducerIfChanged() {
  const ProducerObservation current = observeProducer();
  ++producerSamples_;
  ++producerReasons_[current.reason];
  if (lastProducer_ && *lastProducer_ == current) {
    return;
  }
  lastProducer_ = current;
  lucent::Line line;
  line.add("reason={} (0x{:08X} seen {} of {} sample(s)) queue=[{},{},{}] staged0=0x{:02X} "
           "stagedAlt0=0x{:02X} cdcIrqSeq={} cdcIrqType={} irqEdge={} dataRead={} dataAvail={} "
           "mode=0x{:02X}",
           current.reason,
           current.reason,
           producerReasonCount(current.reason),
           producerSamples_,
           current.queueByte0,
           current.queueByte1,
           current.queueByte2,
           current.stagedFirst,
           current.stagedAltFirst,
           current.cdcIrqSequence,
           current.cdcIrqType,
           current.cdcIrqEdge,
           current.cdcDataRead,
           current.cdcDataAvailable,
           current.cdcMode);
  // The guard's own predicate, evaluated on the bytes the guest staged, so the line says which arm
  // the producer took rather than leaving the reader to re-derive it.
  line.add(" | guard 0x04 on staged0 = {}",
           (current.stagedFirst & 0x04u) != 0 ? "SET -> reason 3, sector refused"
                                              : "clear -> producer continues");
  // The delivery gate, with its own verdict: IRQ2 is bit 2 of I_STAT and the guest's chain element
  // is the one libstr registered, so a queued controller INT1 with I_STAT clear is a controller
  // event the guest has never been told about.
  line.add(" | iStat=0x{:03X} iMask=0x{:03X} IRQ2 {} chain={} handler=0x{:08X} pendingWork={}",
           current.iStat,
           current.iMask,
           (current.iStat & 0x4u) ? "LATCHED" : "CLEAR",
           current.irqChainLength,
           current.irqHandler,
           current.pendingWork);
  line.flush_debug("cdready");
}

void Spider1StreamDriver::stGetNext(Core *core) {
  from(*core).poll(*core);
}

void Spider1StreamDriver::poll(Core &core) {
  if (!callOriginalOrPropagate(core, stGetNextAddress)) {
    return;
  }
  // Non-zero means "no sector ready" — exactly when the drive owes the guest one. Pump a sector and
  // answer again. EXACTLY ONE retry: if the stream still has nothing after being pumped, "not
  // ready" is the honest answer and the guest is entitled to keep spinning. Looping here until
  // something appeared would mask a genuinely dry stream and hang inside an override instead of in
  // the game.
  if (core.r[2] != 0) {
    core.game->cd.pumpStream(&core, 1);
    // A typed exit pending after controller service must be consumed before another guest call.
    if (core.executionControl().pending() || !callOriginalOrPropagate(core, stGetNextAddress)) {
      return;
    }
    if (core.r[2] != 0) {
      // On hardware the display clock and SPU keep advancing while libstr polls for an
      // asynchronous sector. A guest runtime would otherwise spin here inside one host step,
      // eventually filling the XA ring and preventing the next movie VSync boundary forever.
      // Preserve StGetNext's "not ready" answer and yield only the field it waited through.
      spider1_stream_wait_field(&core);
    }
  }

  // The first poll and every change in producer/consumer controls, current statuses, or CDC
  // IRQ/data progress is visible on the next StGetNext call, including a ready result. The
  // periodic sample catches other slot changes without walking all 48 slots on every hot poll.
  lucent::Line ring;
  if (lucent::channel_on("ring")) {
    auto observation = sampleRingIfChanged();
    if (observation) {
      ring.add("base=0x{:08X} slots={} frameStart={} cons={} writeIdx={} cdIrqSeq={} cdIrqType={} "
               "cdDataRead={} | status: ",
               observation->base,
               observation->slots,
               observation->frameStart,
               observation->consumer,
               observation->writeIndex,
               observation->cdIrqSequence,
               observation->cdIrqType,
               observation->cdDataRead);
      if (observation->base == 0) {
        ring.add("unavailable (no ring base)");
      } else {
        for (uint32_t i = 0; i < observation->slots && i < 64u; i++) {
          ring.add("{} ", (int)(int16_t)core.mem_r16(observation->base + i * ringSlotStride));
        }
      }
    }
  }
  ring.flush_debug("ring");
  reportProducerIfChanged();
}

} // namespace spider::spider1
