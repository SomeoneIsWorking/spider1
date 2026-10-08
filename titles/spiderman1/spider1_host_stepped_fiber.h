// The finite fiber this title runs its boot and its mode steps on.
#pragma once

#include "coro.h"
#include "host_turn.h"

#include <cstdint>
#include <functional>
#include <memory>

class Core;

namespace spider::spider1 {

// Only the boot phase is held by the host turn and answers the post-logo wait's pad-service yield.
enum class Spider1FiberPhase : std::uint8_t {
  None,
  Boot,
  Mode,
};

// Runs the non-returning boot prefix and mode loops as fibers that yield at title field boundaries.
// Field delivery and presentation fences stay with the frame driver.
class Spider1HostSteppedFiber final {
public:
  Spider1HostSteppedFiber() = default;
  // Cancels a live fiber and shuts down a live host turn so no callback outlives the driver.
  ~Spider1HostSteppedFiber();
  Spider1HostSteppedFiber(const Spider1HostSteppedFiber &) = delete;
  Spider1HostSteppedFiber &operator=(const Spider1HostSteppedFiber &) = delete;

  bool active() const {
    return fiber_ != nullptr;
  }
  // An inactive fiber is not done; reading it as finished hides a missing owner.
  bool done() const {
    return fiber_ != nullptr && fiber_->done();
  }
  bool runningOn(const Core &core) const {
    return core_ == &core;
  }
  Spider1FiberPhase phase() const {
    return phase_;
  }
  // Blocked at a field boundary the host has not satisfied.
  bool fieldWaitOutstanding() const {
    return fieldWaiting_;
  }
  // Its field was already delivered this step, so the next wait may not consume another.
  bool fieldSatisfied() const {
    return fieldSatisfied_;
  }

  // Start the boot prefix, the only phase paced by the host turn; `turn` is the frame driver's
  // callback.
  void beginBoot(Core &core,
                 psx::cpu::HostTurnFunction turn,
                 unsigned fieldRateMilliHz,
                 const std::function<void()> &body);
  // Start one mode step, paced only by the mode's own field waits.
  void beginModeStep(Core &core, const std::function<void()> &body);

  // Hand the turn back; once, after which only explicit field waits yield.
  void shutdownBootstrapHostTurn();

  // `returnPc` is only reported in refusals.
  void yieldField(Core &core, uint32_t returnPc);

  // Run until finished or blocked again.
  void resume();

  // Retire a completed fiber, refusing unless it is complete and in `expected`.
  void finish(Spider1FiberPhase expected);

  // Retire a completed mode fiber at the end of a host step.
  void drop();

  // Clear the outstanding wait once its field is handled.
  void clearFieldWait();
  void setFieldSatisfied();

private:
  void start(Core &core, Spider1FiberPhase phase, const std::function<void()> &body);

  std::unique_ptr<Coro> fiber_;
  Core *core_ = nullptr;
  Spider1FiberPhase phase_ = Spider1FiberPhase::None;
  bool fieldWaiting_ = false;
  bool fieldSatisfied_ = false;
  bool hostTurnRegistered_ = false;
};

} // namespace spider::spider1
