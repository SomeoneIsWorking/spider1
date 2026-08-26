#include "spider1_runtime.h"

#include "game.h"
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
void spiderman_install_recomp() {}
void spiderman_install_sync_natives(Game *) {}
void spiderman_install_render_seam(Game *) {}
int spiderman_str_skip_selftest(const char *, const char *) {
  return 0;
}

int main() {
  static_assert(std::is_base_of_v<GameRuntime, spider::SpiderRuntime>);
  static_assert(std::is_base_of_v<spider::SpiderRuntime, spider::Spider1Runtime>);
  static_assert(!std::is_base_of_v<LegacyGameRuntimeAdapter, spider::Spider1Runtime>);

  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  const GameHooks *legacyHooks = runtime.legacyHooksForMigration();
  const RenderCapabilities renderCapabilities = runtime.renderCapabilities();
  if (psxport_game_runtime() != &runtime || core.runtime != &runtime || core.gameCtx == nullptr ||
      runtime.legacyConfigForMigration() == nullptr || legacyHooks == nullptr ||
      !renderCapabilities.nativeRenderPath || !renderCapabilities.temporalInterpolation ||
      renderCapabilities.defaultPath != RenderPath::Native ||
      renderCapabilities.playerPathCount() != 2 ||
      !renderCapabilities.playerSelectable(RenderPath::Native) ||
      !game_guest_vram_is_picture(*game) || runtime.executableIdentity().fileSize != 749568u ||
      runtime.executableIdentity().sha256 !=
          "d2270e35581ba083d9441166e9a45ead4f869ab07e890f9a512ad7ee4cc0b15b") {
    std::fprintf(stderr, "SpiderRuntime did not own the installed compatibility seam\n");
    return 1;
  }
  if (legacyHooks->bootInit != nullptr || legacyHooks->registerOverrides != nullptr) {
    std::fprintf(stderr, "boot or override ownership remained in legacy GameHooks\n");
    return 1;
  }

  std::puts(
      "Spider1Runtime: measured legacy facts, native+temporal rendering, owned boot and overrides");
  return 0;
}
