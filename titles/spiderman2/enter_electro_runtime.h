#pragma once

#include "spider_runtime.h"

namespace spider::enterelectro {

class EnterElectroRuntime final : public SpiderRuntime {
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

  // From SLUS_013.78.
  const GuestProgramImage image_ = {
      .bss = {0x800C2AF4u, 0x800CF0DCu},
      .stackTopWordAddress = 0x800C0D10u,
      .stackReserveWordAddress = 0x800C0D0Cu,
      .heapBase = 0x800CF0DCu,
      .heapSizeStoreAddress = 0x800BED00u,
      .heapBaseStoreAddress = 0x800BECFCu,
      .globalPointer = 0x800C1764u,
      .libcInitEntry = 0x800988B0u,
      .gameMainEntry = 0x80031F54u,
      .crt0Entry = 0x80093C68u,
      // Pre-BSS resident span (BSS starts at 0x800C2AF4), not the PS-X EXE payload extent.
      .residentText = {0x00010000u, 0x000C2AF4u},
      .backtraceText = {},
      .stackBias = {true, -8},
  };
};

} // namespace spider::enterelectro
