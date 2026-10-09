#include "spider1_bootstrap_turn.h"

#include "spider1_frame_driver.h"
#include "spider1_guest_layout.h"
#include "spider1_runtime.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>

namespace spider::spider1 {

Spider1BootstrapTurn::Spider1BootstrapTurn(Game &game) : game_(game), execution_(game.core) {}

void Spider1BootstrapTurn::begin(Core &core) {
  // The retail crt0 calls main with this return address.
  core.r[31] = crt0MainReturn;
  result_ = execution_.enter(Spider1Runtime::from(core).guestProgramImage()->gameMainEntry);
}

void Spider1BootstrapTurn::step(Core &core) {
  // A spent cycle budget is a field of guest time: deliver it, then resume.
  if (result_.reason == psx::cpu::ExecutionExitReason::BudgetExhausted && result_.cycles != 0) {
    Spider1FrameDriver::from(core).deliverBootstrapWaitField(core, result_.guestPc);
    result_ = execution_.resumeAt(core.pc);
    return;
  }
  if (!Spider1Runtime::from(core).resumeBootstrapBoundary(core, result_)) {
    reportExecutionResult(core, result_, "Spider-Man 1");
    game_.presentation.commitUnpresented(&core);
    game_.run.requestEnd();
    return;
  }
  result_ = execution_.resumeAt(core.pc);
}

} // namespace spider::spider1
