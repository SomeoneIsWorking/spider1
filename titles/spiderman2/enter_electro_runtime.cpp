#include "enter_electro_runtime.h"

#include <lucent/log.h>

namespace spider::enterelectro {

const ExecutableIdentity EnterElectroRuntime::identity_{
    .serial = SPIDER_TITLE_SERIAL,
    .fileSize = SPIDER_TITLE_EXECUTABLE_SIZE,
    .sha256 = SPIDER_TITLE_EXECUTABLE_SHA256,
};

std::string_view EnterElectroRuntime::discEnvironment() const {
  return SPIDER_TITLE_DISC_ENV;
}

std::string_view EnterElectroRuntime::defaultExecutable() const {
  return SPIDER_TITLE_GUEST_EXE;
}

const ExecutableIdentity &EnterElectroRuntime::executableIdentity() const {
  return identity_;
}

void *EnterElectroRuntime::createContext(Core &) {
  return nullptr;
}

void EnterElectroRuntime::destroyContext(void *) {}

void EnterElectroRuntime::registerOverrides(Game &) {
  lucent::info("boot",
               "title serial {} installs no title overrides: only its executable/crt0 boundary is "
               "implemented",
               serial());
}

void EnterElectroRuntime::bootInit(Core &) {
  refuseUnported("gameMain 0x80031F54", "EE-02 (first game-owned call)");
}

const GuestProgramImage *EnterElectroRuntime::guestProgramImage() const {
  return &image_;
}

RenderCapabilities EnterElectroRuntime::renderCapabilities() const {
  // No native producer or temporal product; widescreen only.
  return RenderCapabilities::widescreenOnly();
}

bool EnterElectroRuntime::guestVramIsPicture(const Game &) const {
  refuseUnported("guest-VRAM picture ownership",
                 "EE-02, then measure Enter Electro's render ownership");
}

} // namespace spider::enterelectro
