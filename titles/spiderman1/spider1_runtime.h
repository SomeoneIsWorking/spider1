#pragma once

#include "gpu_vk.h"
#include "spider1_cd_stream.h"
#include "spider1_widescreen.h"
#include "spider_runtime.h"

class Core;

namespace spider::spider1 {

class Spider1Runtime final : public SpiderRuntime {
public:
  std::string_view discEnvironment() const override;
  std::string_view defaultExecutable() const override;
  const ExecutableIdentity &executableIdentity() const override;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  // Owner lookup for native overrides, which carry no owner pointer. Out of line because the
  // `dynamic_cast` needs the key function's typeinfo.
  static Spider1Runtime &from(Core &core);
  CdStreamService &cdStream() {
    return cdStream_;
  }
  void bootInit(Core &core) override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  void prepareBootstrap(Game &game) override;
  bool resumeBootstrapBoundary(Core &core, const psx::cpu::ExecutionResult &result) override;
  const GuestProgramImage *guestProgramImage() const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  const GuestCdStreamCallbackLayout *guestCdStreamCallbackLayout() const override;
  const GuestPadBufferLayout *guestPadBufferLayout() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;

private:
  static const ExecutableIdentity identity_;
  // Process-lifetime: aspect selection and the shared plan latch; per-frame state lives elsewhere.
  mutable Spider1Widescreen widescreen_{gpu_vk_latch_guest_projection};
  // Recovered CD-ROM service 0x8008C3E0; process-lifetime because it counts over the whole run.
  CdStreamService cdStream_;
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

} // namespace spider::spider1
