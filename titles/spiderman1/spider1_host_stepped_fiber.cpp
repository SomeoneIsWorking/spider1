#include "spider1_host_stepped_fiber.h"

#include "host_turn.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spider {

Spider1HostSteppedFiber::~Spider1HostSteppedFiber() {
  if (hostTurnRegistered_) {
    psx::cpu::shutdownHostTurn();
    hostTurnRegistered_ = false;
  }
  if (fiber_ && !fiber_->done()) {
    fiber_->cancel();
  }
  fiber_.reset();
}

void Spider1HostSteppedFiber::start(Core &core,
                                    Spider1FiberPhase phase,
                                    const std::function<void()> &body) {
  if (fiber_ || phase_ != Spider1FiberPhase::None) {
    lucent::error("boot",
                  "Spider-Man 1 finite {} fiber was started more than once",
                  phase == Spider1FiberPhase::Boot ? "boot" : "mode");
    std::abort();
  }
  core_ = &core;
  phase_ = phase;
  fieldWaiting_ = false;
  fieldSatisfied_ = false;
  fiber_ = std::make_unique<Coro>();
  fiber_->start(body);
}

void Spider1HostSteppedFiber::beginBoot(Core &core,
                                        psx::cpu::HostTurnFunction turn,
                                        unsigned fieldRateMilliHz,
                                        const std::function<void()> &body) {
  start(core, Spider1FiberPhase::Boot, body);
  psx::cpu::registerHostTurn(core, turn, fieldRateMilliHz);
  hostTurnRegistered_ = true;
}

void Spider1HostSteppedFiber::beginModeStep(Core &core, const std::function<void()> &body) {
  start(core, Spider1FiberPhase::Mode, body);
}

void Spider1HostSteppedFiber::shutdownBootstrapHostTurn() {
  if (!hostTurnRegistered_) {
    return;
  }
  psx::cpu::shutdownHostTurn();
  hostTurnRegistered_ = false;
  lucent::info("hostturn",
               "Spider-Man 1 bootstrap turn complete; native frame driver now owns every field");
}

void Spider1HostSteppedFiber::yieldField(Core &core, uint32_t returnPc) {
  if (!fiber_ || core_ != &core || phase_ == Spider1FiberPhase::None || fieldWaiting_ ||
      fieldSatisfied_) {
    lucent::error("frame",
                  "Spider-Man 1 field yield has no resumable finite owner: phase={} waiting={} "
                  "satisfied={} ra=0x{:08X}",
                  static_cast<unsigned>(phase_),
                  fieldWaiting_,
                  fieldSatisfied_,
                  returnPc);
    std::abort();
  }
  fieldWaiting_ = true;
  fiber_->yield();
}

void Spider1HostSteppedFiber::resume() {
  if (!fiber_ || fiber_->done()) {
    lucent::error("frame", "Spider-Man 1 attempted to resume an absent or completed finite fiber");
    std::abort();
  }
  fiber_->resume();
  if (!fiber_->done() && !fieldWaiting_) {
    lucent::error("frame", "Spider-Man 1 finite fiber returned without a field boundary");
    std::abort();
  }
}

void Spider1HostSteppedFiber::finish(Spider1FiberPhase expected) {
  if (!fiber_ || !fiber_->done() || phase_ != expected || fieldWaiting_ || fieldSatisfied_) {
    lucent::error("boot", "Spider-Man 1 attempted to finish an incomplete boot fiber");
    std::abort();
  }
  shutdownBootstrapHostTurn();
  fiber_.reset();
  core_ = nullptr;
  phase_ = Spider1FiberPhase::None;
}

void Spider1HostSteppedFiber::drop() {
  fiber_.reset();
  core_ = nullptr;
  phase_ = Spider1FiberPhase::None;
}

void Spider1HostSteppedFiber::clearFieldWait() {
  fieldWaiting_ = false;
  fieldSatisfied_ = false;
}

void Spider1HostSteppedFiber::setFieldSatisfied() {
  fieldSatisfied_ = true;
}

} // namespace spider
