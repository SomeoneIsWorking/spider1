#include "spider1_mode_driver.h"

#include "core.h"
#include "recomp_iface.h"

#include <cstdlib>
#include <lucent/log.h>
#include <type_traits>

namespace spider {
namespace {

// SLUS_008.75 outer-main and mode facts. No address in this file applies to Enter Electro.
constexpr uint32_t kAsyncModeState = 0x800B5464u;
constexpr uint32_t kGameVblankCount = 0x800B5468u;
constexpr uint32_t kPrimaryModeState = 0x800B4F34u;
constexpr uint32_t kPrimaryPostClear = 0x800B56F8u;
constexpr uint32_t kOuterClear = 0x800B53D4u;
constexpr uint32_t kOuterCaseValue = 0x800B486Cu;
constexpr uint32_t kOuterCycleIndex = 0x800B4F30u;
constexpr uint32_t kOuterCycleTable = 0x80098B14u;
constexpr uint32_t kModeObject = 0x800B4FD0u;
constexpr uint32_t kModeArgument = 0x800A5688u;
constexpr uint32_t kLevelName = 0x800A568Cu;
constexpr uint32_t kLevelLookupKey = 0x800B4FD8u;
constexpr uint32_t kLevelAltKey = 0x800B4FE0u;
constexpr uint32_t kLevelAlternateFlag = 0x800B4F40u;
constexpr uint32_t kFrameHandshake = 0x800B5474u;
constexpr uint32_t kCurrentDrawBuffer = 0x800B54A8u;
constexpr uint32_t kDisplayBuffer0 = 0x8009A6E4u;
constexpr uint32_t kPadState = 0x800A4DF4u;
constexpr uint32_t kInvalidWaitArgument = 0x80093C28u;
constexpr uint32_t kAlternateObjectPointer = 0x800B49A0u;
constexpr uint32_t kAlternateFile = 0x800B57E8u;
constexpr uint32_t kUiTable = 0x80097760u;
constexpr uint32_t kRenderArgument = 0x1000u;

constexpr uint32_t kBeginOuterMode = 0x8006BE28u;
constexpr uint32_t kModeObjectOpen = 0x8001B990u;
constexpr uint32_t kModeObjectRead = 0x8001BEC4u;
constexpr uint32_t kModeObjectClose = 0x8001BDCCu;
constexpr uint32_t kModePrepare = 0x8001895Cu;
constexpr uint32_t kPrimaryPrepare = 0x80047478u;
constexpr uint32_t kPrimaryReset = 0x8006AF28u;
constexpr uint32_t kPrimaryOptionalInit = 0x8005E77Cu;
constexpr uint32_t kPrimaryOptionalInitFlag = 0x800B5778u;
constexpr uint32_t kPrimaryOptionalInitArgument = 0x12A6CC58u;
constexpr uint32_t kPrimaryTableInit = 0x80060C3Cu;
constexpr uint32_t kPrimaryPreFrame = 0x8002BBCCu;
constexpr uint32_t kFrameBegin = 0x800612B8u;
constexpr uint32_t kPoolRotate = 0x8002BB9Cu;
constexpr uint32_t kLogic = 0x8002BBD4u;
constexpr uint32_t kAudioState = 0x80062CE0u;
constexpr uint32_t kRenderWalk = 0x8002BD5Cu;
constexpr uint32_t kRenderTail = 0x8002B184u;
constexpr uint32_t kOtRelink = 0x8007F930u;
constexpr uint32_t kDrawSync = 0x800819A4u;
constexpr uint32_t kFieldService = 0x8005E234u;
constexpr uint32_t kSubmitFrame = 0x80061308u;
constexpr uint32_t kExitTest = 0x80059664u;
constexpr uint32_t kExitObject = 0x800B5268u;
constexpr uint32_t kPrimaryTeardownFlag = 0x800B54A0u;
constexpr uint32_t kPrimaryTeardown = 0x800610C8u;
constexpr uint32_t kPrimaryAudioTeardown = 0x80069D3Cu;
constexpr uint32_t kPrimaryReady = 0x800B58D0u;
constexpr uint32_t kPrimarySpecialExit = 0x80018900u;
constexpr uint32_t kPrimaryFinalize = 0x80063D80u;
constexpr uint32_t kPrimaryRelease = 0x8002B88Cu;

constexpr uint32_t kTransitionCopyImage = 0x80081D10u;
constexpr uint32_t kTransitionStoreRow = 0x80081CB0u;
constexpr uint32_t kTransitionLoadRow = 0x80081C50u;
constexpr uint32_t kGuestAllocate = 0x800651C8u;
constexpr uint32_t kGuestRelease = 0x800654E8u;

constexpr uint32_t kPadRead = 0x8006B514u;
constexpr uint32_t kPadReset = 0x8006AFECu;
constexpr uint32_t kUiSetDepth = 0x80019524u;
constexpr uint32_t kUiAllocate = 0x8002BAB4u;
constexpr uint32_t kUiConstruct = 0x80016424u;
constexpr uint32_t kUiBind = 0x80016B30u;
constexpr uint32_t kUiUpdate = 0x80016CA4u;
constexpr uint32_t kUiText = 0x8001957Cu;
constexpr uint32_t kUiQuad = 0x80019A90u;
constexpr uint32_t kUiEffect = 0x8006D61Cu;
constexpr uint32_t kUiReadInput = 0x80017E14u;
constexpr uint32_t kUiFinishFrame = 0x800174CCu;
constexpr uint32_t kUiSound = 0x80063770u;
constexpr uint32_t kUiExitTest = 0x80016C64u;

constexpr uint32_t kOuterReset = 0x80017920u;
constexpr uint32_t kResourceBegin = 0x8005DAE0u;
constexpr uint32_t kResourceFind = 0x80058CE4u;
constexpr uint32_t kResourceTransform = 0x80047DF8u;
constexpr uint32_t kResourceFinish = 0x8005AC00u;
constexpr uint32_t kResourceBind = 0x8006F948u;
constexpr uint32_t kLevelLookup = 0x8005F1D4u;
constexpr uint32_t kLevelIndex = 0x80018898u;
constexpr uint32_t kLevelCommit = 0x8006F0D4u;
constexpr uint32_t kLevelObject = 0x80018800u;
constexpr uint32_t kInvalidWaitSetup = 0x80014D54u;

constexpr uint32_t kAlternateBegin = 0x800166A0u;
constexpr uint32_t kAlternateUpdate = 0x80016CA4u;
constexpr uint32_t kAlternateEffect = 0x8006D320u;
constexpr uint32_t kAlternateRelease = 0x800165FCu;

void call(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  rec_dispatch(&core, entry);
}

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

// Coro cancellation uses longjmp to leave a field-blocked guest call chain. Keep every automatic
// object that can be alive across such a yield trivially destructible; normal returns restore the
// emulated guest stack explicitly below.
static_assert(std::is_trivially_destructible_v<GuestStackFrame>);

uint16_t transitionPixel(uint16_t pixel) {
  const uint32_t sum = (pixel & 31u) + ((pixel >> 5u) & 31u) + ((pixel >> 10u) & 31u);
  const uint32_t dim = (341u * sum) >> 11u;
  const uint32_t strong = (1365u * sum) >> 12u;
  return static_cast<uint16_t>((pixel & 0x8000u) | (strong << 10u) | (dim << 5u) | dim);
}

uint32_t transitionWord(uint32_t word) {
  return static_cast<uint32_t>(transitionPixel(static_cast<uint16_t>(word))) |
         (static_cast<uint32_t>(transitionPixel(static_cast<uint16_t>(word >> 16u))) << 16u);
}

} // namespace

Spider1ModeDriver::Spider1ModeDriver(Spider1ModeHost &host) : host_(host) {}

void Spider1ModeDriver::start(Core &core) {
  if (state_ != State::Dormant) {
    lucent::error("frame", "Spider-Man 1 outer driver was started more than once");
    std::abort();
  }
  enterOuterCycle(core);
}

void Spider1ModeDriver::enterOuterCycle(Core &core) {
  core.r[4] = 0;
  call(core, kBeginOuterMode, 0x8002C3A8u);
  core.mem_w32(kOuterClear, 0);
  core.r[16] = kModeObject;
  state_ = State::AwaitOuterReady;
}

void Spider1ModeDriver::readOuterArgumentsAndPreparePrimary(Core &core) {
  core.mem_w32(core.r[29] + 16u, outerFlag21_);
  core.mem_w32(core.r[29] + 20u, outerFlag20_);
  outerFlag21_ = 0;
  outerFlag20_ = 0;

  core.r[4] = kModeObject;
  core.r[5] = 1;
  call(core, kModeObjectOpen, 0x8002C3E0u);
  core.r[4] = kModeObject;
  core.r[5] = 0;
  core.r[6] = core.r[29] + 16u;
  core.r[7] = core.mem_r32(core.r[29] + 24u);
  call(core, kModeObjectRead, 0x8002C404u);
  core.r[4] = kModeObject;
  call(core, kModeObjectClose, 0x8002C40Cu);
  core.r[4] = kModeArgument;
  core.r[5] = 0;
  core.r[6] = 0;
  call(core, kModePrepare, 0x8002C420u);
  initializePrimary(core);
}

void Spider1ModeDriver::initializePrimary(Core &core) {
  GuestStackFrame stack(core, 24);
  call(core, kPrimaryPrepare, 0x8002C184u);
  core.mem_w32(kGameVblankCount, 0);
  core.mem_w32(0x800B4F38u, 0);
  call(core, kPrimaryReset, 0x8002C198u);
  core.mem_w32(0x800B5690u, 0);
  core.mem_w32(0x800B5694u, 0);
  core.mem_w32(0x800B4FECu, 0);
  core.mem_w32(0x800B5004u, 0);
  core.mem_w32(kPrimaryModeState, 0);
  if (core.mem_r32(kPrimaryOptionalInitFlag) != 0) {
    core.r[4] = kPrimaryOptionalInitArgument;
    call(core, kPrimaryOptionalInit, 0x8002C1CCu);
  }
  state_ = State::PrimaryWarmup;
  stack.restore();
}

void Spider1ModeDriver::stepPrimaryWarmup(Core &core) {
  GuestStackFrame stack(core, 24);
  host_.waitFields(core, 1);
  core.r[4] = 32;
  core.r[5] = 8;
  call(core, kPrimaryTableInit, 0x8002C1E0u);
  state_ = State::PrimaryFrame;
  host_.commitRepeatedFieldFrame(core);
  stack.restore();
}

void Spider1ModeDriver::finishPrimary(Core &core) {
  core.mem_w32(kPrimaryTeardownFlag, 0);
  call(core, kPrimaryTeardown, 0x8002C308u);
  call(core, kPrimaryAudioTeardown, 0x8002C310u);
  core.mem_w16(kPrimaryReady, 1);
  if (core.mem_r32(kPrimaryModeState) == 3) {
    call(core, kPrimarySpecialExit, 0x8002C334u);
  }
  call(core, kPrimaryFinalize, 0x8002C33Cu);
  call(core, kPrimaryRelease, 0x8002C344u);
}

void Spider1ModeDriver::stepPrimary(Core &core) {
  bool exited = false;
  {
    GuestStackFrame stack(core, 24);
    call(core, kPrimaryPreFrame, 0x8002C1F8u);
    const uint32_t fieldAtFrameStart = core.mem_r32(kGameVblankCount);
    call(core, kFrameBegin, 0x8002C208u);
    call(core, kPoolRotate, 0x8002C210u);
    call(core, kLogic, 0x8002C218u);
    if (core.mem_r32(kPrimaryModeState) != 0) {
      host_.commitUnpresentedFrame(core);
      finishPrimary(core);
      exited = true;
    } else {
      call(core, kAudioState, 0x8002C230u);
      call(core, kRenderWalk, 0x8002C238u);
      call(core, kRenderTail, 0x8002C240u);
      const uint32_t drawBuffer = core.mem_r32(kCurrentDrawBuffer);
      core.r[4] = core.mem_r32(drawBuffer + 112u);
      core.r[5] = kRenderArgument;
      call(core, kOtRelink, 0x8002C258u);
      if (core.mem_r32(kGameVblankCount) == fieldAtFrameStart) {
        host_.waitFields(core, 1);
      }
      core.mem_w32(kFrameHandshake, 0);
      while (true) {
        host_.waitFields(core, 1);
        core.r[4] = 1;
        call(core, kDrawSync, 0x8002C29Cu);
        if (core.r[2] == 0) {
          break;
        }
        call(core, kFieldService, 0x8002C28Cu);
      }
      call(core, kSubmitFrame, 0x8002C2ACu);
      host_.commitSubmittedFrame(core);
      if (core.mem_r32(kFrameHandshake) == 0) {
        call(core, kFieldService, 0x8002C2C8u);
        core.mem_w32(kFrameHandshake, 1);
      }
      core.r[4] = core.mem_r32(kExitObject);
      call(core, kExitTest, 0x8002C2E4u);
      if (core.r[2] != 0) {
        core.mem_w32(kPrimaryModeState, 2);
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

void Spider1ModeDriver::dispatchPrimaryExit(Core &core) {
  const uint32_t selector = core.mem_r32(kPrimaryModeState);
  core.mem_w32(kPrimaryPostClear, 0);
  if (selector != 2 && selector != 9) {
    core.mem_w32(kOuterClear, 0);
  }

  switch (spider1OuterRoute(selector)) {
  case Spider1OuterRoute::RestartWithModeFive:
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C560u);
    core.mem_w32(kOuterCaseValue, 5);
    enterOuterCycle(core);
    return;
  case Spider1OuterRoute::Menu:
    core.mem_w32(kOuterCaseValue, 20);
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C490u);
    startTransition(Spider1OuterRoute::Menu);
    return;
  case Spider1OuterRoute::Level:
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C600u);
    startTransition(Spider1OuterRoute::Level);
    return;
  case Spider1OuterRoute::CycleSelection: {
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C5A4u);
    const uint8_t index = static_cast<uint8_t>(core.mem_r8(kOuterCycleIndex) + 1u);
    core.mem_w8(kOuterCycleIndex, index);
    const uint32_t record = kOuterCycleTable + static_cast<uint32_t>(index) * 184u;
    const uint32_t value = core.mem_r32(record + 180u);
    if (core.mem_r8(value) == 0) {
      core.mem_w8(kOuterCycleIndex, 0);
    }
    enterOuterCycle(core);
    return;
  }
  case Spider1OuterRoute::ResourcePrimary: {
    core.r[4] = 2;
    call(core, kBeginOuterMode, 0x8002C508u);
    call(core, kResourceBegin, 0x8002C510u);
    core.r[4] = 4128;
    call(core, kResourceFind, 0x8002C518u);
    uint32_t resource = core.r[2];
    if (resource != 0) {
      core.r[4] = resource;
      call(core, kResourceTransform, 0x8002C52Cu);
      resource = core.r[2];
    }
    call(core, kResourceFinish, 0x8002C538u);
    core.r[4] = 756;
    call(core, kResourceFind, 0x8002C540u);
    if (core.r[2] != 0) {
      core.r[4] = core.r[2];
      core.r[5] = resource;
      call(core, kResourceBind, 0x8002C550u);
    }
    initializePrimary(core);
    return;
  }
  case Spider1OuterRoute::TransitionThenOuter:
    core.mem_w32(kOuterCaseValue, 20);
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C58Cu);
    startTransition(Spider1OuterRoute::TransitionThenOuter);
    return;
  case Spider1OuterRoute::TransitionThenOuterWithFlag:
    outerFlag21_ = 1;
    core.mem_w32(kOuterCaseValue, 20);
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C58Cu);
    startTransition(Spider1OuterRoute::TransitionThenOuterWithFlag);
    return;
  case Spider1OuterRoute::ResetThenPrimary:
    call(core, kOuterReset, 0x8002C4ECu);
    core.r[4] = 0;
    call(core, kBeginOuterMode, 0x8002C4F4u);
    core.mem_w8(kModeArgument + 13u, 0);
    core.r[4] = kModeArgument;
    core.r[5] = 0;
    core.r[6] = 0;
    call(core, kModePrepare, 0x8002C420u);
    initializePrimary(core);
    return;
  case Spider1OuterRoute::Invalid:
    startInvalidRoute(core);
    return;
  }
  std::abort();
}

void Spider1ModeDriver::startTransition(Spider1OuterRoute continuation) {
  transitionContinuation_ = continuation;
  state_ = State::TransitionFirstFrame;
}

void Spider1ModeDriver::stepTransitionFirst(Core &core) {
  GuestStackFrame stack(core, 72);
  core.r[4] = 0;
  call(core, kDrawSync, 0x800604ECu);
  transitionY_ = core.mem_r32(kCurrentDrawBuffer) == kDisplayBuffer0 ? 0u : 256u;
  core.mem_w16(stack.at(16), 0);
  core.mem_w16(stack.at(18), transitionY_);
  core.mem_w16(stack.at(20), 512);
  core.mem_w16(stack.at(22), 240);
  core.r[4] = stack.at(16);
  core.r[5] = 0;
  core.r[6] = transitionY_ == 0 ? 256 : 0;
  call(core, kTransitionCopyImage, 0x80060534u);
  core.r[4] = 0;
  call(core, kDrawSync, 0x8006053Cu);
  host_.waitFields(core, 1);
  core.mem_w8(kDisplayBuffer0 + 144u, 0);
  core.mem_w8(kDisplayBuffer0 + 24u, 0);
  call(core, kFrameBegin, 0x80060550u);
  call(core, kSubmitFrame, 0x80060558u);
  state_ = State::TransitionSecondFrame;
  host_.commitSubmittedFrame(core);
  stack.restore();
}

void Spider1ModeDriver::stepTransitionSecond(Core &core) {
  {
    GuestStackFrame stack(core, 72);
    core.r[4] = 4096;
    core.r[5] = 0;
    core.r[6] = 1;
    call(core, kGuestAllocate, 0x80060568u);
    const uint32_t pixels = core.r[2];
    core.mem_w16(stack.at(40), 0);
    core.mem_w16(stack.at(42), transitionY_);
    core.mem_w16(stack.at(44), 512);
    core.mem_w16(stack.at(46), 1);
    for (uint32_t row = 0; row < 240; ++row) {
      const uint32_t rowPixels = pixels + (row & 3u) * 1024u;
      core.r[4] = stack.at(40);
      core.r[5] = rowPixels;
      call(core, kTransitionStoreRow, 0x800605C8u);
      for (uint32_t word = 0; word < 256; ++word) {
        const uint32_t address = rowPixels + word * 4u;
        core.mem_w32(address, transitionWord(core.mem_r32(address)));
      }
      core.r[4] = stack.at(40);
      core.r[5] = rowPixels;
      call(core, kTransitionLoadRow, 0x800606D4u);
      core.mem_w16(stack.at(42), static_cast<uint16_t>(core.mem_r16(stack.at(42)) + 1u));
    }
    core.r[4] = pixels;
    call(core, kGuestRelease, 0x800606F8u);
    call(core, kFrameBegin, 0x80060700u);
    call(core, kSubmitFrame, 0x80060708u);
    host_.commitSubmittedFrame(core);
    call(core, kFrameBegin, 0x80060710u);
    core.mem_w8(kDisplayBuffer0 + 144u, 1);
    core.mem_w8(kDisplayBuffer0 + 24u, 1);
    core.mem_w32(kAsyncModeState, 0x001E005Au);
    stack.restore();
  }
  finishTransition(core);
}

void Spider1ModeDriver::finishTransition(Core &core) {
  switch (transitionContinuation_) {
  case Spider1OuterRoute::Menu:
    core.mem_w32(kAsyncModeState, 10);
    state_ = State::AwaitMenuReady;
    return;
  case Spider1OuterRoute::Level:
    prepareLevelRoute(core);
    return;
  case Spider1OuterRoute::TransitionThenOuter:
  case Spider1OuterRoute::TransitionThenOuterWithFlag:
    enterOuterCycle(core);
    return;
  default:
    lucent::error("frame", "Spider-Man 1 transition completed with an invalid continuation");
    std::abort();
  }
}

void Spider1ModeDriver::initializeMenu(Core &core) {
  GuestStackFrame stack(core, 88);
  call(core, kPadRead, 0x80016114u);
  core.r[4] = kPadState;
  call(core, kPadReset, 0x80016120u);
  core.r[4] = 256;
  call(core, kUiSetDepth, 0x80016128u);

  core.mem_w16(stack.at(32), 0);
  core.mem_w16(stack.at(34), core.mem_r32(kCurrentDrawBuffer) == kDisplayBuffer0 ? 0 : 256);
  core.mem_w16(stack.at(36), 512);
  core.mem_w16(stack.at(38), 256);
  core.r[4] = stack.at(32);
  core.r[5] = 0;
  core.r[6] = core.mem_r16(stack.at(34)) == 0 ? 256 : 0;
  call(core, kTransitionCopyImage, 0x80016174u);
  core.mem_w8(kDisplayBuffer0 + 24u, 0);
  core.mem_w8(kDisplayBuffer0 + 144u, 0);

  core.r[4] = 1160;
  call(core, kUiAllocate, 0x80016184u);
  menuObject_ = core.r[2];
  if (menuObject_ != 0) {
    core.r[4] = menuObject_;
    core.r[5] = 256;
    core.r[6] = 112;
    core.r[7] = 0;
    core.mem_w32(stack.at(16), 256);
    core.mem_w32(stack.at(20), 256);
    core.mem_w32(stack.at(24), 16);
    call(core, kUiConstruct, 0x800161B4u);
    menuObject_ = core.r[2];
  }
  core.r[4] = menuObject_;
  core.r[5] = core.mem_r32(kUiTable + 808u);
  call(core, kUiBind, 0x800161D8u);
  core.r[4] = menuObject_;
  core.r[5] = core.mem_r32(kUiTable + 820u);
  call(core, kUiBind, 0x800161E4u);
  core.mem_w8(menuObject_ + 11u, 0);
  core.mem_w8(menuObject_ + 24u, 1);
  menuFrame_ = 0;
  menuResult_ = false;
  state_ = State::MenuFrame;
  stack.restore();
}

void Spider1ModeDriver::finishMenu(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, 1);
  core.r[4] = 0;
  call(core, kDrawSync, 0x80016354u);
  call(core, kSubmitFrame, 0x8001635Cu);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  call(core, kDrawSync, 0x80016364u);
  if (menuObject_ != 0) {
    const uint32_t vtable = core.mem_r32(menuObject_);
    core.r[4] = menuObject_ + static_cast<uint32_t>(static_cast<int32_t>(
                                  static_cast<int16_t>(core.mem_r16(vtable + 8u))));
    core.r[5] = 3;
    core.r[31] = 0x80016388u;
    rec_dispatch(&core, core.mem_r32(vtable + 12u));
  }
  core.mem_w8(kDisplayBuffer0 + 24u, 1);
  core.mem_w8(kDisplayBuffer0 + 144u, 1);
  call(core, kFrameBegin, 0x8001639Cu);
  core.mem_w32(kAsyncModeState, 80);
  state_ = State::AwaitMenuExit;
}

void Spider1ModeDriver::stepMenu(Core &core) {
  GuestStackFrame stack(core, 88);
  call(core, kPrimaryPreFrame, 0x800161F8u);
  call(core, kFrameBegin, 0x80016200u);
  call(core, kPoolRotate, 0x80016208u);
  const uint32_t fieldAtFrameStart = core.mem_r32(kGameVblankCount);
  core.r[4] = menuObject_;
  call(core, kUiUpdate, 0x8001621Cu);
  core.r[4] = 127;
  core.r[5] = 25;
  core.r[6] = 33;
  core.r[7] = 0;
  call(core, kUiText, 0x80016230u);
  core.r[4] = 256;
  core.r[5] = 64;
  core.r[6] = core.mem_r32(kUiTable);
  core.r[7] = 0;
  core.mem_w32(stack.at(16), 4096);
  call(core, kUiQuad, 0x8001624Cu);
  if (menuFrame_ < 2) {
    core.r[4] = 460;
    core.r[5] = 173;
    call(core, kUiEffect, 0x80016264u);
  }
  ++menuFrame_;
  call(core, kPadRead, 0x8001626Cu);
  core.r[4] = stack.at(40);
  core.r[5] = stack.at(44);
  core.r[6] = stack.at(48);
  core.r[7] = stack.at(52);
  call(core, kUiReadInput, 0x80016280u);
  core.r[4] = menuObject_;
  call(core, kUiFinishFrame, 0x80016288u);
  if (core.mem_r32(stack.at(40)) != 0) {
    core.r[4] = 21;
    core.r[5] = 8192;
    core.r[6] = 0;
    call(core, kUiSound, 0x800162A4u);
    core.r[4] = menuObject_;
    core.r[5] = core.mem_r32(kUiTable + 808u);
    call(core, kUiExitTest, 0x800162B0u);
    menuResult_ = core.r[2] != 0;
    finishMenu(core);
    stack.restore();
    return;
  }
  if (core.mem_r32(kGameVblankCount) == fieldAtFrameStart) {
    host_.waitFields(core, 1);
  }
  core.mem_w32(kFrameHandshake, 0);
  while (true) {
    host_.waitFields(core, 1);
    core.r[4] = 1;
    call(core, kDrawSync, 0x80016304u);
    if (core.r[2] == 0) {
      break;
    }
    call(core, kFieldService, 0x800162F4u);
  }
  call(core, kSubmitFrame, 0x80016314u);
  host_.commitSubmittedFrame(core);
  if (core.mem_r32(kFrameHandshake) == 0) {
    call(core, kFieldService, 0x80016330u);
    core.mem_w32(kFrameHandshake, 1);
  }
  stack.restore();
}

void Spider1ModeDriver::prepareLevelRoute(Core &core) {
  core.r[4] = kLevelName;
  core.r[5] = kLevelLookupKey;
  call(core, kLevelLookup, 0x8002C61Cu);
  if (core.r[2] != 0) {
    core.r[4] = kLevelAltKey;
    call(core, kLevelIndex, 0x8002C62Cu);
    const uint32_t counter = kLevelName + 82u + core.r[2];
    const uint8_t value = core.mem_r8(counter);
    if (value < 255) {
      core.mem_w8(counter, static_cast<uint8_t>(value + 1u));
    }
    call(core, kLevelCommit, 0x8002C654u);
    outerFlag20_ = 1;
    enterOuterCycle(core);
    return;
  }
  core.r[4] = kLevelName;
  call(core, kLevelIndex, 0x8002C664u);
  if (core.r[2] != UINT32_MAX) {
    const uint32_t counter = kLevelName + 81u + core.r[2];
    const uint8_t value = core.mem_r8(counter);
    if (value < 255) {
      core.mem_w8(counter, static_cast<uint8_t>(value + 1u));
    }
  }
  call(core, kLevelCommit, 0x8002C694u);
  state_ = State::AwaitLevelReady;
}

void Spider1ModeDriver::initializeAlternate(Core &) {
  alternateFlag_ = true;
  alternateNeedsInit_ = true;
  state_ = State::AlternateFrame;
}

bool Spider1ModeDriver::finishAlternate(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, 1);
  core.r[4] = 0;
  call(core, kDrawSync, 0x8006F484u);
  call(core, kSubmitFrame, 0x8006F48Cu);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  call(core, kDrawSync, 0x8006F494u);
  call(core, kOuterReset, 0x8006F49Cu);
  core.r[4] = 0;
  call(core, kDrawSync, 0x8006F4A4u);
  core.r[4] = kPadState;
  call(core, kPadReset, 0x8006F4ACu);
  const uint32_t object = core.mem_r32(kAlternateObjectPointer);
  core.r[4] = object;
  call(core, kAlternateRelease, 0x8006F4BCu);
  if (core.mem_r8(object + 14u) == 1) {
    core.r[4] = kAlternateFile;
    core.r[5] = 1;
    call(core, kModeObjectOpen, 0x8006F4E4u);
    core.mem_w32(core.r[29] + 24u, 1);
    core.r[4] = kAlternateFile;
    core.r[5] = 1;
    core.r[6] = core.r[29] + 24u;
    core.r[7] = core.r[29] + 28u;
    call(core, kModeObjectRead, 0x8006F4FCu);
    core.r[4] = kAlternateFile;
    call(core, kModeObjectClose, 0x8006F504u);
    if (core.mem_r32(core.r[29] + 28u) != 0) {
      alternateFlag_ = true;
      alternateNeedsInit_ = true;
      state_ = State::AlternateFrame;
      return true;
    }
  }
  return false;
}

void Spider1ModeDriver::stepAlternate(Core &core) {
  bool finished = false;
  bool repeat = false;
  {
    const uint32_t object = core.mem_r32(kAlternateObjectPointer);
    if (alternateNeedsInit_) {
      core.mem_w8(object + 14u, alternateFlag_ ? 1 : 0);
      core.r[4] = object;
      core.r[5] = 0;
      call(core, kAlternateBegin, 0x8006F2FCu);
      alternateNeedsInit_ = false;
    }
    GuestStackFrame stack(core, 72);
    call(core, kPrimaryPreFrame, 0x8006F304u);
    call(core, kFrameBegin, 0x8006F30Cu);
    call(core, kPoolRotate, 0x8006F314u);
    const uint32_t fieldAtFrameStart = core.mem_r32(kGameVblankCount);
    core.r[4] = 800;
    core.r[5] = 173;
    call(core, kUiEffect, 0x8006F32Cu);
    core.r[4] = 256;
    call(core, kUiSetDepth, 0x8006F334u);
    core.r[4] = object;
    call(core, kAlternateUpdate, 0x8006F344u);
    core.r[4] = 77;
    core.r[5] = 83;
    core.r[6] = 105;
    core.r[7] = 0;
    call(core, kUiText, 0x8006F358u);
    core.r[4] = 256;
    core.r[5] = 60;
    core.r[6] = core.mem_r32(kUiTable + 416u);
    core.r[7] = 0;
    core.mem_w32(stack.at(16), 4096);
    call(core, kUiQuad, 0x8006F374u);
    core.r[4] = 800;
    core.r[5] = 460;
    call(core, kAlternateEffect, 0x8006F380u);
    call(core, kPadRead, 0x8006F388u);
    if (core.mem_r8(kPadState + 48u) == 0) {
      alternateFlag_ = false;
      core.mem_w8(kPadState + 49u, 0);
    }
    core.r[4] = object;
    call(core, kUiFinishFrame, 0x8006F3B0u);

    bool exit = !alternateFlag_;
    if (!exit && (core.mem_r8(kPadState + 225u) != 0 || core.mem_r8(kPadState + 49u) != 0)) {
      core.r[4] = 31;
      core.r[5] = 8192;
      core.r[6] = 0;
      core.mem_w8(kPadState + 49u, 0);
      core.mem_w8(kPadState + 225u, 0);
      call(core, kUiSound, 0x8006F3ECu);
      exit = true;
    }
    if (exit) {
      repeat = finishAlternate(core);
      finished = true;
    } else {
      if (core.mem_r32(kGameVblankCount) == fieldAtFrameStart) {
        host_.waitFields(core, 1);
      }
      core.mem_w32(kFrameHandshake, 0);
      while (true) {
        host_.waitFields(core, 1);
        core.r[4] = 1;
        call(core, kDrawSync, 0x8006F438u);
        if (core.r[2] == 0) {
          break;
        }
        call(core, kFieldService, 0x8006F428u);
      }
      call(core, kSubmitFrame, 0x8006F448u);
      host_.commitSubmittedFrame(core);
      if (core.mem_r32(kFrameHandshake) == 0) {
        call(core, kFieldService, 0x8006F464u);
        core.mem_w32(kFrameHandshake, 1);
      }
    }
    stack.restore();
  }
  if (finished && !repeat) {
    core.r[4] = kModeArgument;
    core.r[5] = 0;
    core.r[6] = 1;
    call(core, kModePrepare, 0x8002C420u);
    initializePrimary(core);
  }
}

void Spider1ModeDriver::startInvalidRoute(Core &) {
  state_ = State::InvalidAwaitReady;
}

void Spider1ModeDriver::initializeInvalidInput(Core &core) {
  core.r[4] = 1;
  core.r[5] = kInvalidWaitArgument;
  core.r[6] = 240;
  call(core, kInvalidWaitSetup, 0x8002C71Cu);
  invalidStartField_ = core.mem_r32(kGameVblankCount);
  invalidSawPad224_ = false;
  invalidSawPad48_ = false;
  state_ = State::InvalidInput;
}

bool Spider1ModeDriver::pollInvalidInput(Core &core) {
  call(core, kPadRead, 0x8002C754u);
  const uint8_t pad224 = core.mem_r8(kPadState + 224u);
  const uint8_t pad48 = core.mem_r8(kPadState + 48u);
  if (pad224 == 0) {
    invalidSawPad224_ = true;
  }
  if (pad48 == 0) {
    invalidSawPad48_ = true;
  }
  const bool released = (invalidSawPad224_ && pad224 != 0) || (invalidSawPad48_ && pad48 != 0);
  const bool timedOut = core.mem_r32(kGameVblankCount) - invalidStartField_ >= 1800u;
  if (released || timedOut) {
    enterOuterCycle(core);
    return true;
  }
  return false;
}

void Spider1ModeDriver::step(Core &core, uint32_t) {
  while (true) {
    switch (state_) {
    case State::Dormant:
      lucent::error("frame", "Spider-Man 1 mode step ran before its finite boot prefix");
      std::abort();
    case State::AwaitOuterReady:
      if (core.mem_r32(kAsyncModeState) != 0) {
        host_.waitFields(core, 1);
        host_.commitRepeatedFieldFrame(core);
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
      if (core.mem_r32(kAsyncModeState) != 0) {
        host_.waitFields(core, 1);
        host_.commitRepeatedFieldFrame(core);
        return;
      }
      initializeMenu(core);
      continue;
    case State::MenuFrame:
      stepMenu(core);
      return;
    case State::AwaitMenuExit:
      if (core.mem_r32(kAsyncModeState) != 0) {
        host_.waitFields(core, 1);
        host_.commitRepeatedFieldFrame(core);
        return;
      }
      if (menuResult_) {
        core.r[4] = kModeArgument;
        core.r[5] = 1;
        core.r[6] = 0;
        call(core, kModePrepare, 0x8002C420u);
        initializePrimary(core);
      } else {
        enterOuterCycle(core);
      }
      continue;
    case State::AwaitLevelReady:
      if (core.mem_r32(kAsyncModeState) != 0) {
        host_.waitFields(core, 1);
        host_.commitRepeatedFieldFrame(core);
        return;
      }
      if (core.mem_r32(kLevelAlternateFlag) == 0) {
        initializeAlternate(core);
      } else {
        core.r[4] = kLevelName;
        call(core, kLevelObject, 0x8002C6C0u);
        if (core.r[2] == 0 || (core.mem_r32(core.r[2] + 8u) & 2u) != 0) {
          initializeAlternate(core);
        } else {
          core.r[4] = kModeArgument;
          core.r[5] = 0;
          core.r[6] = 1;
          call(core, kModePrepare, 0x8002C420u);
          initializePrimary(core);
        }
      }
      continue;
    case State::AlternateFrame:
      stepAlternate(core);
      return;
    case State::InvalidAwaitReady:
      if (core.mem_r32(kAsyncModeState) != 0) {
        host_.waitFields(core, 1);
        host_.commitRepeatedFieldFrame(core);
        return;
      }
      initializeInvalidInput(core);
      continue;
    case State::InvalidInput:
      if (pollInvalidInput(core)) {
        continue;
      }
      host_.waitFields(core, 1);
      host_.commitRepeatedFieldFrame(core);
      return;
    }
  }
}

} // namespace spider
