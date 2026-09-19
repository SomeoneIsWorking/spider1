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
  if (result.reason != psx::cpu::ExecutionExitReason::FrameBoundary ||
      result.guestPc != spider1::platformServices.vsyncAddress || core.pc != result.guestPc ||
      core.r[4] != 0) {
    return false;
  }
  Spider1FrameDriver &driver = Spider1FrameDriver::from(core);
  if (core.r[31] == spider1::resetGraphVsyncReturn) {
    driver.serviceBootstrapVsync(core);
  } else if (spider1::isMovieFieldReturn(core.r[31])) {
    driver.serviceBootstrapMovieVsync(core);
  } else {
    return false;
  }
  return true;
}

const GuestProgramImage *Spider1Runtime::guestProgramImage() const {
  return &image_;
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
