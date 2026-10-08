// A bounded guest stack window for one synchronous call chain.
#pragma once

#include "core.h"

#include <cstdint>
#include <type_traits>

namespace spider::spider1 {

// Lowers the guest stack pointer by `bytes` and addresses a window in it, relative to the new
// r[29]. Not RAII: a field yield longjmps out without destructors, so `restore()` is explicit.
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

// A yield may longjmp across a live window.
static_assert(std::is_trivially_destructible_v<GuestStackFrame>);

} // namespace spider::spider1
