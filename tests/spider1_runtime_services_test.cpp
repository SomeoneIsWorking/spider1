#include "spider1_platform_facts.h"
#include "spider1_runtime.h"

#include "cd_control.h"
#include "game.h"
#include "hw_bind.h"
#include "native_dispatch.h"
#include "native_execution.h"
#include "testutil.h"

#include <memory>

namespace {

inline constexpr uint32_t kCdParameter = 0x80112000u;
inline constexpr uint32_t kCdResult = 0x80113000u;
inline constexpr uint32_t kCdReturn = 0x80010100u;
inline constexpr uint32_t kInnerCdSync = 0x8008C944u;

struct CdCallbackProbe {
  static inline unsigned calls = 0;

  static void invoked(Core *) {
    ++calls;
  }
};

class NoStockWorkAreaRuntime final : public GameRuntime {
public:
  NoStockWorkAreaRuntime() : plan_(spider::spider1::platformServices) {
    plan_.stockCdWorkArea = {};
  }

  void *createContext(Core &) override {
    return nullptr;
  }
  void destroyContext(void *) override {}
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::direct();
  }
  bool guestVramIsPicture(const Game &) const override {
    return false;
  }
  const PlatformHlePlan *platformHlePlan() const override {
    return &plan_;
  }

private:
  PlatformHlePlan plan_;
};

void dispatch_cd(Core &core, uint8_t command, uint32_t parameter, uint32_t result) {
  core.r[4] = command;
  core.r[5] = parameter;
  core.r[6] = result;
  core.r[31] = kCdReturn;
  CHECK(psx::cpu::dispatchGuest(
            core, spider::spider1::cdCommandAddress, psx::cpu::ExecutionBudget::fromCycles(100))
            .returned());
}

void test_stock_cd_command_preserves_measured_guest_state_and_pending_result() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  runtime.prepareBootstrap(*game);
  CHECK(game->platform_hle.lookup(spider::spider1::cdCommandAddress) == cd_command_stock_sync);

  // Instrument the already installed guest callback addresses. Stock command completion currently
  // has no callback-dispatch owner; a later implementation must make this assertion change with
  // proved controller/event semantics, not by assuming these callbacks are harmless.
  CdCallbackProbe::calls = 0;
  spider::installNativeOverride(
      core, spider::spider1::cdReadyCallback, "test CD ready callback", CdCallbackProbe::invoked);
  spider::installNativeOverride(
      core, spider::spider1::cdSyncCallback, "test CD sync callback", CdCallbackProbe::invoked);
  core.r[31] = kCdReturn;
  CHECK(psx::cpu::dispatchGuest(
            core, spider::spider1::cdReadyCallback, psx::cpu::ExecutionBudget::fromCycles(100))
            .returned());
  CHECK_EQ(CdCallbackProbe::calls, 1u);
  CdCallbackProbe::calls = 0;
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, spider::spider1::cdReadyCallback);
  core.mem_w32(spider::spider1::cdSyncCallbackSlot, spider::spider1::cdSyncCallback);

  core.mem_w8(kCdParameter, 0x00u);
  core.mem_w8(kCdParameter + 1u, 0x02u);
  core.mem_w8(kCdParameter + 2u, 0x16u);
  core.mem_w8(kCdParameter + 3u, 0x01u);
  for (uint32_t index = 0; index < 5u; ++index) {
    core.mem_w8(spider::spider1::cdLastPositionAddress + index, 0xA5u);
  }
  dispatch_cd(core, 0x02u, kCdParameter, kCdResult);
  CHECK_EQ(core.r[2], 0u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress), 0x00u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress + 1u), 0x02u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress + 2u), 0x16u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress + 3u), 0x01u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastModeAddress), 0xA5u);

  core.mem_w8(kCdParameter, 0xE0u);
  dispatch_cd(core, 0x0Eu, kCdParameter, kCdResult);
  CHECK_EQ(core.r[2], 0u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastModeAddress), 0xE0u);
  CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress + 1u), 0x02u);

  game->disc.track_count = 2;
  game->disc.tracks[0] = DiscTrackInfo{1, 0, 45'000, 150, 0, 0};
  game->disc.tracks[1] = DiscTrackInfo{2, 45'000, 9'000, 0, 0, 0};
  dispatch_cd(core, 0x13u, 0u, kCdResult);
  CHECK_EQ(core.r[2], 0u);
  CHECK_EQ(core.mem_r8(kCdResult), 0x02u);
  CHECK_EQ(core.mem_r8(kCdResult + 1u), 0x01u);
  CHECK_EQ(core.mem_r8(kCdResult + 2u), 0x02u);
  core.mem_w8(kCdResult + 1u, 0xA5u);
  core.r[4] = 0u;
  core.r[5] = kCdResult;
  core.r[31] = kCdReturn;
  CHECK(psx::cpu::dispatchGuest(core, kInnerCdSync, psx::cpu::ExecutionBudget::fromCycles(100))
            .returned());
  CHECK_EQ(core.r[2], 2u);
  CHECK_EQ(core.mem_r8(kCdResult + 1u), 0x01u);
  CHECK_EQ(CdCallbackProbe::calls, 0u);
  CHECK_EQ(core.mem_r32(spider::spider1::cdReadyCallbackSlot), spider::spider1::cdReadyCallback);
  CHECK_EQ(core.mem_r32(spider::spider1::cdSyncCallbackSlot), spider::spider1::cdSyncCallback);
  CHECK(core.imageCatalog().deactivate(image));
}

void test_stock_cd_command_without_measured_work_area_does_not_write_guest_state() {
  NoStockWorkAreaRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate("Spider-Man resident", {0x00010000u, 0x000C65D4u}, 1u);
  game->platform_hle.initBuiltins();
  CHECK(game->platform_hle.lookup(spider::spider1::cdCommandAddress) == cd_command_stock_sync);
  core.mem_w8(kCdParameter, 0x00u);
  core.mem_w8(kCdParameter + 1u, 0x02u);
  core.mem_w8(kCdParameter + 2u, 0x16u);
  core.mem_w8(kCdParameter + 3u, 0x01u);
  for (uint32_t index = 0; index < 5u; ++index) {
    core.mem_w8(spider::spider1::cdLastPositionAddress + index, 0xA5u);
  }
  dispatch_cd(core, 0x02u, kCdParameter, 0u);
  CHECK_EQ(core.r[2], 0u);
  core.mem_w8(kCdParameter, 0xE0u);
  dispatch_cd(core, 0x0Eu, kCdParameter, 0u);
  CHECK_EQ(core.r[2], 0u);
  for (uint32_t index = 0; index < 5u; ++index) {
    CHECK_EQ(core.mem_r8(spider::spider1::cdLastPositionAddress + index), 0xA5u);
  }
  CHECK(core.imageCatalog().deactivate(image));
}

void test_measured_services_use_the_direct_runtime() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  CHECK(core.cfg == nullptr);
  const auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  CHECK(game->frameDriver != nullptr);
  runtime.prepareBootstrap(*game);
  CHECK(game->platform_hle.hasNativeFrameLoopContract());
  CHECK(game->platform_hle.lookup(0x80089ECCu) == cd_read_stock_sync);
  CHECK(game->platform_hle.lookup(0x8008A068u) == cd_readsync_stock_sync);
  CHECK(game->platform_hle.lookup(0x8008C944u) == cd_sync_stock_sync);
  CHECK(core.nativeDispatcher().isInstalled({image, 0x8008A16Cu}));
  CHECK(game->platform_hle.lookup(0x8002C354u) == nullptr);
  CHECK(!game->platform_hle.register_(0x8002C354u, cd_read_stock_sync));

  gte_bind(&core);
  core.r[4] = 231;
  core.r[5] = 117;
  core.r[31] = 0x80010100u;
  CHECK(psx::cpu::dispatchGuest(core, 0x8008BF24u, psx::cpu::ExecutionBudget::fromCycles(100))
            .returned());
  CHECK(!core.rsub.projParams.geomValid());
  core.r[4] = 287;
  CHECK(psx::cpu::dispatchGuest(core, 0x8008BF14u, psx::cpu::ExecutionBudget::fromCycles(100))
            .returned());
  CHECK(core.rsub.projParams.geomValid());
  CHECK(core.rsub.projParams.geomOfx() == 231.0f);
  CHECK(core.rsub.projParams.geomOfy() == 117.0f);
  CHECK(core.rsub.projParams.geomH() == 287.0f);

  const auto field =
      psx::cpu::dispatchGuest(core, 0x80084BE0u, psx::cpu::ExecutionBudget::fromCycles(100));
  CHECK(field.reason == psx::cpu::ExecutionExitReason::FrameBoundary);
  CHECK(!game->platform_hle.register_(0x80084BE0u, cd_read_stock_sync));

  core.mem_w32(0x800B397Cu, 7u);
  core.r[31] = 0x80010100u;
  const auto dmaArm =
      psx::cpu::dispatchGuest(core, 0x80083C60u, psx::cpu::ExecutionBudget::fromCycles(100));
  CHECK(dmaArm.returned());
  CHECK_EQ(core.r[2], 247u);
  CHECK_EQ(core.mem_r32(0x800B0F64u), 247u);
  CHECK_EQ(core.mem_r32(0x800B0F68u), 0u);
  CHECK_EQ(core.mem_r32(0x800B397Cu), 7u);

  core.mem_w32(spider::spider1::cdReadyCallbackSlot, 0u);
  core.mem_w32(spider::spider1::cdSyncCallbackSlot, 0u);
  core.mem_w32(spider::spider1::cdEventCallbackSlot, 0u);
  core.mem_w32(spider::spider1::cdEventUnusedSlot, 0xffffffffu);
  core.r[31] = 0x80010100u;
  const auto cdInit = psx::cpu::dispatchGuest(
      core, spider::spider1::cdInitAddress, psx::cpu::ExecutionBudget::fromCycles(100));
  CHECK(cdInit.returned());
  CHECK_EQ(core.r[2], 1u);
  CHECK_EQ(core.mem_r32(spider::spider1::cdReadyCallbackSlot), spider::spider1::cdReadyCallback);
  CHECK_EQ(core.mem_r32(spider::spider1::cdSyncCallbackSlot), spider::spider1::cdSyncCallback);
  CHECK_EQ(core.mem_r32(spider::spider1::cdEventCallbackSlot), spider::spider1::cdEventCallback);
  CHECK_EQ(core.mem_r32(spider::spider1::cdEventUnusedSlot), 0u);
  CHECK(core.imageCatalog().deactivate(image));
}

void test_pad_service_writes_retail_receive_buffers_only() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  // Retail PadInitDirect arguments and the per-frame mirror from the independent RE-05
  // consumer trace. A +2 or mirror-base regression must fail at the production pad seam.
  core.mem_w32(0x800A50ECu, 0xa5a5a5a5u);
  core.mem_w32(0x800A510Eu, 0xa5a5a5a5u);
  core.mem_w32(0x800A5130u, 0x12345678u);
  game->pad.driveHold(0xfff7u);
  game->pad.serviceFrame();
  CHECK_EQ(core.mem_r8(0x800A50ECu), 0x00u);
  CHECK_EQ(core.mem_r8(0x800A50EDu), 0x41u);
  CHECK_EQ(core.mem_r16(0x800A50EEu), 0xfff7u);
  CHECK_EQ(core.mem_r8(0x800A510Eu), 0xffu);
  CHECK_EQ(core.mem_r32(0x800A5130u), 0x12345678u);
}

} // namespace

int main() {
  RUN(stock_cd_command_preserves_measured_guest_state_and_pending_result);
  RUN(stock_cd_command_without_measured_work_area_does_not_write_guest_state);
  RUN(measured_services_use_the_direct_runtime);
  RUN(pad_service_writes_retail_receive_buffers_only);
  return pt_summary();
}
