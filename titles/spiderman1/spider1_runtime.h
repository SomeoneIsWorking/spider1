#pragma once

#include "spider_runtime.h"

namespace spider {

class Spider1Runtime final : public SpiderRuntime {
public:
  std::string_view discEnvironment() const override;
  std::string_view defaultExecutable() const override;
  const ExecutableIdentity &executableIdentity() const override;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  const GuestProgramImage *guestProgramImage() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;

private:
  static const ExecutableIdentity identity_;
  const GuestProgramImage image_ = {
      .bss = {0x800B5994u, 0x800C65D4u},
      .stackTopWordAddress = 0x800B3E70u,
      .stackReserveWordAddress = 0x800B3E6Cu,
      .heapBase = 0x800C65D4u,
      .heapSizeStoreAddress = 0x800B1240u,
      .heapBaseStoreAddress = 0x800B123Cu,
      .globalPointer = 0x800B47F4u,
      .libcInitEntry = 0x8008DC98u,
      .gameMainEntry = 0x8002C354u,
      .crt0Entry = 0x8008739Cu,
      .residentText = {0x00010000u, 0x000C65D4u},
      .backtraceText = {},
      .stackBias = {false, 0},
  };
};

} // namespace spider
