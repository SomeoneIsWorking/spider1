#include "spider1_mode_driver.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_decisions.h"
#include "spider1_mode_frame_boundary.h"

#include "core.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spider {
namespace {

// The guest stack window the primary mode builds its outgoing argument blocks in. It is the size
// the retail body of that mode uses, and it is restored explicitly on the normal return: a mode's
// guest call can be suspended by a field yield, so nothing may depend on a destructor to put the
// stack pointer back.
constexpr uint32_t kPrimaryGuestStackBytes = 24u;

// The authenticated `jal` return addresses the outer cycle's own call sites use, in call order.
constexpr uint32_t kOuterCycleBeginModeReturnPc = 0x8002C3A8u;
constexpr uint32_t kReadArgumentsOpenReturnPc = 0x8002C3E0u;
constexpr uint32_t kReadArgumentsReadReturnPc = 0x8002C404u;
constexpr uint32_t kReadArgumentsCloseReturnPc = 0x8002C40Cu;
constexpr uint32_t kPrepareModeArgumentReturnPc = 0x8002C420u;

constexpr uint32_t kRestartWithModeFiveReturnPc = 0x8002C560u;
constexpr uint32_t kEnterMenuReturnPc = 0x8002C490u;
constexpr uint32_t kEnterLevelReturnPc = 0x8002C600u;
constexpr uint32_t kCycleSelectionReturnPc = 0x8002C5A4u;
constexpr uint32_t kResourceOuterModeReturnPc = 0x8002C508u;
constexpr uint32_t kResourceBeginReturnPc = 0x8002C510u;
constexpr uint32_t kResourceFindFirstReturnPc = 0x8002C518u;
constexpr uint32_t kResourceTransformReturnPc = 0x8002C52Cu;
constexpr uint32_t kResourceFinishReturnPc = 0x8002C538u;
constexpr uint32_t kResourceFindSecondReturnPc = 0x8002C540u;
constexpr uint32_t kResourceBindReturnPc = 0x8002C550u;
constexpr uint32_t kTransitionThenOuterReturnPc = 0x8002C58Cu;
constexpr uint32_t kPrimaryAfterResetReturnPc = 0x8002C4ECu;
constexpr uint32_t kPrimaryAfterResetBeginReturnPc = 0x8002C4F4u;

// The primary mode's own call sites.
constexpr uint32_t kPrimaryPrepareReturnPc = 0x8002C184u;
constexpr uint32_t kPrimaryResetReturnPc = 0x8002C198u;
constexpr uint32_t kPrimaryOptionalInitReturnPc = 0x8002C1CCu;
constexpr uint32_t kPrimaryTableInitReturnPc = 0x8002C1E0u;
constexpr uint32_t kPrimaryPreFrameReturnPc = 0x8002C1F8u;
constexpr uint32_t kPrimaryFrameBeginReturnPc = 0x8002C208u;
constexpr uint32_t kPrimaryPoolRotateReturnPc = 0x8002C210u;
constexpr uint32_t kPrimaryLogicReturnPc = 0x8002C218u;
constexpr uint32_t kPrimaryAudioStateReturnPc = 0x8002C230u;
constexpr uint32_t kPrimaryRenderWalkReturnPc = 0x8002C238u;
constexpr uint32_t kPrimaryRenderTailReturnPc = 0x8002C240u;
constexpr uint32_t kPrimaryOtRelinkReturnPc = 0x8002C258u;
constexpr uint32_t kPrimaryDrawSyncReturnPc = 0x8002C29Cu;
constexpr uint32_t kPrimaryFieldServiceReturnPc = 0x8002C28Cu;
constexpr uint32_t kPrimarySubmitReturnPc = 0x8002C2ACu;
constexpr uint32_t kPrimaryHandshakeFieldServiceReturnPc = 0x8002C2C8u;
constexpr uint32_t kPrimaryExitTestReturnPc = 0x8002C2E4u;
constexpr uint32_t kPrimaryTeardownReturnPc = 0x8002C308u;
constexpr uint32_t kPrimaryAudioTeardownReturnPc = 0x8002C310u;
constexpr uint32_t kPrimarySpecialExitReturnPc = 0x8002C334u;
constexpr uint32_t kPrimaryFinalizeReturnPc = 0x8002C33Cu;
constexpr uint32_t kPrimaryReleaseReturnPc = 0x8002C344u;

// The level route's own call sites.
constexpr uint32_t kLevelLookupReturnPc = 0x8002C61Cu;
constexpr uint32_t kLevelAltIndexReturnPc = 0x8002C62Cu;
constexpr uint32_t kLevelAltCommitReturnPc = 0x8002C654u;
constexpr uint32_t kLevelIndexReturnPc = 0x8002C664u;
constexpr uint32_t kLevelCommitReturnPc = 0x8002C694u;
constexpr uint32_t kLevelObjectReturnPc = 0x8002C6C0u;

} // namespace

Spider1ModeDriver::Spider1ModeDriver(Spider1ModeHost &host)
    : host_(host), menu_(host), alternate_(host), wipe_(host) {}

void Spider1ModeDriver::start(Core &core) {
  if (state_ != State::Dormant) {
    lucent::error("frame", "Spider-Man 1 outer driver was started more than once");
    std::abort();
  }
  enterOuterCycle(core);
}

// ---------------------------------------------------------------------------------------------
// The outer cycle
// ---------------------------------------------------------------------------------------------

void Spider1ModeDriver::enterOuterCycle(Core &core) {
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kOuterCycleBeginModeReturnPc);
  core.mem_w32(spider1::outerClear, 0);
  // The outer function keeps the object it is about to open in a callee-saved register, so the
  // driver hands it over the way the retail caller left it.
  core.r[16] = spider1::modeObject;
  state_ = State::AwaitOuterReady;
}

void Spider1ModeDriver::readOuterArgumentsAndPreparePrimary(Core &core) {
  // The two flags the previous route armed are the outer function's arguments, so they go into its
  // outgoing block in the guest's own stack frame and are consumed here.
  core.mem_w32(core.r[29] + spider1::outerArgumentFirstSlot, pendingOuterArgumentFirst_);
  core.mem_w32(core.r[29] + spider1::outerArgumentSecondSlot, pendingOuterArgumentSecond_);
  pendingOuterArgumentFirst_ = 0;
  pendingOuterArgumentSecond_ = 0;

  core.r[4] = spider1::modeObject;
  core.r[5] = spider1::modeObjectOpenMode;
  spider1CallGuest(core, spider1::modeObjectOpen, kReadArgumentsOpenReturnPc);
  core.r[4] = spider1::modeObject;
  core.r[5] = spider1::modeObjectReadFrame;
  core.r[6] = core.r[29] + spider1::outerArgumentFirstSlot;
  core.r[7] = core.mem_r32(core.r[29] + spider1::outerArgumentReadResultSlot);
  spider1CallGuest(core, spider1::modeObjectRead, kReadArgumentsReadReturnPc);
  core.r[4] = spider1::modeObject;
  spider1CallGuest(core, spider1::modeObjectClose, kReadArgumentsCloseReturnPc);
  core.r[4] = spider1::modeArgument;
  core.r[5] = 0;
  core.r[6] = 0;
  spider1CallGuest(core, spider1::modePrepare, kPrepareModeArgumentReturnPc);
  initializePrimary(core);
}

void Spider1ModeDriver::dispatchPrimaryExit(Core &core) {
  const uint32_t selector = core.mem_r32(spider1::primaryModeState);
  core.mem_w32(spider1::primaryPostClear, 0);
  const Spider1OuterRoute route = spider1OuterRoute(selector);
  if (!spider1ModePreservesOuterClear(route)) {
    core.mem_w32(spider1::outerClear, 0);
  }
  switch (route) {
  case Spider1OuterRoute::RestartWithModeFive:
    restartOuterCycleWithModeFive(core);
    return;
  case Spider1OuterRoute::Menu:
    enterMenuThroughTransition(core);
    return;
  case Spider1OuterRoute::Level:
    enterLevelThroughTransition(core);
    return;
  case Spider1OuterRoute::CycleSelection:
    advanceOuterCycleSelection(core);
    return;
  case Spider1OuterRoute::ResourcePrimary:
    enterResourcePrimary(core);
    return;
  case Spider1OuterRoute::TransitionThenOuter:
    enterTransitionThenOuter(core);
    return;
  case Spider1OuterRoute::ResetThenPrimary:
    enterPrimaryAfterReset(core);
    return;
  case Spider1OuterRoute::TransitionThenOuterWithFlag:
    enterTransitionThenOuterWithFlag(core);
    return;
  case Spider1OuterRoute::Invalid:
    startInvalidRoute();
    return;
  }
  std::abort();
}
void Spider1ModeDriver::restartOuterCycleWithModeFive(Core &core) {
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kRestartWithModeFiveReturnPc);
  core.mem_w32(spider1::outerCaseValue, spider1::outerRestartCaseValue);
  enterOuterCycle(core);
}

void Spider1ModeDriver::enterMenuThroughTransition(Core &core) {
  core.mem_w32(spider1::outerCaseValue, spider1::outerTransitionCaseValue);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kEnterMenuReturnPc);
  startTransition(Spider1OuterRoute::Menu);
}

void Spider1ModeDriver::enterLevelThroughTransition(Core &core) {
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kEnterLevelReturnPc);
  startTransition(Spider1OuterRoute::Level);
}

void Spider1ModeDriver::advanceOuterCycleSelection(Core &core) {
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kCycleSelectionReturnPc);
  const uint8_t index = static_cast<uint8_t>(core.mem_r8(spider1::outerCycleIndex) + 1u);
  core.mem_w8(spider1::outerCycleIndex, index);
  const uint32_t record =
      spider1::outerCycleTable + static_cast<uint32_t>(index) * spider1::outerCycleRecordStride;
  const uint32_t value = core.mem_r32(record + spider1::outerCycleRecordObjectOffset);
  // A table entry whose object's first byte is zero is the end of the list, so the selection wraps
  // rather than walking off the end of the table.
  if (core.mem_r8(value) == 0) {
    core.mem_w8(spider1::outerCycleIndex, 0);
  }
  enterOuterCycle(core);
}

void Spider1ModeDriver::enterResourcePrimary(Core &core) {
  core.r[4] = spider1::resourceBeginModeNumber;
  spider1CallGuest(core, spider1::beginOuterMode, kResourceOuterModeReturnPc);
  spider1CallGuest(core, spider1::resourceBegin, kResourceBeginReturnPc);
  core.r[4] = spider1::resourceFindFirstId;
  spider1CallGuest(core, spider1::resourceFind, kResourceFindFirstReturnPc);
  uint32_t resource = core.r[2];
  if (resource != 0) {
    core.r[4] = resource;
    spider1CallGuest(core, spider1::resourceTransform, kResourceTransformReturnPc);
    resource = core.r[2];
  }
  spider1CallGuest(core, spider1::resourceFinish, kResourceFinishReturnPc);
  core.r[4] = spider1::resourceFindSecondId;
  spider1CallGuest(core, spider1::resourceFind, kResourceFindSecondReturnPc);
  if (core.r[2] != 0) {
    core.r[4] = core.r[2];
    core.r[5] = resource;
    spider1CallGuest(core, spider1::resourceBind, kResourceBindReturnPc);
  }
  initializePrimary(core);
}

void Spider1ModeDriver::enterTransitionThenOuter(Core &core) {
  core.mem_w32(spider1::outerCaseValue, spider1::outerTransitionCaseValue);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kTransitionThenOuterReturnPc);
  startTransition(Spider1OuterRoute::TransitionThenOuter);
}

void Spider1ModeDriver::enterTransitionThenOuterWithFlag(Core &core) {
  pendingOuterArgumentFirst_ = 1;
  core.mem_w32(spider1::outerCaseValue, spider1::outerTransitionCaseValue);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kTransitionThenOuterReturnPc);
  startTransition(Spider1OuterRoute::TransitionThenOuterWithFlag);
}

void Spider1ModeDriver::enterPrimaryAfterReset(Core &core) {
  spider1CallGuest(core, spider1::outerReset, kPrimaryAfterResetReturnPc);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::beginOuterMode, kPrimaryAfterResetBeginReturnPc);
  core.mem_w8(spider1::modeArgument + spider1::modeArgumentResultByte, 0);
  core.r[4] = spider1::modeArgument;
  core.r[5] = 0;
  core.r[6] = 0;
  spider1CallGuest(core, spider1::modePrepare, kPrepareModeArgumentReturnPc);
  initializePrimary(core);
}

void Spider1ModeDriver::startTransition(Spider1OuterRoute continuation) {
  transitionContinuation_ = continuation;
  state_ = State::TransitionFirstFrame;
}

// ---------------------------------------------------------------------------------------------
// The primary mode
// ---------------------------------------------------------------------------------------------

void Spider1ModeDriver::initializePrimary(Core &core) {
  const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
  spider1CallGuest(core, spider1::primaryPrepare, kPrimaryPrepareReturnPc);
  core.mem_w32(spider1::gameVblankCount, 0);
  core.mem_w32(spider1::kUnattributedWord4F38, 0);
  spider1CallGuest(core, spider1::primaryReset, kPrimaryResetReturnPc);
  core.mem_w32(spider1::kUnattributedWord5690, 0);
  core.mem_w32(spider1::kUnattributedWord5694, 0);
  core.mem_w32(spider1::kUnattributedWord4FEC, 0);
  core.mem_w32(spider1::kUnattributedWord5004, 0);
  core.mem_w32(spider1::primaryModeState, 0);
  if (core.mem_r32(spider1::primaryOptionalInitFlag) != 0) {
    core.r[4] = spider1::primaryOptionalInitArgument;
    spider1CallGuest(core, spider1::primaryOptionalInit, kPrimaryOptionalInitReturnPc);
  }
  state_ = State::PrimaryWarmup;
  stack.restore();
}

void Spider1ModeDriver::stepPrimaryWarmup(Core &core) {
  const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
  host_.waitFields(core, spider1::oneField);
  core.r[4] = spider1::primaryTableInitCount;
  core.r[5] = spider1::primaryTableInitStride;
  spider1CallGuest(core, spider1::primaryTableInit, kPrimaryTableInitReturnPc);
  state_ = State::PrimaryFrame;
  // The warm-up drew nothing, so this field presents the image the display is already scanning.
  host_.commitRepeatedFieldFrame(core);
  stack.restore();
}

void Spider1ModeDriver::finishPrimary(Core &core) {
  core.mem_w32(spider1::primaryTeardownFlag, 0);
  spider1CallGuest(core, spider1::primaryTeardown, kPrimaryTeardownReturnPc);
  spider1CallGuest(core, spider1::primaryAudioTeardown, kPrimaryAudioTeardownReturnPc);
  core.mem_w16(spider1::primaryReady, 1);
  if (core.mem_r32(spider1::primaryModeState) == spider1::primarySelectorSpecialExit) {
    spider1CallGuest(core, spider1::primarySpecialExit, kPrimarySpecialExitReturnPc);
  }
  spider1CallGuest(core, spider1::primaryFinalize, kPrimaryFinalizeReturnPc);
  spider1CallGuest(core, spider1::primaryRelease, kPrimaryReleaseReturnPc);
}

void Spider1ModeDriver::stepPrimary(Core &core) {
  bool exited = false;
  {
    const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
    spider1CallGuest(core, spider1::primaryPreFrame, kPrimaryPreFrameReturnPc);
    const uint32_t vblankAtFrameStart = core.mem_r32(spider1::gameVblankCount);
    spider1CallGuest(core, spider1::frameBegin, kPrimaryFrameBeginReturnPc);
    spider1CallGuest(core, spider1::poolRotate, kPrimaryPoolRotateReturnPc);
    spider1CallGuest(core, spider1::logic, kPrimaryLogicReturnPc);
    if (core.mem_r32(spider1::primaryModeState) != 0) {
      // The logic pass decided the frame is over before anything was drawn, so this host step
      // closes without presenting a frame at all.
      host_.commitUnpresentedFrame(core);
      finishPrimary(core);
      exited = true;
    } else {
      spider1CallGuest(core, spider1::audioState, kPrimaryAudioStateReturnPc);
      spider1CallGuest(core, spider1::renderWalk, kPrimaryRenderWalkReturnPc);
      spider1CallGuest(core, spider1::renderTail, kPrimaryRenderTailReturnPc);
      const uint32_t drawBuffer = core.mem_r32(spider1::currentDrawBuffer);
      core.r[4] = core.mem_r32(drawBuffer + spider1::drawBufferOtLengthOffset);
      core.r[5] = spider1::renderArgument;
      spider1CallGuest(core, spider1::otRelink, kPrimaryOtRelinkReturnPc);
      spider1ModeWaitUnadvancedField(core, host_, vblankAtFrameStart);
      core.mem_w32(spider1::frameHandshake, 0);
      spider1ModeDrainDrawFields(
          core, host_, kPrimaryDrawSyncReturnPc, kPrimaryFieldServiceReturnPc);
      spider1CallGuest(core, spider1::submitFrame, kPrimarySubmitReturnPc);
      host_.commitSubmittedFrame(core);
      spider1ModeCompleteFrameHandshake(core, kPrimaryHandshakeFieldServiceReturnPc);
      core.r[4] = core.mem_r32(spider1::primaryExitObject);
      spider1CallGuest(core, spider1::exitTest, kPrimaryExitTestReturnPc);
      if (core.r[2] != 0) {
        core.mem_w32(spider1::primaryModeState, spider1::primarySelectorRequestExit);
        finishPrimary(core);
        exited = true;
      }
    }
    stack.restore();
  }
  if (exited) {
    dispatchPrimaryExit(core);
  }
}

// ---------------------------------------------------------------------------------------------
// The 3D transition
// ---------------------------------------------------------------------------------------------

void Spider1ModeDriver::stepTransitionFirst(Core &core) {
  // The wipe starts on whichever display half the transition is leaving. That half is the only
  // thing the driver has to say about the picture: the copy, the darkening, and the row scratch
  // belong to the wipe.
  const uint16_t startOffset =
      static_cast<uint16_t>(core.mem_r32(spider1::currentDrawBuffer) == spider1::displayBuffer0
                                ? 0
                                : spider1::displayBufferHeight);
  wipe_.begin(wipeDestination(), startOffset);
  wipe_.stepFirst(core);
  state_ = State::TransitionSecondFrame;
}

void Spider1ModeDriver::stepTransitionSecond(Core &core) {
  // The wipe publishes its own completion state; what the title does with it is the driver's step,
  // and it happens after, so the menu's load state supersedes what the wipe wrote.
  switch (wipe_.stepSecond(core)) {
  case Spider1TransitionWipe::Destination::Menu:
    core.mem_w32(spider1::asyncModeState, spider1::asyncModeStateAwaitingMenu);
    state_ = State::AwaitMenuReady;
    return;
  case Spider1TransitionWipe::Destination::Level:
    prepareLevelRoute(core);
    return;
  case Spider1TransitionWipe::Destination::OuterCycle:
    enterOuterCycle(core);
    return;
  }
  std::abort();
}

Spider1TransitionWipe::Destination Spider1ModeDriver::wipeDestination() const {
  switch (transitionContinuation_) {
  case Spider1OuterRoute::Menu:
    return Spider1TransitionWipe::Destination::Menu;
  case Spider1OuterRoute::Level:
    return Spider1TransitionWipe::Destination::Level;
  case Spider1OuterRoute::TransitionThenOuter:
  case Spider1OuterRoute::TransitionThenOuterWithFlag:
    return Spider1TransitionWipe::Destination::OuterCycle;
  default:
    break;
  }
  // No route arms a wipe it cannot finish, so this is unreachable for every input the dispatch can
  // produce. It refuses rather than defaulting, because defaulting would send an unarmed wipe into
  // the outer cycle and look like a working transition.
  lucent::error("frame", "Spider-Man 1 transition completed with an invalid continuation");
  std::abort();
}

// ---------------------------------------------------------------------------------------------
// The level route
// ---------------------------------------------------------------------------------------------

void Spider1ModeDriver::prepareLevelRoute(Core &core) {
  core.r[4] = spider1::levelName;
  core.r[5] = spider1::levelLookupKey;
  spider1CallGuest(core, spider1::levelLookup, kLevelLookupReturnPc);
  if (core.r[2] != 0) {
    // The second key matched. Its visit counter is bumped, the commit publishes it, and the title
    // goes back to the OUTER cycle rather than into a mode — this key means the level is not to be
    // entered directly.
    core.r[4] = spider1::levelAltKey;
    spider1CallGuest(core, spider1::levelIndex, kLevelAltIndexReturnPc);
    const uint32_t counter = spider1::levelName + spider1::levelVisitCounterSecondKey + core.r[2];
    core.mem_w8(counter, spider1ModeSaturatingIncrement(core.mem_r8(counter)));
    spider1CallGuest(core, spider1::levelCommit, kLevelAltCommitReturnPc);
    pendingOuterArgumentSecond_ = 1;
    enterOuterCycle(core);
    return;
  }
  core.r[4] = spider1::levelName;
  spider1CallGuest(core, spider1::levelIndex, kLevelIndexReturnPc);
  if (core.r[2] != spider1::levelLookupMissing) {
    const uint32_t counter = spider1::levelName + spider1::levelVisitCounterFirstKey + core.r[2];
    core.mem_w8(counter, spider1ModeSaturatingIncrement(core.mem_r8(counter)));
  }
  spider1CallGuest(core, spider1::levelCommit, kLevelCommitReturnPc);
  state_ = State::AwaitLevelReady;
}

void Spider1ModeDriver::startAlternate() {
  alternate_.begin();
  state_ = State::AlternateFrame;
}

void Spider1ModeDriver::startInvalidRoute() {
  state_ = State::InvalidAwaitReady;
}

// ---------------------------------------------------------------------------------------------
// The mode-entry gate and the state machine
// ---------------------------------------------------------------------------------------------

bool Spider1ModeDriver::awaitModeReady(Core &core) {
  if (core.mem_r32(spider1::asyncModeState) == 0) {
    return false;
  }
  // The mode's load is still running, so this host step presents one field of whatever the display
  // was already scanning and ends without entering the mode.
  host_.waitFields(core, spider1::oneField);
  host_.commitRepeatedFieldFrame(core);
  return true;
}

void Spider1ModeDriver::step(Core &core, uint32_t) {
  while (true) {
    switch (state_) {
    case State::Dormant:
      lucent::error("frame", "Spider-Man 1 mode step ran before its finite boot prefix");
      std::abort();
    case State::AwaitOuterReady:
      if (awaitModeReady(core)) {
        return;
      }
      readOuterArgumentsAndPreparePrimary(core);
      continue;
    case State::PrimaryWarmup:
      stepPrimaryWarmup(core);
      return;
    case State::PrimaryFrame:
      stepPrimary(core);
      return;
    case State::TransitionFirstFrame:
      stepTransitionFirst(core);
      return;
    case State::TransitionSecondFrame:
      stepTransitionSecond(core);
      return;
    case State::AwaitMenuReady:
      if (awaitModeReady(core)) {
        return;
      }
      menu_.begin(core);
      state_ = State::MenuFrame;
      continue;
    case State::MenuFrame:
      if (menu_.step(core) == Spider1MenuMode::Outcome::Finished) {
        state_ = State::AwaitMenuExit;
      }
      return;
    case State::AwaitMenuExit:
      if (awaitModeReady(core)) {
        return;
      }
      if (menu_.accepted()) {
        core.r[4] = spider1::modeArgument;
        core.r[5] = 1;
        core.r[6] = 0;
        spider1CallGuest(core, spider1::modePrepare, kPrepareModeArgumentReturnPc);
        initializePrimary(core);
      } else {
        enterOuterCycle(core);
      }
      continue;
    case State::AwaitLevelReady:
      if (awaitModeReady(core)) {
        return;
      }
      if (core.mem_r32(spider1::levelAlternateFlag) == 0) {
        startAlternate();
        continue;
      }
      core.r[4] = spider1::levelName;
      spider1CallGuest(core, spider1::levelObject, kLevelObjectReturnPc);
      if (core.r[2] == 0 || (core.mem_r32(core.r[2] + spider1::levelObjectFlagWordOffset) &
                             spider1::levelObjectAlternateBit) != 0) {
        startAlternate();
        continue;
      }
      core.r[4] = spider1::modeArgument;
      core.r[5] = 0;
      core.r[6] = 1;
      spider1CallGuest(core, spider1::modePrepare, kPrepareModeArgumentReturnPc);
      initializePrimary(core);
      continue;
    case State::AlternateFrame:
      if (alternate_.step(core) == Spider1AlternateMode::Outcome::RestartPrimary) {
        core.r[4] = spider1::modeArgument;
        core.r[5] = 0;
        core.r[6] = 1;
        spider1CallGuest(core, spider1::modePrepare, kPrepareModeArgumentReturnPc);
        initializePrimary(core);
      }
      return;
    case State::InvalidAwaitReady:
      if (awaitModeReady(core)) {
        return;
      }
      invalidInput_.begin(core, spider1::invalidInputWaitFields);
      state_ = State::InvalidInput;
      continue;
    case State::InvalidInput:
      if (invalidInput_.poll(core)) {
        enterOuterCycle(core);
        continue;
      }
      host_.waitFields(core, spider1::oneField);
      host_.commitRepeatedFieldFrame(core);
      return;
    }
  }
}

} // namespace spider
