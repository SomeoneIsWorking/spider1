// spider1_host_stepped_fiber.h — the finite fiber this title runs its boot and its mode steps on.
#pragma once

#include "coro.h"
#include "host_turn.h"

#include <cstdint>
#include <functional>
#include <memory>

class Core;

namespace spider {

// Why a fiber is running. There are exactly two reasons, and the distinction is load-bearing: the
// boot fiber is the only one that may be held by the elapsed-time host turn, and only the boot
// phase answers to the post-logo wait's per-field pad-service yield.
enum class Spider1FiberPhase : std::uint8_t {
  None,
  Boot,
  Mode,
};

// The one owner of Spider-Man 1's guest code that does not return.
//
// The retail boot prefix and the mode loops are non-returning guest bodies, so the port runs them
// as host-stepped fibers: guest code runs until it reaches a title display-field boundary, the
// fiber yields there, the host delivers the field and presents a fence, and the fiber resumes on
// the next host frame. Everything about that — which phase is live, whether a field is outstanding,
// whether the fiber may be resumed, whether it may be finished — is one piece of bookkeeping, and
// it was spread across the frame driver as six members and five methods interleaved with pad
// service, widescreen latching, and presentation commits.
//
// What this class deliberately does NOT own: the field delivery itself, the presentation fence, and
// what the fiber's body does. Those belong to the frame driver, which is the only thing that knows
// about audio, the presentation sink, and the mode driver.
class Spider1HostSteppedFiber final {
public:
  Spider1HostSteppedFiber() = default;
  // A live fiber is cancelled and a live host turn shut down: the driver's destruction must not
  // leave the runtime holding a callback into a dead object.
  ~Spider1HostSteppedFiber();
  Spider1HostSteppedFiber(const Spider1HostSteppedFiber &) = delete;
  Spider1HostSteppedFiber &operator=(const Spider1HostSteppedFiber &) = delete;

  bool active() const {
    return fiber_ != nullptr;
  }
  // A fiber that is not active is not "done": it never existed, and reading it as finished is how a
  // missing owner turns into a silent success.
  bool done() const {
    return fiber_ != nullptr && fiber_->done();
  }
  bool runningOn(const Core &core) const {
    return core_ == &core;
  }
  Spider1FiberPhase phase() const {
    return phase_;
  }
  // A fiber blocked at a field boundary the host has not satisfied yet.
  bool fieldWaitOutstanding() const {
    return fieldWaiting_;
  }
  // A fiber whose outstanding field has already been delivered this step, so the next wait may not
  // consume another one.
  bool fieldSatisfied() const {
    return fieldSatisfied_;
  }

  // Start the finite boot prefix, which is the ONE phase the elapsed-time host turn exists to pace.
  // `turn` is the callback the turn raises — the frame driver's, because yielding a boot fiber at a
  // field boundary needs the frame driver's own checks — and `fieldRateMilliHz` is the display
  // field rate the turn runs at.
  void beginBoot(Core &core,
                 psx::cpu::HostTurnFunction turn,
                 unsigned fieldRateMilliHz,
                 const std::function<void()> &body);
  // Start one mode step, which is paced by the mode's own field waits and by nothing else.
  void beginModeStep(Core &core, const std::function<void()> &body);

  // Hand the turn back: after this, only a mode's explicit field waits may yield the fiber. This is
  // the single handoff from the boot turn to the native field owner, and it happens once.
  void shutdownBootstrapHostTurn();

  // Block the fiber at a title field boundary. `returnPc` is only reported, so a refusal can name
  // where the guest asked to wait.
  void yieldField(Core &core, uint32_t returnPc);

  // Run the fiber until it finishes or blocks again. Refuses to resume an absent or completed one.
  void resume();

  // Retire a completed fiber, refusing unless it really is complete and in `expected`. The boot
  // handoff depends on that refusal: finishing an incomplete fiber would drop guest work on the
  // floor.
  void finish(Spider1FiberPhase expected);

  // Retire a completed mode fiber at the end of a host step, with no phase to check.
  void drop();

  // Clear the outstanding field wait, once its field has been dealt with, and start a fresh step.
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

} // namespace spider
