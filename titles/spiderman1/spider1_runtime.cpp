#include "spider1_runtime.h"
#include "spider1_platform_facts.h"

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

void *Spider1Runtime::createContext(Core &) {
  return nullptr;
}

void Spider1Runtime::destroyContext(void *) {}

void Spider1Runtime::registerOverrides(Game &game) {
  game.platform_hle.initBuiltins();
}

void Spider1Runtime::bootInit(Core &) {
  refuseUnported("native frame owner", "runtime Lightrec execution before native frame extraction");
}

const GuestProgramImage *Spider1Runtime::guestProgramImage() const {
  return &image_;
}

const PlatformHlePlan *Spider1Runtime::platformHlePlan() const {
  return &spider1::platformServices;
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
