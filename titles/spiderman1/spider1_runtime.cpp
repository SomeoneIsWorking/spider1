#include "spider1_runtime.h"

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
extern void spiderman_install_sync_natives(Game *);

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
  // Spider-Man is the lineage title whose native display-list producer and pre-GTE temporal
  // contracts are being implemented. Preserve its native/GTE player choice and true temporal
  // interpolation product while those title-owned paths replace the explicit guest-frame debt.
  return RenderCapabilities::interpolatedNative();
}

bool Spider1Runtime::guestVramIsPicture(const Game &game) const {
  // Spider-Man 1 still runs the guest's drawing path, and its measured compatibility policy keeps
  // upload-only screens visible. Delegate to the bounded adapter while that title-specific policy
  // is migrated; do not move it into the lineage base or infer an answer for Enter Electro.
  return legacy_.guestVramIsPicture(game);
}

std::unique_ptr<TemporalFramePresentation>
Spider1Runtime::createTemporalFramePresentation(Game &game) {
  // Spider-Man's guest logic is not an already-60fps title. Preserve its established optional
  // interpolation decorator while the direct Enter Electro runtime remains neutral.
  return legacy_.createTemporalFramePresentation(game);
}

void Spider1Runtime::registerOverrides(Game &game) {
  // These framework compatibility services still consume Spider-Man 1's measured GameConfig.
  // Keep them behind this legacy title runtime so a direct title cannot dereference an absent
  // compatibility view before its own boot boundary.
  game.cd.overridesInit();
  game.platform_hle.initBuiltins();
  spiderman_install_sync_natives(&game);
  spiderman_install_render_seam(&game);
  spiderman_install_cd_stream(&game);
  spiderman_install_module_loader(&game);
  card_overrides_init(&game);
  spiderman_install_diag_overrides(&game);
  fntrace_init();
}

void Spider1Runtime::bootInit(Core &core) {
  const GuestProgramImage *image = guestProgramImage();
  if (!image || !image->gameMainEntry) {
    lucent::error("boot",
                  "the measured RE-01 gameMain entry is absent from Spider-Man's program image; "
                  "refusing to dispatch address 0");
    std::abort();
  }
  lucent::info("boot",
               "Phase 0: dispatching guest main() 0x{:08X} on the recompiled substrate",
               image->gameMainEntry);
  rec_dispatch(&core, image->gameMainEntry);
}

} // namespace spider
