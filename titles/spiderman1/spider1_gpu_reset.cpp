#include "spider1_gpu_reset.h"

#include "native_execution.h"

#include "core.h"

namespace spider::spider1 {
namespace {

// Enter `entry` as if ResetGraph had `jal`'d it from `returnPc`.
bool callResetStep(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  return dispatchGuestOrPropagate(core, entry);
}

// ResetGraph GPU steps in call order; the addresses are not monotonic.
constexpr uint32_t kBeginReset = 0x8008C000u;
constexpr uint32_t kReadDisplayMode = 0x80084E00u;
constexpr uint32_t kProbeResetState = 0x8008C010u;
constexpr uint32_t kApplyReset = 0x8008C020u;
constexpr uint32_t kStep0 = 0x8008C210u;
constexpr uint32_t kStep1 = 0x8008C10Cu;
constexpr uint32_t kStep2 = 0x8008C1A0u;
constexpr uint32_t kStep3 = 0x8008C030u;
constexpr uint32_t kFinalizeDisplayMode1 = 0x800848F0u;

// Display mode read back, and the one that takes the extra finalize step.
constexpr uint32_t kDisplayModeOne = 1u;
constexpr uint32_t kNoMode = 0u;

} // namespace

bool Spider1GpuReset::reset(Core &core, uint32_t requestedMode) {
  uint32_t mode = requestedMode;
  if (!callResetStep(core, kBeginReset, 0x80084794u) ||
      !callResetStep(core, kReadDisplayMode, 0x800847A4u)) {
    return false;
  }
  const uint32_t priorMode = core.r[2];
  if (!callResetStep(core, kProbeResetState, 0x800847ACu)) {
    return false;
  }
  // A probe reporting "not reset" discards the requested mode.
  if (core.r[2] == kNoMode) {
    mode = kNoMode;
  }
  core.r[4] = mode;
  if (!callResetStep(core, kApplyReset, 0x800847C0u) || !callResetStep(core, kStep0, 0x800847C8u) ||
      !callResetStep(core, kStep1, 0x800847D0u) || !callResetStep(core, kStep2, 0x800847D8u) ||
      !callResetStep(core, kStep3, 0x800847E0u)) {
    return false;
  }
  if (priorMode == kDisplayModeOne) {
    (void)callResetStep(core, kFinalizeDisplayMode1, 0x800847F4u);
  }
  return true;
}

} // namespace spider::spider1
