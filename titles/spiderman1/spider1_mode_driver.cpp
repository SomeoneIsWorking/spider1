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

namespace spider::spider1 {
namespace {

// Guest stack window of the primary mode's outgoing argument blocks; restored explicitly because a
// field yield can suspend the guest call before any destructor runs.
constexpr uint32_t kPrimaryGuestStackBytes = 24u;

// `jal` return addresses of the outer cycle's call sites, in call order.
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

// Primary mode call sites.
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

// Level route call sites.
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

void Spider1ModeDriver::enterOuterCycle(Core &core) {
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kOuterCycleBeginModeReturnPc);
  core.mem_w32(outerClear, 0);
  // The outer function keeps the object it opens in a callee-saved register.
  core.r[16] = modeObject;
  state_ = State::AwaitOuterReady;
}

void Spider1ModeDriver::readOuterArgumentsAndPreparePrimary(Core &core) {
  // The flags the previous route armed are the outer function's arguments in its outgoing block.
  core.mem_w32(core.r[29] + outerArgumentFirstSlot, pendingOuterArgumentFirst_);
  core.mem_w32(core.r[29] + outerArgumentSecondSlot, pendingOuterArgumentSecond_);
  pendingOuterArgumentFirst_ = 0;
  pendingOuterArgumentSecond_ = 0;

  core.r[4] = modeObject;
  core.r[5] = modeObjectOpenMode;
  enterGuestCall(core, modeObjectOpen, kReadArgumentsOpenReturnPc);
  core.r[4] = modeObject;
  core.r[5] = modeObjectReadFrame;
  core.r[6] = core.r[29] + outerArgumentFirstSlot;
  core.r[7] = core.mem_r32(core.r[29] + outerArgumentReadResultSlot);
  enterGuestCall(core, modeObjectRead, kReadArgumentsReadReturnPc);
  core.r[4] = modeObject;
  enterGuestCall(core, modeObjectClose, kReadArgumentsCloseReturnPc);
  core.r[4] = modeArgument;
  core.r[5] = 0;
  core.r[6] = 0;
  enterGuestCall(core, modePrepare, kPrepareModeArgumentReturnPc);
  initializePrimary(core);
}

void Spider1ModeDriver::dispatchPrimaryExit(Core &core) {
  const uint32_t selector = core.mem_r32(primaryModeState);
  core.mem_w32(primaryPostClear, 0);
  const Spider1OuterRoute route = outerRouteFor(selector);
  if (!modePreservesOuterClear(route)) {
    core.mem_w32(outerClear, 0);
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
  enterGuestCall(core, beginOuterMode, kRestartWithModeFiveReturnPc);
  core.mem_w32(outerCaseValue, outerRestartCaseValue);
  enterOuterCycle(core);
}

void Spider1ModeDriver::enterMenuThroughTransition(Core &core) {
  core.mem_w32(outerCaseValue, outerTransitionCaseValue);
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kEnterMenuReturnPc);
  startTransition(Spider1OuterRoute::Menu);
}

void Spider1ModeDriver::enterLevelThroughTransition(Core &core) {
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kEnterLevelReturnPc);
  startTransition(Spider1OuterRoute::Level);
}

void Spider1ModeDriver::advanceOuterCycleSelection(Core &core) {
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kCycleSelectionReturnPc);
  const uint8_t index = static_cast<uint8_t>(core.mem_r8(outerCycleIndex) + 1u);
  core.mem_w8(outerCycleIndex, index);
  const uint32_t record = outerCycleTable + static_cast<uint32_t>(index) * outerCycleRecordStride;
  const uint32_t value = core.mem_r32(record + outerCycleRecordObjectOffset);
  // A table entry whose object's first byte is zero ends the list, so the selection wraps.
  if (core.mem_r8(value) == 0) {
    core.mem_w8(outerCycleIndex, 0);
  }
  enterOuterCycle(core);
}

void Spider1ModeDriver::enterResourcePrimary(Core &core) {
  core.r[4] = resourceBeginModeNumber;
  enterGuestCall(core, beginOuterMode, kResourceOuterModeReturnPc);
  enterGuestCall(core, resourceBegin, kResourceBeginReturnPc);
  core.r[4] = resourceFindFirstId;
  enterGuestCall(core, resourceFind, kResourceFindFirstReturnPc);
  uint32_t resource = core.r[2];
  if (resource != 0) {
    core.r[4] = resource;
    enterGuestCall(core, resourceTransform, kResourceTransformReturnPc);
    resource = core.r[2];
  }
  enterGuestCall(core, resourceFinish, kResourceFinishReturnPc);
  core.r[4] = resourceFindSecondId;
  enterGuestCall(core, resourceFind, kResourceFindSecondReturnPc);
  if (core.r[2] != 0) {
    core.r[4] = core.r[2];
    core.r[5] = resource;
    enterGuestCall(core, resourceBind, kResourceBindReturnPc);
  }
  initializePrimary(core);
}

void Spider1ModeDriver::enterTransitionThenOuter(Core &core) {
  core.mem_w32(outerCaseValue, outerTransitionCaseValue);
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kTransitionThenOuterReturnPc);
  startTransition(Spider1OuterRoute::TransitionThenOuter);
}

void Spider1ModeDriver::enterTransitionThenOuterWithFlag(Core &core) {
  pendingOuterArgumentFirst_ = 1;
  core.mem_w32(outerCaseValue, outerTransitionCaseValue);
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kTransitionThenOuterReturnPc);
  startTransition(Spider1OuterRoute::TransitionThenOuterWithFlag);
}

void Spider1ModeDriver::enterPrimaryAfterReset(Core &core) {
  enterGuestCall(core, outerReset, kPrimaryAfterResetReturnPc);
  core.r[4] = 0;
  enterGuestCall(core, beginOuterMode, kPrimaryAfterResetBeginReturnPc);
  core.mem_w8(modeArgument + modeArgumentResultByte, 0);
  core.r[4] = modeArgument;
  core.r[5] = 0;
  core.r[6] = 0;
  enterGuestCall(core, modePrepare, kPrepareModeArgumentReturnPc);
  initializePrimary(core);
}

void Spider1ModeDriver::startTransition(Spider1OuterRoute continuation) {
  transitionContinuation_ = continuation;
  state_ = State::TransitionFirstFrame;
}

void Spider1ModeDriver::initializePrimary(Core &core) {
  const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
  enterGuestCall(core, primaryPrepare, kPrimaryPrepareReturnPc);
  core.mem_w32(gameVblankCount, 0);
  core.mem_w32(kUnattributedWord4F38, 0);
  enterGuestCall(core, primaryReset, kPrimaryResetReturnPc);
  core.mem_w32(kUnattributedWord5690, 0);
  core.mem_w32(kUnattributedWord5694, 0);
  core.mem_w32(kUnattributedWord4FEC, 0);
  core.mem_w32(kUnattributedWord5004, 0);
  core.mem_w32(primaryModeState, 0);
  if (core.mem_r32(primaryOptionalInitFlag) != 0) {
    core.r[4] = primaryOptionalInitArgument;
    enterGuestCall(core, primaryOptionalInit, kPrimaryOptionalInitReturnPc);
  }
  state_ = State::PrimaryWarmup;
  stack.restore();
}

void Spider1ModeDriver::stepPrimaryWarmup(Core &core) {
  const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
  host_.waitFields(core, oneField);
  core.r[4] = primaryTableInitCount;
  core.r[5] = primaryTableInitStride;
  enterGuestCall(core, primaryTableInit, kPrimaryTableInitReturnPc);
  state_ = State::PrimaryFrame;
  // The warm-up drew nothing; present the image the display is already scanning.
  host_.commitRepeatedFieldFrame(core);
  stack.restore();
}

void Spider1ModeDriver::finishPrimary(Core &core) {
  core.mem_w32(primaryTeardownFlag, 0);
  enterGuestCall(core, primaryTeardown, kPrimaryTeardownReturnPc);
  enterGuestCall(core, primaryAudioTeardown, kPrimaryAudioTeardownReturnPc);
  core.mem_w16(primaryReady, 1);
  if (core.mem_r32(primaryModeState) == primarySelectorSpecialExit) {
    enterGuestCall(core, primarySpecialExit, kPrimarySpecialExitReturnPc);
  }
  enterGuestCall(core, primaryFinalize, kPrimaryFinalizeReturnPc);
  enterGuestCall(core, primaryRelease, kPrimaryReleaseReturnPc);
}

void Spider1ModeDriver::stepPrimary(Core &core) {
  bool exited = false;
  {
    const GuestStackFrame stack(core, kPrimaryGuestStackBytes);
    enterGuestCall(core, primaryPreFrame, kPrimaryPreFrameReturnPc);
    const uint32_t vblankAtFrameStart = core.mem_r32(gameVblankCount);
    enterGuestCall(core, frameBegin, kPrimaryFrameBeginReturnPc);
    enterGuestCall(core, poolRotate, kPrimaryPoolRotateReturnPc);
    enterGuestCall(core, logic, kPrimaryLogicReturnPc);
    if (core.mem_r32(primaryModeState) != 0) {
      // The logic pass ended the frame before drawing; close the host step without presenting.
      host_.commitUnpresentedFrame(core);
      finishPrimary(core);
      exited = true;
    } else {
      enterGuestCall(core, audioState, kPrimaryAudioStateReturnPc);
      enterGuestCall(core, renderWalk, kPrimaryRenderWalkReturnPc);
      enterGuestCall(core, renderTail, kPrimaryRenderTailReturnPc);
      const uint32_t drawBuffer = core.mem_r32(currentDrawBuffer);
      core.r[4] = core.mem_r32(drawBuffer + drawBufferOtLengthOffset);
      core.r[5] = renderArgument;
      enterGuestCall(core, otRelink, kPrimaryOtRelinkReturnPc);
      modeWaitUnadvancedField(core, host_, vblankAtFrameStart);
      core.mem_w32(frameHandshake, 0);
      modeDrainDrawFields(core, host_, kPrimaryDrawSyncReturnPc, kPrimaryFieldServiceReturnPc);
      enterGuestCall(core, submitFrame, kPrimarySubmitReturnPc);
      host_.commitSubmittedFrame(core);
      modeCompleteFrameHandshake(core, kPrimaryHandshakeFieldServiceReturnPc);
      core.r[4] = core.mem_r32(primaryExitObject);
      enterGuestCall(core, exitTest, kPrimaryExitTestReturnPc);
      if (core.r[2] != 0) {
        core.mem_w32(primaryModeState, primarySelectorRequestExit);
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

void Spider1ModeDriver::stepTransitionFirst(Core &core) {
  // The wipe starts on the display half the transition is leaving.
  const uint16_t startOffset = static_cast<uint16_t>(
      core.mem_r32(currentDrawBuffer) == displayBuffer0 ? 0 : displayBufferHeight);
  wipe_.begin(wipeDestination(), startOffset);
  wipe_.stepFirst(core);
  state_ = State::TransitionSecondFrame;
}

void Spider1ModeDriver::stepTransitionSecond(Core &core) {
  // The menu's load state must supersede what the wipe published.
  switch (wipe_.stepSecond(core)) {
  case Spider1TransitionWipe::Destination::Menu:
    core.mem_w32(asyncModeState, asyncModeStateAwaitingMenu);
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
  // Unreachable: no route arms a wipe it cannot finish. Refuse rather than default.
  lucent::error("frame", "Spider-Man 1 transition completed with an invalid continuation");
  std::abort();
}

void Spider1ModeDriver::prepareLevelRoute(Core &core) {
  core.r[4] = levelName;
  core.r[5] = levelLookupKey;
  enterGuestCall(core, levelLookup, kLevelLookupReturnPc);
  if (core.r[2] != 0) {
    // Second key matched: bump its counter, commit, and return to the outer cycle, not the level.
    core.r[4] = levelAltKey;
    enterGuestCall(core, levelIndex, kLevelAltIndexReturnPc);
    const uint32_t counter = levelName + levelVisitCounterSecondKey + core.r[2];
    core.mem_w8(counter, modeSaturatingIncrement(core.mem_r8(counter)));
    enterGuestCall(core, levelCommit, kLevelAltCommitReturnPc);
    pendingOuterArgumentSecond_ = 1;
    enterOuterCycle(core);
    return;
  }
  core.r[4] = levelName;
  enterGuestCall(core, levelIndex, kLevelIndexReturnPc);
  if (core.r[2] != levelLookupMissing) {
    const uint32_t counter = levelName + levelVisitCounterFirstKey + core.r[2];
    core.mem_w8(counter, modeSaturatingIncrement(core.mem_r8(counter)));
  }
  enterGuestCall(core, levelCommit, kLevelCommitReturnPc);
  state_ = State::AwaitLevelReady;
}

void Spider1ModeDriver::startAlternate() {
  alternate_.begin();
  state_ = State::AlternateFrame;
}

void Spider1ModeDriver::startInvalidRoute() {
  state_ = State::InvalidAwaitReady;
}

bool Spider1ModeDriver::awaitModeReady(Core &core) {
  if (core.mem_r32(asyncModeState) == 0) {
    return false;
  }
  // Load still running: present one field of the current display and end without entering the mode.
  host_.waitFields(core, oneField);
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
        core.r[4] = modeArgument;
        core.r[5] = 1;
        core.r[6] = 0;
        enterGuestCall(core, modePrepare, kPrepareModeArgumentReturnPc);
        initializePrimary(core);
      } else {
        enterOuterCycle(core);
      }
      continue;
    case State::AwaitLevelReady:
      if (awaitModeReady(core)) {
        return;
      }
      if (core.mem_r32(levelAlternateFlag) == 0) {
        startAlternate();
        continue;
      }
      core.r[4] = levelName;
      enterGuestCall(core, levelObject, kLevelObjectReturnPc);
      if (core.r[2] == 0 ||
          (core.mem_r32(core.r[2] + levelObjectFlagWordOffset) & levelObjectAlternateBit) != 0) {
        startAlternate();
        continue;
      }
      core.r[4] = modeArgument;
      core.r[5] = 0;
      core.r[6] = 1;
      enterGuestCall(core, modePrepare, kPrepareModeArgumentReturnPc);
      initializePrimary(core);
      continue;
    case State::AlternateFrame:
      if (alternate_.step(core) == Spider1AlternateMode::Outcome::RestartPrimary) {
        core.r[4] = modeArgument;
        core.r[5] = 0;
        core.r[6] = 1;
        enterGuestCall(core, modePrepare, kPrepareModeArgumentReturnPc);
        initializePrimary(core);
      }
      return;
    case State::InvalidAwaitReady:
      if (awaitModeReady(core)) {
        return;
      }
      invalidInput_.begin(core, invalidInputWaitFields);
      state_ = State::InvalidInput;
      continue;
    case State::InvalidInput:
      if (invalidInput_.poll(core)) {
        enterOuterCycle(core);
        continue;
      }
      host_.waitFields(core, oneField);
      host_.commitRepeatedFieldFrame(core);
      return;
    }
  }
}

} // namespace spider::spider1
