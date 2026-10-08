#include "spider1_runtime.h"
#include "spider1_frame_driver.h"
#include "spider1_platform_facts.h"
#include "spider1_stream_driver.h"

#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spider::spider1 {

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

// `NativeFunction` carries no owner pointer, so overrides find the title owner through the Core;
// the checked downcast refuses a foreign Core.
Spider1Runtime &Spider1Runtime::from(Core &core) {
  auto *self = dynamic_cast<Spider1Runtime *>(core.runtime);
  if (self == nullptr) {
    lucent::error("cd", "Spider-Man 1 CD service has no matching title runtime");
    std::abort();
  }
  return *self;
}

void *Spider1Runtime::createContext(Core &core) {
  return new Spider1StreamDriver(core);
}

void Spider1Runtime::destroyContext(void *context) {
  // CD service counts are reported once at the end of the run, where the whole-run denominator
  // exposes a service that never ran.
  cdStream_.report();
  delete static_cast<Spider1StreamDriver *>(context);
}

void Spider1Runtime::registerOverrides(Game &game) {
  game.platform_hle.initBuiltins();
  Spider1FrameDriver::from(game.core).installBootstrapOverrides();
  Spider1StreamDriver::from(game.core).install();
  cdStream_.install(game.core);
  installSpider1Widescreen(game.core);
}

void Spider1Runtime::bootInit(Core &core) {
  Spider1FrameDriver::from(core).beginGuest(core);
}

std::unique_ptr<FrameDriver> Spider1Runtime::createFrameDriver(Game &game) {
  return std::make_unique<Spider1FrameDriver>(game);
}

bool Spider1Runtime::resumeBootstrapBoundary(Core &core, const psx::cpu::ExecutionResult &result) {
  if (result.reason == psx::cpu::ExecutionExitReason::CooperativeYield &&
      result.guestPc == stGetNextAddress && core.pc == result.guestPc) {
    Spider1FrameDriver::from(core).serviceBootstrapStreamWait(core);
    return true;
  }
  if (result.reason != psx::cpu::ExecutionExitReason::FrameBoundary || core.r[4] != 0) {
    return false;
  }
  // The resume address must be one of the authenticated `jal` return PCs: a leaf's entry would
  // re-request the boundary forever.
  if (core.r[31] != resetGraphVsyncReturn && !isMovieFieldReturn(core.r[31])) {
    return false;
  }
  if (result.guestPc != core.r[31] || core.pc != core.r[31]) {
    return false;
  }
  Spider1FrameDriver &driver = Spider1FrameDriver::from(core);
  if (core.r[31] == resetGraphVsyncReturn) {
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
  // The policy answers which aspect was selected; the per-Core owner publishes the guest
  // projection.
  return &widescreen_;
}

const PlatformHlePlan *Spider1Runtime::platformHlePlan() const {
  return &platformServices;
}

// The CD data-ready interrupt goes to the guest's own source-2 handler 0x8008DA24 only: libstr's
// ready callback 0x800860B4 calls stock CdReady 0x8008CBC4, which needs the response that handler
// stages via 0x8008C3E0, and the stand-in handler would pop it first.
const GuestCdStreamCallbackLayout *Spider1Runtime::guestCdStreamCallbackLayout() const {
  return nullptr;
}

const GuestPadBufferLayout *Spider1Runtime::guestPadBufferLayout() const {
  return &padBuffers;
}

RenderCapabilities Spider1Runtime::renderCapabilities() const {
  // No native producer is attached; keep the native renderer control hidden.
  return RenderCapabilities::widescreenOnly();
}

bool Spider1Runtime::guestVramIsPicture(const Game &) const {
  return true;
}

} // namespace spider::spider1
