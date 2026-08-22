#include "spider_runtime.h"

#include "cfg.h"
#include "core.h"
#include "fntrace.h"
#include "game.h"
#include "legacy_game_interface.h"
#include "memcard.h"
#include "render_seam.h"
#include "spider_context.h"

#include <cstdlib>

extern void spiderman_install_cd_stream(Game *);      // game/core/cd_stream.cpp
extern void spiderman_install_diag_overrides(Game *); // game/core/diag_overrides.cpp
extern void spiderman_install_module_loader(Game *);  // game/core/module_loader.cpp
extern void spiderman_install_sync_natives(Game *);   // game/core/sync_native.cpp

namespace spider {

SpiderRuntime::SpiderRuntime()
    : LegacyGameRuntimeAdapter(legacy::measuredConfig, legacy::compatibilityHooks) {}

void *SpiderRuntime::createContext(Core &) {
  return new SpiderContext;
}

void SpiderRuntime::destroyContext(void *context) {
  delete static_cast<SpiderContext *>(context);
}

void SpiderRuntime::registerOverrides(Game &game) {
  // Generic HLE is installed by main first, then this title's measured native handlers.
  spiderman_install_sync_natives(&game);

  // This installs the engine submitFrame override and states the honest Gte default.
  // native_boot_run reads the policy after this method returns, so explicit config layers still
  // outrank the default.
  spiderman_install_render_seam(&game);

  spiderman_install_cd_stream(&game);
  spiderman_install_module_loader(&game);
  card_overrides_init(&game);
  spiderman_install_diag_overrides(&game);

  // PSXPORT_FNTRACE claims guest override slots, so it must remain last. A later real override
  // could silently displace the tracer and turn a reached function into a false negative.
  fntrace_init();
}

void SpiderRuntime::bootInit(Core &core) {
  const GameConfig *config = legacyConfigForMigration();
  if (!config || !config->gameMain) {
    cfg_loge("boot",
             "the measured RE-01 gameMain entry is absent from Spider-Man's legacy program facts; "
             "refusing to dispatch address 0");
    std::abort();
  }
  cfg_logi("boot",
           "Phase 0: dispatching guest main() 0x%08X on the recompiled substrate",
           config->gameMain);
  rec_dispatch(&core, config->gameMain);
}

} // namespace spider
