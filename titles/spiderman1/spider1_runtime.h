#pragma once

#include "gpu_vk.h"
#include "spider1_cd_stream.h"
#include "spider1_widescreen.h"
#include "spider_runtime.h"

class Core;

namespace spider {

class Spider1Runtime final : public SpiderRuntime {
public:
  std::string_view discEnvironment() const override;
  std::string_view defaultExecutable() const override;
  const ExecutableIdentity &executableIdentity() const override;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  // The one accessor that reaches this runtime's state from a native override, which cannot carry
  // an owner pointer. `Spider1StreamDriver::from` does the same for the stream pump's per-Core
  // context, so this is the established route and not a second registry.
  //
  // DEFINED OUT OF LINE, and an earlier revision of this header declared it `inline` with a comment
  // claiming that "an inline definition is emitted weakly into every translation unit, so tests
  // need no extra link edge". That is false and it broke the build: `dynamic_cast<Spider1Runtime
  // *>` on a polymorphic class needs Spider1Runtime's TYPEINFO, and the compiler emits
  // typeinfo/vtable in the translation unit that defines the class's key function. So an inline
  // body in a header still emits an undefined reference to the vtable, and
  // `spider1_guest_boundary_owners_test` -- which compiles the CD service against a bare `Core` and
  // does not link this file -- failed at link time with "undefined reference to vtable for
  // spider::Spider1Runtime". Out of line is also the honest shape: the owner lookup from a Core is
  // a fact about THIS runtime, and it has exactly one home.
  static Spider1Runtime &from(Core &core);
  spider1::CdStreamService &cdStream() {
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
  // Process-lifetime, and honestly so: it answers the player's aspect selection and it holds the
  // shared plan latch. Everything that changes per frame lives in the framework's own per-Game
  // latch and in the guest record, which is where the guest's own projection state already was.
  mutable Spider1Widescreen widescreen_{gpu_vk_latch_guest_projection};
  // The recovered CD-ROM service 0x8008C3E0. Process-lifetime for the same reason the widescreen
  // latch above is: it counts what the guest's own poll loop did over the whole run, and a
  // per-frame owner would be the wrong lifetime for a counter whose denominator is the whole run.
  spider1::CdStreamService cdStream_;
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
