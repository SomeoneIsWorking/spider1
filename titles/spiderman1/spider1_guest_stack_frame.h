// spider1_guest_stack_frame.h — a bounded guest stack window for one synchronous call chain.
#pragma once

#include "core.h"

#include <cstdint>
#include <type_traits>

namespace spider {

// Moves the emulated guest stack pointer down by `bytes` for as long as it is alive, and hands back
// addresses INSIDE that window so a mode can build a retail outgoing-argument block in the guest's
// own stack frame rather than in a host allocation.
//
// THE LIFETIME IS NOT RAII, and that is deliberate. A mode driver's guest calls can be suspended by
// a cooperative field yield and resumed on a later host frame, and cancellation leaves a blocked
// guest call chain by longjmp. Neither path runs a C++ destructor on the way out, so an object that
// restored the stack pointer on scope exit could leave `r[29]` pointing into a window the guest has
// already reused. `restore()` is therefore explicit and is called on the normal return of every
// window, and the class holds nothing but a pointer and a saved word so it is trivially
// destructible and a longjmp across it is harmless. `at(offset)` is relative to the NEW `r[29]`, so
// an offset of 0 is the window's own base.
class GuestStackFrame final {
public:
  GuestStackFrame(Core &core, uint32_t bytes) : core_(&core), savedSp_(core.r[29]) {
    core_->r[29] = savedSp_ - bytes;
  }

  void restore() const {
    core_->r[29] = savedSp_;
  }

  uint32_t at(uint32_t offset) const {
    return core_->r[29] + offset;
  }

private:
  Core *core_;
  uint32_t savedSp_;
};

// A yield may longjmp across a live window, so a window must leave nothing to unwind.
static_assert(std::is_trivially_destructible_v<GuestStackFrame>);

} // namespace spider
