#include "spider1_invalid_selector_input.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_mode_decisions.h"

#include "core.h"

namespace spider {
namespace {

// The authenticated `jal` return addresses for the two calls this owner makes. Each is the address
// the guest itself recorded when it called the entry, and the port installs it so the retail body
// resumes at the instruction after its own `jal`.
constexpr uint32_t kSetupReturnPc = 0x8002C71Cu;
constexpr uint32_t kPadReadReturnPc = 0x8002C754u;

} // namespace

void Spider1InvalidSelectorInput::begin(Core &core, uint32_t fieldsToWait) {
  core.r[4] = 1;
  core.r[5] = spider1::invalidWaitArgument;
  core.r[6] = fieldsToWait;
  spider1CallGuest(core, spider1::invalidWaitSetup, kSetupReturnPc);
  startField_ = core.mem_r32(spider1::gameVblankCount);
  sawSecondKeyPressed_ = false;
  sawFirstKeyPressed_ = false;
}

bool Spider1InvalidSelectorInput::poll(Core &core) {
  spider1CallGuest(core, spider1::padRead, kPadReadReturnPc);
  const uint8_t secondKey = core.mem_r8(spider1::padState + spider1::padStateByteE0);
  const uint8_t firstKey = core.mem_r8(spider1::padState + spider1::padStateByte30);
  lastSecondKey_ = secondKey;
  lastFirstKey_ = firstKey;
  if (secondKey == 0) {
    sawSecondKeyPressed_ = true;
  }
  if (firstKey == 0) {
    sawFirstKeyPressed_ = true;
  }
  return spider1ModeInvalidInputSatisfied(sawSecondKeyPressed_,
                                          secondKey,
                                          sawFirstKeyPressed_,
                                          firstKey,
                                          core.mem_r32(spider1::gameVblankCount) - startField_,
                                          spider1::invalidInputTimeoutFields);
}

} // namespace spider
