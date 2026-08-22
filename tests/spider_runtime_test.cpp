#include "spider_runtime.h"

#include "core.h"
#include "game_iface.h"

#include <cstdio>
#include <memory>
#include <type_traits>

// SpiderRuntime's installer dependencies are not exercised by this ownership test. The real port's
// live boot gate exercises them; these definitions keep the test linked to the shipping runtime TU
// without duplicating any installer behavior.
void spiderman_install_cd_stream(Game *) {}
void spiderman_install_diag_overrides(Game *) {}
void spiderman_install_module_loader(Game *) {}
void spiderman_install_sync_natives(Game *) {}
void spiderman_install_render_seam(Game *) {}
int spiderman_str_skip_selftest(const char *, const char *) {
  return 0;
}

int main() {
  static_assert(std::is_base_of_v<GameRuntime, spider::SpiderRuntime>);
  static_assert(std::is_base_of_v<LegacyGameRuntimeAdapter, spider::SpiderRuntime>);

  spider::SpiderRuntime runtime;
  psxport_install_game(runtime);
  auto core = std::make_unique<Core>();
  const GameHooks *legacyHooks = runtime.legacyHooksForMigration();
  if (psxport_game_runtime() != &runtime || core->runtime != &runtime || core->gameCtx == nullptr ||
      runtime.legacyConfigForMigration() == nullptr || legacyHooks == nullptr) {
    std::fprintf(stderr, "SpiderRuntime did not own the installed compatibility seam\n");
    return 1;
  }
  if (legacyHooks->bootInit != nullptr || legacyHooks->registerOverrides != nullptr) {
    std::fprintf(stderr, "boot or override ownership remained in legacy GameHooks\n");
    return 1;
  }

  std::puts("SpiderRuntime: derived install, measured legacy facts, owned boot and overrides");
  return 0;
}
