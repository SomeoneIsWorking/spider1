#include "spider1_platform_facts.h"
#include "spider1_runtime.h"
#include "spider1_stream_driver.h"

#include "cd_control.h"
#include "cdc_state.h"
#include "execution_control.h"
#include "game.h"
#include "hw_bind.h"
#include "native_dispatch.h"
#include "native_execution.h"
#include "testutil.h"

#include <array>
#include <memory>

namespace {

inline constexpr uint32_t kCdParameter = 0x80112000u;
inline constexpr uint32_t kCdResult = 0x80113000u;
inline constexpr uint32_t kCdReturn = 0x80010100u;
inline constexpr uint32_t kInnerCdSync = 0x8008C944u;
inline constexpr uint32_t kVsyncCallbackEntry = 0x8008B8CCu;
inline constexpr uint32_t kSyntheticFieldCallback = 0x80015000u;
inline constexpr uint32_t kStreamCallback = 0x800860B4u;
inline constexpr uint32_t kVblankCount = spider::spider1::libetcVblankCountAddress;
inline constexpr uint32_t kVsyncQueryResult = 0x800B2000u;
inline constexpr uint32_t kSyntheticCdIsr = 0x80014000u;
inline constexpr uint32_t kSyntheticCdIrqElement = 0x80012300u;
inline constexpr uint32_t kSyntheticCdResponse = 0x800B2004u;
inline constexpr uint32_t kCdIStat = 0x1F801070u;
inline constexpr uint32_t kCdIMask = 0x1F801074u;

// A synthetic guest ISR performs the stock libcd ordering through the shipping MMIO/Lightrec path:
// read the current response, acknowledge controller INT1 and I_STAT, then invoke the installed
// ready callback. The fixture is not a claim that these are Spider's exact ISR instructions.
inline constexpr std::array<uint32_t, 24> kGuestCdIsrWords{
    0x27BDFFF0u, 0xAFBF000Cu, 0x3C081F80u, 0x35081800u, 0xA1000000u, 0x91090001u,
    0x3C0A800Bu, 0xA1492004u, 0x240B0001u, 0xA10B0000u, 0xA10B0003u, 0x3C0C1F80u,
    0x358C1070u, 0x240D07FBu, 0xAD8D0000u, 0x8D4E3B18u, 0x24040001u, 0x01202821u,
    0x01C0F809u, 0x00000000u, 0x8FBF000Cu, 0x27BD0010u, 0x03E00008u, 0x00000000u,
};

struct FieldCallbackProbe {
  static inline unsigned calls = 0;

  static void invoked(Core *) {
    ++calls;
  }
};

struct CdCallbackProbe {
  static inline unsigned calls = 0;
  static inline uint32_t irqTypeAtCall = 0;
  static inline uint32_t responseAtCall = 0;
  static inline uint32_t a0AtCall = 0;
  static inline uint32_t a1AtCall = 0;

  static void invoked(Core *core) {
    ++calls;
    irqTypeAtCall = cdc_current_irq_type(&core->game->cdc);
    responseAtCall = core->mem_r8(kSyntheticCdResponse);
    a0AtCall = core->r[4];
    a1AtCall = core->r[5];
  }
};

struct CdCallbackExitProbe {
  static inline unsigned calls = 0;

  static void invoked(Core *core) {
    ++calls;
    psx::cpu::requestExecutionExit(*core,
                                   {psx::cpu::ExecutionExitReason::HostService,
                                    core->pc,
                                    0,
                                    "synthetic CD-ready callback requested an exit"});
  }
};

void publishDataReady(Game &game) {
  game.cdc.q_head = 0;
  game.cdc.q_tail = 1;
  game.cdc.q[0].type = 1;
  game.cdc.q[0].len = 1;
  game.cdc.q[0].resp[0] = game.cdc.stat;
}

void installGuestCdIsr(Game &game) {
  Core &core = game.core;
  for (uint32_t index = 0; index < kGuestCdIsrWords.size(); ++index) {
    core.mem_w32(kSyntheticCdIsr + index * 4u, kGuestCdIsrWords[index]);
  }
  core.mem_w32(kSyntheticCdIrqElement + 4u, kSyntheticCdIsr);
  core.mem_w32(kSyntheticCdIrqElement + 8u, 0u);
  game.hle.irqEnq(2u, kSyntheticCdIrqElement);
  core.mem_w32(kCdIMask, 1u << 2u);
}

void publishGuestCdInt1(Game &game, uint8_t response) {
  game.cdc.stat = response;
  publishDataReady(game);
  ++game.cdc.irq_sequence;
  game.cdc.irq_edge = 1u;
  CHECK_EQ(game.core.mem_r32(kCdIStat) & (1u << 2u), 1u << 2u);
}

void writeNestedVsyncQueryCallback(Core &core) {
  // Synthetic guest ready callback with the authenticated CdReady call shape: VSync(-1)
  // executes after the guest ISR consumes INT1 and stores the query result. This crosses the
  // shipping HLE and Lightrec paths without taking a movie field.
  core.mem_w32(kStreamCallback, 0x27BDFFF0u);      // addiu sp, sp, -16
  core.mem_w32(kStreamCallback + 4u, 0xAFBF000Cu); // sw ra, 12(sp)
  core.mem_w32(kStreamCallback + 8u,
               0x0C000000u |
                   ((spider::spider1::platformServices.vsyncAddress >> 2u) & 0x03FFFFFFu));
  core.mem_w32(kStreamCallback + 12u, 0x2404FFFFu); // addiu a0, zero, -1 (delay slot)
  core.mem_w32(kStreamCallback + 16u, 0x3C08800Bu); // lui t0, 0x800b
  core.mem_w32(kStreamCallback + 20u, 0xAD022000u); // sw v0, 0x2000(t0)
  core.mem_w32(kStreamCallback + 24u, 0x8FBF000Cu); // lw ra, 12(sp)
  core.mem_w32(kStreamCallback + 28u, 0x27BD0010u); // addiu sp, sp, 16
  core.mem_w32(kStreamCallback + 32u, 0x03E00008u); // jr ra
  core.mem_w32(kStreamCallback + 36u, 0u);
}

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

void test_retail_movie_field_exit_resumes_at_each_authenticated_return() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  runtime.prepareBootstrap(*game);
  CHECK(game->platform_hle.lookup(kVsyncCallbackEntry) != nullptr);
  spider::installNativeOverride(
      core, kSyntheticFieldCallback, "test display field callback", FieldCallbackProbe::invoked);
  FieldCallbackProbe::calls = 0;
  core.r[4] = kSyntheticFieldCallback;
  core.r[31] = kCdReturn;
  CHECK(
      psx::cpu::dispatchGuest(core, kVsyncCallbackEntry, psx::cpu::ExecutionBudget::fromCycles(100))
          .returned());

  auto unexpected = psx::cpu::ExecutionResult{psx::cpu::ExecutionExitReason::FrameBoundary,
                                              spider::spider1::platformServices.vsyncAddress,
                                              0,
                                              {}};
  core.pc = unexpected.guestPc;
  core.r[4] = 0;
  core.r[31] = 0x80010140u;
  auto oldFence = game->presentation.fence();
  auto oldVblank = core.mem_r32(kVblankCount);
  CHECK(!runtime.resumeBootstrapBoundary(core, unexpected));
  CHECK_EQ(game->presentation.fence(), oldFence);
  CHECK_EQ(core.mem_r32(kVblankCount), oldVblank);
  CHECK_EQ(core.pc, unexpected.guestPc);

  std::array<uint32_t, 3> returns{spider::spider1::movieInitialVsyncReturn,
                                  spider::spider1::movieFrameVsyncReturn,
                                  spider::spider1::movieTeardownVsyncReturn};
  core.r[16] = 0;
  for (uint32_t index = 0; index < returns.size(); ++index) {
    uint32_t returnPc = returns[index];
    uint32_t callPc = returnPc - 8u;
    core.mem_w32(callPc,
                 0x0C000000u |
                     ((spider::spider1::platformServices.vsyncAddress >> 2u) & 0x03FFFFFFu));
    core.mem_w32(callPc + 4u, 0x24040000u);   // addiu a0, zero, 0: VSync(0) delay slot
    core.mem_w32(returnPc, 0x26100001u);      // addiu s0, s0, 1: guest continuation
    core.mem_w32(returnPc + 4u, 0x0000000Du); // break: stop after one resumed instruction
    core.r[31] = 0;
    auto exit =
        psx::cpu::dispatchGuestUntilExit(core, callPc, psx::cpu::ExecutionBudget::fromCycles(100));
    CHECK_EQ(exit.reason, psx::cpu::ExecutionExitReason::FrameBoundary);
    CHECK_EQ(exit.guestPc, spider::spider1::platformServices.vsyncAddress);
    CHECK_EQ(core.r[31], returnPc);
    CHECK_EQ(core.r[16], index);
    CHECK(runtime.resumeBootstrapBoundary(core, exit));
    CHECK_EQ(core.pc, returnPc);
    CHECK_EQ(game->presentation.fence(), oldFence + index + 1u);
    CHECK_EQ(core.mem_r32(kVblankCount), oldVblank + index + 1u);
    CHECK_EQ(FieldCallbackProbe::calls, index + 1u);
    auto continuation =
        psx::cpu::dispatchGuestUntilExit(core, core.pc, psx::cpu::ExecutionBudget::fromCycles(100));
    CHECK_EQ(continuation.reason, psx::cpu::ExecutionExitReason::HostService);
    CHECK_EQ(core.r[16], index + 1u);
  }
  CHECK(core.imageCatalog().deactivate(image));
}

void test_direct_stream_wait_preserves_original_result_and_field_ownership() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  runtime.prepareBootstrap(*game);
  CHECK(core.nativeDispatcher().isInstalled({image, spider::spider1::stGetNextAddress}));
  CHECK_EQ(cd_ready_callback_pointer(core), spider::spider1::cdReadyCallbackSlot);
  CHECK_EQ(runtime.guestCdStreamCallbackLayout()->readyCallbackPointer,
           spider::spider1::cdReadyCallbackSlot);
  CHECK_EQ(runtime.guestCdStreamCallbackLayout()->owner,
           GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt);

  spider::installNativeOverride(
      core, kSyntheticFieldCallback, "test display field callback", FieldCallbackProbe::invoked);
  FieldCallbackProbe::calls = 0;
  core.r[4] = kSyntheticFieldCallback;
  core.r[31] = kCdReturn;
  CHECK(
      psx::cpu::dispatchGuest(core, kVsyncCallbackEntry, psx::cpu::ExecutionBudget::fromCycles(100))
          .returned());

  uint32_t entry = spider::spider1::stGetNextAddress;
  core.mem_w32(entry, 0x26310001u);      // addiu s1, s1, 1: count original-body executions
  core.mem_w32(entry + 4u, 0x24020001u); // addiu v0, zero, 1: no sector ready
  core.mem_w32(entry + 8u, 0x03E00008u); // jr ra
  core.mem_w32(entry + 12u, 0u);
  spider::installNativeOverride(
      core, kStreamCallback, "test stream callback", CdCallbackProbe::invoked);
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, kStreamCallback);
  game->cd.stream_active = 1;
  publishDataReady(*game);
  CdCallbackProbe::calls = 0;
  core.r[17] = 0;
  core.r[31] = kCdReturn;
  const auto dry = psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(200));
  CHECK_EQ(dry.reason, psx::cpu::ExecutionExitReason::CooperativeYield);
  CHECK_EQ(dry.guestPc, entry);
  CHECK_EQ(core.pc, entry);
  CHECK_EQ(core.r[2], 1u);
  CHECK_EQ(core.r[17], 2u);
  CHECK_EQ(game->cd.stream_delivered, 0u);
  CHECK_EQ(CdCallbackProbe::calls, 0u); // host pump must not bypass the guest ISR
  CHECK_EQ(cdc_current_irq_type(&game->cdc), 1u);
  const auto fence = game->presentation.fence();
  const auto vblank = core.mem_r32(kVblankCount);
  core.mem_w32(0x800B0FA8u, 0xA5A5A5A5u); // VSync horizontal baseline
  core.mem_w32(0x800B0FACu, 0x5A5A5A5Au); // VSync last-field record

  const psx::cpu::ExecutionResult wrongPc{
      psx::cpu::ExecutionExitReason::CooperativeYield, entry + 4u, 0, {}};
  CHECK(!runtime.resumeBootstrapBoundary(core, wrongPc));
  CHECK_EQ(core.pc, entry);
  CHECK_EQ(game->presentation.fence(), fence);
  CHECK_EQ(core.mem_r32(kVblankCount), vblank);

  CHECK(runtime.resumeBootstrapBoundary(core, dry));
  CHECK_EQ(core.pc, kCdReturn);
  CHECK_EQ(core.r[31], kCdReturn);
  CHECK_EQ(core.r[2], 1u);
  CHECK_EQ(core.r[17], 2u);
  CHECK_EQ(FieldCallbackProbe::calls, 1u);
  CHECK_EQ(game->presentation.fence(), fence + 1u);
  CHECK_EQ(core.mem_r32(kVblankCount), vblank + 1u);
  CHECK_EQ(core.mem_r32(0x800B0FA8u), 0xA5A5A5A5u);
  CHECK_EQ(core.mem_r32(0x800B0FACu), 0x5A5A5A5Au);

  CHECK(core.imageCatalog().deactivate(image));
}

void test_stream_int1_reaches_ready_callback_only_after_guest_isr_consumes_response() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  installGuestCdIsr(*game);
  spider::installNativeOverride(
      core, kStreamCallback, "test stream callback", CdCallbackProbe::invoked);
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, kStreamCallback);
  core.mem_w8(kSyntheticCdResponse, 0xFFu);
  core.r[4] = 0xAAAAAAAAu;
  core.r[5] = 0xBBBBBBBBu;
  core.r[29] = 0x801FFFC0u;
  core.r[31] = kCdReturn;
  game->cd.stream_active = 1;
  CdCallbackProbe::calls = 0;
  publishGuestCdInt1(*game, 0x22u);

  game->cd.pumpStream(&core, 1);
  CHECK_EQ(CdCallbackProbe::calls, 0u); // reached negative: host pump cannot invoke ready first
  CHECK_EQ(cdc_current_irq_type(&game->cdc), 1u);
  CHECK_EQ(core.mem_r8(kSyntheticCdResponse), 0xFFu);
  CHECK_EQ(game->cd.stream_delivered, 0u);

  game->hle.irqPoll(&core);
  CHECK_EQ(CdCallbackProbe::calls, 1u);
  CHECK_EQ(CdCallbackProbe::irqTypeAtCall, 0u); // guest ISR acknowledged INT1 first
  CHECK_EQ(CdCallbackProbe::responseAtCall, 0x22u);
  CHECK_EQ(CdCallbackProbe::a0AtCall, 1u);
  CHECK_EQ(CdCallbackProbe::a1AtCall, 0x22u);
  CHECK_EQ(cdc_current_irq_type(&game->cdc), 0u);
  CHECK_EQ(core.mem_r32(kCdIStat) & (1u << 2u), 0u);
  CHECK_EQ(core.r[4], 0xAAAAAAAAu);
  CHECK_EQ(core.r[5], 0xBBBBBBBBu);
  CHECK_EQ(game->cd.stream_delivered, 0u);
  CHECK(core.imageCatalog().deactivate(image));
}

void test_ready_stream_slot_returns_guest_result_without_host_field() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  uint32_t entry = spider::spider1::stGetNextAddress;
  core.mem_w32(entry, 0x26310001u);      // addiu s1, s1, 1: count original-body executions
  core.mem_w32(entry + 4u, 0x24020000u); // addiu v0, zero, 0: ready
  core.mem_w32(entry + 8u, 0x03E00008u); // jr ra
  core.mem_w32(entry + 12u, 0u);
  spider::installNativeOverride(
      core, kStreamCallback, "test stream callback", CdCallbackProbe::invoked);
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, kStreamCallback);
  game->cd.stream_active = 1;
  publishDataReady(*game);
  CdCallbackProbe::calls = 0;
  core.r[17] = 0;
  core.r[31] = kCdReturn;
  const auto ready =
      psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(200));
  CHECK(ready.returned());
  CHECK_EQ(core.pc, kCdReturn);
  CHECK_EQ(core.r[2], 0u);
  CHECK_EQ(core.r[17], 1u);
  CHECK_EQ(CdCallbackProbe::calls, 0u);
  CHECK_EQ(game->cd.stream_delivered, 0u);
  CHECK_EQ(game->presentation.fence(), 0u);
  CHECK(core.imageCatalog().deactivate(image));
}

void test_ring_diagnostic_samples_first_change_and_owns_each_core() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto first = std::make_unique<Game>();
  auto second = std::make_unique<Game>();
  Core &core = first->core;
  Core &other = second->core;
  auto &stream = spider::spider1::Spider1StreamDriver::from(core);
  auto &otherStream = spider::spider1::Spider1StreamDriver::from(other);
  CHECK(&stream != &otherStream);

  uint32_t base = 0x80148000u;
  for (Core *instance : {&core, &other}) {
    instance->mem_w32(spider::spider1::ringBaseAddress, base);
    instance->mem_w32(spider::spider1::ringSlotCountAddress, 48u);
  }
  CHECK(stream.sampleRingIfChanged().has_value());  // the first poll is visible
  CHECK(!stream.sampleRingIfChanged().has_value()); // unchanged hot poll is silent
  core.mem_w32(spider::spider1::ringWriteIndexAddress, 1u);
  CHECK(stream.sampleRingIfChanged().has_value()); // producer index moves
  CHECK(!stream.sampleRingIfChanged().has_value());
  core.mem_w16(base + spider::spider1::ringSlotStride, 2u);
  CHECK(stream.sampleRingIfChanged().has_value()); // selected slot becomes ready
  core.mem_w16(base, 4u);
  CHECK(stream.sampleRingIfChanged().has_value()); // consumer slot changes without an index move
  first->cdc.irq_sequence = 1u;
  first->cdc.q[0].type = 1u;
  first->cdc.q_tail = 1;
  CHECK(stream.sampleRingIfChanged().has_value()); // first controller INT1 is visible immediately
  first->cdc.q_head = 1;
  CHECK(stream.sampleRingIfChanged().has_value()); // guest acknowledgment is visible immediately
  first->cdc.data_rd = 4;
  CHECK(stream.sampleRingIfChanged().has_value()); // data FIFO consumption is visible immediately
  CHECK(!stream.sampleRingIfChanged().has_value());
  CHECK(otherStream.sampleRingIfChanged().has_value()); // another Core has its own first sample
  CHECK(!otherStream.sampleRingIfChanged().has_value());
  CHECK(!stream.sampleRingIfChanged().has_value());
}

void test_guest_isr_callback_exit_does_not_retry_original_stream_poll() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  uint32_t entry = spider::spider1::stGetNextAddress;
  core.mem_w32(entry, 0x26310001u);      // addiu s1, s1, 1: count original-body executions
  core.mem_w32(entry + 4u, 0x24020001u); // addiu v0, zero, 1: dry
  core.mem_w32(entry + 8u, 0x03E00008u); // jr ra
  core.mem_w32(entry + 12u, 0u);
  spider::installNativeOverride(
      core, kStreamCallback, "test exiting stream callback", CdCallbackExitProbe::invoked);
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, kStreamCallback);
  installGuestCdIsr(*game);
  game->cd.stream_active = 1;
  CdCallbackExitProbe::calls = 0;
  core.r[17] = 0;
  core.r[29] = 0x801FFFC0u;
  core.r[31] = kCdReturn;

  const auto dry = psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(200));
  CHECK_EQ(dry.reason, psx::cpu::ExecutionExitReason::CooperativeYield);
  CHECK_EQ(core.r[17], 2u); // exactly one dry retry before any guest ISR
  CHECK_EQ(CdCallbackExitProbe::calls, 0u);
  publishGuestCdInt1(*game, 0x22u);
  game->cd.pumpStream(&core, 1);
  CHECK_EQ(CdCallbackExitProbe::calls, 0u);
  game->hle.irqPoll(&core);
  CHECK(core.executionControl().pending());
  CHECK_EQ(core.r[17], 2u); // callback's typed exit cannot re-enter the original body
  CHECK_EQ(CdCallbackExitProbe::calls, 1u);
  CHECK_EQ(game->cd.stream_delivered, 0u);
  CHECK_EQ(game->presentation.fence(), 0u);
  CHECK(core.imageCatalog().deactivate(image));
}

void test_nested_cd_ready_vsync_query_returns_without_frame_or_register_loss() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  auto image = core.imageCatalog().activate(
      "Spider-Man resident", runtime.guestProgramImage()->residentText, 1u);
  runtime.registerOverrides(*game);
  runtime.prepareBootstrap(*game);
  uint32_t entry = spider::spider1::stGetNextAddress;
  core.mem_w32(entry, 0x26310001u);      // addiu s1, s1, 1: original-body calls
  core.mem_w32(entry + 4u, 0x24020001u); // addiu v0, zero, 1: still dry
  core.mem_w32(entry + 8u, 0x03E00008u); // jr ra
  core.mem_w32(entry + 12u, 0u);
  writeNestedVsyncQueryCallback(core);
  core.mem_w32(spider::spider1::cdReadyCallbackSlot, kStreamCallback);
  installGuestCdIsr(*game);
  core.mem_w32(kVblankCount, 37u);
  core.mem_w32(kVsyncQueryResult, 0xDEADBEEFu);
  game->cd.stream_active = 1;
  core.r[4] = 0x807FFEF0u; // outer StGetNext's address output pointer
  core.r[5] = 0x807FFEF4u;
  core.r[29] =
      0x801FFFC0u; // guest callback saves RA on the same valid stack it would have in retail
  core.r[17] = 0;
  core.r[31] = kCdReturn;
  const auto dry = psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(300));
  CHECK_EQ(dry.reason, psx::cpu::ExecutionExitReason::CooperativeYield);
  CHECK_EQ(dry.guestPc, entry);
  CHECK_EQ(core.r[2], 1u);
  CHECK_EQ(core.r[4], 0x807FFEF0u);
  CHECK_EQ(core.r[5], 0x807FFEF4u);
  CHECK_EQ(core.r[31], kCdReturn);
  CHECK_EQ(core.r[17], 2u); // original StGetNext returned dry twice
  CHECK_EQ(core.mem_r32(kVsyncQueryResult), 0xDEADBEEFu);
  publishGuestCdInt1(*game, 0x22u);
  game->cd.pumpStream(&core, 1);
  CHECK_EQ(core.mem_r32(kVsyncQueryResult), 0xDEADBEEFu); // host did not call ready
  game->hle.irqPoll(&core);
  CHECK_EQ(core.mem_r32(kVsyncQueryResult), 37u);
  CHECK_EQ(core.mem_r32(kVblankCount), 37u); // VSync(-1) did not deliver a field
  CHECK_EQ(game->presentation.fence(), 0u);
  CHECK_EQ(game->cd.stream_delivered, 0u);

  CHECK(runtime.resumeBootstrapBoundary(core, dry));
  CHECK_EQ(core.pc, kCdReturn);
  CHECK_EQ(core.r[2], 1u);
  CHECK_EQ(core.mem_r32(kVblankCount), 38u); // only the dry-poll host field advances it
  CHECK_EQ(game->presentation.fence(), 1u);
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
  RUN(retail_movie_field_exit_resumes_at_each_authenticated_return);
  RUN(direct_stream_wait_preserves_original_result_and_field_ownership);
  RUN(stream_int1_reaches_ready_callback_only_after_guest_isr_consumes_response);
  RUN(ready_stream_slot_returns_guest_result_without_host_field);
  RUN(ring_diagnostic_samples_first_change_and_owns_each_core);
  RUN(guest_isr_callback_exit_does_not_retry_original_stream_poll);
  RUN(nested_cd_ready_vsync_query_returns_without_frame_or_register_loss);
  RUN(stock_cd_command_preserves_measured_guest_state_and_pending_result);
  RUN(stock_cd_command_without_measured_work_area_does_not_write_guest_state);
  RUN(measured_services_use_the_direct_runtime);
  RUN(pad_service_writes_retail_receive_buffers_only);
  return pt_summary();
}
