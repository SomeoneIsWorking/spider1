#pragma once

#include "game_iface.h"

namespace spider {

// Process-lifetime owner of Spider-Man's framework-facing behavior. The legacy base is temporary:
// it keeps measured compatibility facts and not-yet-typed callbacks reachable without making the
// GameConfig/GameHooks pair the title's public architecture.
class SpiderRuntime final : public LegacyGameRuntimeAdapter {
public:
  SpiderRuntime();

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
};

} // namespace spider
