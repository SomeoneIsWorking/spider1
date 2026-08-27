#include "spider1_runtime.h"
#include "spider1_frame_driver.h"

#include "cfg.h"
#include "core.h"
#include "fntrace.h"
#include "game.h"
#include "legacy_game_interface.h"
#include "memcard.h"
#include "render_seam.h"
#include "spider_context.h"

#include <lucent/log.h>

#include <cstdlib>

extern void spiderman_install_cd_stream(Game *);
extern void spiderman_install_diag_overrides(Game *);
extern void spiderman_install_module_loader(Game *);
extern void spiderman_install_recomp();

namespace spider {

const ExecutableIdentity Spider1Runtime::identity_{
    .serial = SPIDER_TITLE_SERIAL,
    .fileSize = SPIDER_TITLE_EXECUTABLE_SIZE,
    .sha256 = SPIDER_TITLE_EXECUTABLE_SHA256,
};

Spider1Runtime::Spider1Runtime() : legacy_(legacy::measuredConfig, legacy::compatibilityHooks) {
  bindLegacyInterface(&legacy::measuredConfig, &legacy::compatibilityHooks);
}

std::string_view Spider1Runtime::discEnvironment() const {
  return SPIDER_TITLE_DISC_ENV;
}

std::string_view Spider1Runtime::defaultExecutable() const {
  return SPIDER_TITLE_GUEST_EXE;
}

const ExecutableIdentity &Spider1Runtime::executableIdentity() const {
  return identity_;
}

void Spider1Runtime::installRecomp() {
  spiderman_install_recomp();
}

void *Spider1Runtime::createContext(Core &) {
  return new SpiderContext;
}

void Spider1Runtime::destroyContext(void *context) {
  delete static_cast<SpiderContext *>(context);
}

const GuestProgramImage *Spider1Runtime::guestProgramImage() const {
  return legacy_.guestProgramImage();
}

RenderCapabilities Spider1Runtime::renderCapabilities() const {
  // Native rendering and temporal interpolation remain targets, not shipping capabilities: no
  // named scene has a complete native producer and no authored pose-history product exists yet.
  // Keep those controls hidden until S006/S009 can supply the pixels they claim to select.
  return RenderCapabilities::widescreenOnly();
}

bool Spider1Runtime::guestVramIsPicture(const Game &game) const {
  // Spider-Man 1 still runs the guest's drawing path, and its measured compatibility policy keeps
  // upload-only screens visible. Delegate to the bounded adapter while that title-specific policy
  // is migrated; do not move it into the lineage base or infer an answer for Enter Electro.
  return legacy_.guestVramIsPicture(game);
}

const GuestWidescreenProjection *Spider1Runtime::guestWidescreenProjection() const {
  return &widescreen_;
}

void Spider1Runtime::registerOverrides(Game &game) {
  // These framework compatibility services still consume Spider-Man 1's measured GameConfig.
  // Keep them behind this legacy title runtime so a direct title cannot dereference an absent
  // compatibility view before its own boot boundary.
  game.cd.overridesInit();
  game.platform_hle.initBuiltins();
  auto *driver = dynamic_cast<Spider1FrameDriver *>(game.frameDriver.get());
  if (!driver) {
    lucent::error("frame", "Spider-Man 1 override registration has no title FrameDriver");
    std::abort();
  }
  driver->installOverrides();
  widescreen_.install();
  spiderman_install_render_seam(&game);
  spiderman_install_cd_stream(&game);
  spiderman_install_module_loader(&game);
  card_overrides_init(&game);
  spiderman_install_diag_overrides(&game);
  fntrace_init();
}

void Spider1Runtime::bootInit(Core &core) {
  auto *driver =
      core.game ? dynamic_cast<Spider1FrameDriver *>(core.game->frameDriver.get()) : nullptr;
  if (!driver) {
    lucent::error("boot", "Spider-Man 1 finite boot has no title FrameDriver");
    std::abort();
  }
  driver->runBootPrefix(core);
}

std::unique_ptr<FrameDriver> Spider1Runtime::createFrameDriver(Game &game) {
  return std::make_unique<Spider1FrameDriver>(game);
}

} // namespace spider
