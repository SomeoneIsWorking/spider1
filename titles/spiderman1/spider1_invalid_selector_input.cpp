#include "spider1_invalid_selector_input.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_mode_decisions.h"

#include "core.h"

namespace spider::spider1 {
namespace {

// Return addresses the guest recorded at its `jal`s for the two calls.
constexpr uint32_t kSetupReturnPc = 0x8002C71Cu;
constexpr uint32_t kPadReadReturnPc = 0x8002C754u;

} // namespace

void Spider1InvalidSelectorInput::begin(Core &core, uint32_t fieldsToWait) {
  core.r[4] = 1;
  core.r[5] = invalidWaitArgument;
  core.r[6] = fieldsToWait;
  enterGuestCall(core, invalidWaitSetup, kSetupReturnPc);
  startField_ = core.mem_r32(gameVblankCount);
  sawSecondKeyPressed_ = false;
  sawFirstKeyPressed_ = false;
}

bool Spider1InvalidSelectorInput::poll(Core &core) {
  enterGuestCall(core, padRead, kPadReadReturnPc);
  const uint8_t secondKey = core.mem_r8(padState + padStateByteE0);
  const uint8_t firstKey = core.mem_r8(padState + padStateByte30);
  lastSecondKey_ = secondKey;
  lastFirstKey_ = firstKey;
  if (secondKey == 0) {
    sawSecondKeyPressed_ = true;
  }
  if (firstKey == 0) {
    sawFirstKeyPressed_ = true;
  }
  return modeInvalidInputSatisfied(sawSecondKeyPressed_,
                                   secondKey,
                                   sawFirstKeyPressed_,
                                   firstKey,
                                   core.mem_r32(gameVblankCount) - startField_,
                                   invalidInputTimeoutFields);
}

} // namespace spider::spider1
