#include "spider1_runtime.h"
#include "spider1_frame_driver.h"
#include "spider1_platform_facts.h"
#include "spider1_stream_driver.h"

#include "frame_loop_shell.h"

#include "game.h"

namespace spider {

const ExecutableIdentity Spider1Runtime::identity_{
    .serial = SPIDER_TITLE_SERIAL,
    .fileSize = SPIDER_TITLE_EXECUTABLE_SIZE,
    .sha256 = SPIDER_TITLE_EXECUTABLE_SHA256,
};

std::string_view Spider1Runtime::discEnvironment() const {
  return SPIDER_TITLE_DISC_ENV;
}

std::string_view Spider1Runtime::defaultExecutable() const {
  return SPIDER_TITLE_GUEST_EXE;
}

const ExecutableIdentity &Spider1Runtime::executableIdentity() const {
  return identity_;
}

void *Spider1Runtime::createContext(Core &core) {
  return new spider1::Spider1StreamDriver(core);
}

void Spider1Runtime::destroyContext(void *context) {
  delete static_cast<spider1::Spider1StreamDriver *>(context);
}

void Spider1Runtime::registerOverrides(Game &game) {
  game.platform_hle.initBuiltins();
  Spider1FrameDriver::from(game.core).installBootstrapOverrides();
  spider1::Spider1StreamDriver::from(game.core).install();
  installSpider1Widescreen(game.core);
}

void Spider1Runtime::bootInit(Core &) {
  refuseUnported("native frame owner", "runtime Lightrec execution before native frame extraction");
}

std::unique_ptr<FrameDriver> Spider1Runtime::createFrameDriver(Game &game) {
  return std::make_unique<Spider1FrameDriver>(game);
}

void Spider1Runtime::prepareBootstrap(Game &game) {
  FrameLoopShell{}.prepareProduct(game);
}

bool Spider1Runtime::resumeBootstrapBoundary(Core &core, const psx::cpu::ExecutionResult &result) {
  if (result.reason == psx::cpu::ExecutionExitReason::CooperativeYield &&
      result.guestPc == spider1::stGetNextAddress && core.pc == result.guestPc) {
    Spider1FrameDriver::from(core).serviceBootstrapStreamWait(core);
    return true;
  }
  if (result.reason != psx::cpu::ExecutionExitReason::FrameBoundary || core.r[4] != 0) {
    return false;
  }
  // WHICH BOUNDARY THIS IS, decided by the guest's own `jal` return address, and the resume address
  // must be exactly that return address.
  //
  // This used to require `result.guestPc == platformServices.vsyncAddress`, i.e. that the guest be
  // parked AT the VSync leaf. That is the wrong identity and it was a spin: a `jal`ed leaf's entry
  // is never a valid resume point, so a boundary stamped with it resumes inside the leaf that asked
  // for it and requests another one forever. A `jal` leaves the address the guest continues at in
  // r[31], and the guest's own `jal` to the VSync leaf sets r[31] to one of exactly the three
  // authenticated return PCs — so requiring the resume to BE that return address is both correct
  // and a STRONGER check than the one it replaces: it pins the caller and the continuation
  // together, where the old condition pinned only the callee and let any caller through.
  if (core.r[31] != spider1::resetGraphVsyncReturn && !spider1::isMovieFieldReturn(core.r[31])) {
    return false;
  }
  if (result.guestPc != core.r[31] || core.pc != core.r[31]) {
    return false;
  }
  Spider1FrameDriver &driver = Spider1FrameDriver::from(core);
  if (core.r[31] == spider1::resetGraphVsyncReturn) {
    driver.serviceBootstrapVsync(core);
  } else {
    driver.serviceBootstrapMovieVsync(core);
  }
  return true;
}

const GuestProgramImage *Spider1Runtime::guestProgramImage() const {
  return &image_;
}

const GuestWidescreenProjection *Spider1Runtime::guestWidescreenProjection() const {
  // The policy answers which aspect the player selected; the per-Core owner below is what publishes
  // a guest projection (external/psxport/docs/presentation-contract.md, "Title-owned guest
  // widescreen"). Declaration alone cannot widen a frame, and returning the policy without the
  // overrides in place would be the half that advertises a capability the picture does not have.
  return &widescreen_;
}

const PlatformHlePlan *Spider1Runtime::platformHlePlan() const {
  return &spider1::platformServices;
}

const GuestCdStreamCallbackLayout *Spider1Runtime::guestCdStreamCallbackLayout() const {
  return &spider1::cdStreamCallbacks;
}

const GuestPadBufferLayout *Spider1Runtime::guestPadBufferLayout() const {
  return &spider1::padBuffers;
}

RenderCapabilities Spider1Runtime::renderCapabilities() const {
  // No native producer is attached during break-first bring-up. Keep the native renderer control
  // hidden until the preserved title owners are reattached through image-aware runtime dispatch.
  return RenderCapabilities::widescreenOnly();
}

bool Spider1Runtime::guestVramIsPicture(const Game &) const {
  return true;
}

} // namespace spider
