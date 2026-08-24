#pragma once

#include "game_iface.h"
#include "spider_runtime.h"

namespace spider {

class Spider1Runtime final : public SpiderRuntime {
public:
  Spider1Runtime();

  std::string_view discEnvironment() const override;
  std::string_view defaultExecutable() const override;
  const ExecutableIdentity &executableIdentity() const override;
  void installRecomp() override;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  const GuestProgramImage *guestProgramImage() const override;
  bool guestVramIsPicture(const Game &game) const override;
  std::unique_ptr<TemporalFramePresentation> createTemporalFramePresentation(Game &game) override;

private:
  static const ExecutableIdentity identity_;

  // Bounded composition keeps the old framework views alive for Spider-Man 1 without making them
  // the shared lineage base or leaking them into Enter Electro.
  LegacyGameRuntimeAdapter legacy_;
};

} // namespace spider
