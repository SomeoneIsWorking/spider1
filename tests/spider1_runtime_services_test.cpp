#include "spider1_runtime.h"

#include "cd_control.h"
#include "game.h"
#include "hw_bind.h"
#include "native_dispatch.h"
#include "testutil.h"

#include <memory>

namespace {

void test_measured_services_use_the_direct_runtime() {
  spider::Spider1Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  CHECK(core.cfg == nullptr);
  runtime.registerOverrides(*game);
  CHECK(game->platform_hle.hasNativeFrameLoopContract());
  CHECK(game->platform_hle.lookup(0x80089ECCu) == cd_read_stock_sync);
  CHECK(game->platform_hle.lookup(0x8008A068u) == cd_readsync_stock_sync);
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
  RUN(measured_services_use_the_direct_runtime);
  RUN(pad_service_writes_retail_receive_buffers_only);
  return pt_summary();
}
