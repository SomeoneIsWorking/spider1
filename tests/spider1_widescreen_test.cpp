// Falsifiers for SLUS_008.75's guest-widescreen owner.
// 0x80061140 publishes the 512x240 draw RECT; 0x80075E18/0x80075E1C read the horizontal window;
// 0x80075E64 stores OFX = (far+near)>>1.

#include "spider1_widescreen.h"

#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "gpu_vk.h"
#include "image_identity.h"
#include "mods.h"
#include "native_dispatch.h"
#include "testutil.h"

#include <array>
#include <cstdint>
#include <memory>

namespace {

using spider::spider1::Spider1ViewportOffset;
using spider::spider1::Spider1Widescreen;

inline constexpr std::uint32_t kRecordAddress = 0x800B0000u;
// Retail 512x240 window read at 0x80075D90-0x80075E74: near/far 0..512, cull first 0, lens 2365,
// H 276, OFX 256, OFY 120 (H and OFX cross-checked against RE-17).
inline constexpr std::uint16_t kRetailNear = 0;
inline constexpr std::uint16_t kRetailFar = 512;
inline constexpr std::uint16_t kRetailDepthLower = 0x0100;
inline constexpr std::uint16_t kRetailDepthUpper = 0x0200;
inline constexpr std::uint16_t kRetailLens = 2365;
inline constexpr std::uint16_t kRetailH = 276;
inline constexpr std::uint16_t kRetailCentreX = 256;
inline constexpr std::uint16_t kRetailCentreY = 120;
inline constexpr std::uint16_t kRetailVerticalNear = 0;
inline constexpr std::uint16_t kRetailVerticalFar = 240;
inline constexpr std::uint16_t kRetailDrawWidth = 512;
inline constexpr std::uint16_t kRetailDrawHeight = 240;
inline constexpr std::uint32_t kDrawEnvironmentAddress = 0x800B1000u;

// Minimum GameRuntime the framework latch consults; hands it the shipping policy.
class TestRuntime final : public GameRuntime {
public:
  explicit TestRuntime(const GuestWidescreenProjection *policy) : policy_(policy) {}
  void setPolicy(const GuestWidescreenProjection *policy) {
    policy_ = policy;
  }
  const GuestWidescreenProjection *guestWidescreenProjection() const override {
    return policy_;
  }
  // Lifecycle hooks are empty; the fixture drives the owner directly.
  void *createContext(Core &) override {
    return nullptr;
  }
  void destroyContext(void *) override {}
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::widescreenOnly();
  }
  bool guestVramIsPicture(const Game &) const override {
    return true;
  }

private:
  const GuestWidescreenProjection *policy_ = nullptr;
};

struct Fixture {
  std::unique_ptr<Game> game;
  std::unique_ptr<Spider1Widescreen> owner;
  TestRuntime runtime{nullptr};
  Core *core = nullptr;

  Fixture() {
    // The latch needs the Core render path (RenderPath::Gte, this title's path) and the title
    // policy; the runtime is installed before the Game because Game's constructor captures it.
    owner = std::make_unique<Spider1Widescreen>(gpu_vk_latch_guest_projection);
    runtime.setPolicy(owner.get());
    psxport_install_game(runtime);
    game = std::make_unique<Game>();
    core = &game->core;
    core->rsub.mode.setPath(RenderPath::Gte);
    // The latch takes the presentation extent from the GP1 display width; 0x80061140 gives a 512
    // RECT.
    core->game->gpu.s_disp_w = kRetailDrawWidth;
    core->game->gpu.s_disp_h = kRetailDrawHeight;
    writeWord(Spider1ViewportOffset::kHorizontalFar, kRetailFar);
    writeWord(Spider1ViewportOffset::kVerticalNear, kRetailVerticalNear);
    writeWord(Spider1ViewportOffset::kHorizontalNear, kRetailNear);
    writeWord(Spider1ViewportOffset::kVerticalFar, kRetailVerticalFar);
    writeWord(Spider1ViewportOffset::kDepthLower, kRetailDepthLower);
    writeWord(Spider1ViewportOffset::kDepthUpper, kRetailDepthUpper);
    writeWord(Spider1ViewportOffset::kLensDivisor, kRetailLens);
    writeWord(Spider1ViewportOffset::kScreenDistance, kRetailH);
    writeWord(Spider1ViewportOffset::kCentreX, kRetailCentreX);
    writeWord(Spider1ViewportOffset::kCentreY, kRetailCentreY);
    core->mem_w32(spider::spider1::kViewportRecordCell, kRecordAddress);
    // The guest draw environment as 0x800884C0 wrote it.
    core->mem_w16(kDrawEnvironmentAddress + 0, 0);
    core->mem_w16(kDrawEnvironmentAddress + 2, 256);
    core->mem_w16(kDrawEnvironmentAddress + 4, kRetailDrawWidth);
    core->mem_w16(kDrawEnvironmentAddress + 6, kRetailDrawHeight);
    core->mem_w16(kDrawEnvironmentAddress + 8, 0x1234);
  }

  void writeWord(int offset, std::uint16_t value) const {
    core->mem_w16(kRecordAddress + offset, value);
  }
  std::uint16_t readWord(int offset) const {
    return core->mem_r16(kRecordAddress + offset);
  }
  std::uint16_t drawWidth() const {
    return core->mem_r16(kDrawEnvironmentAddress + 4);
  }

  // The guest body recovered from 0x80075D0C, recomputed so the test can tell the owner wrote the
  // widened centre from the title's own derivation producing it. Two-stage fixed-point division:
  //   0x80075E24 subu 0x80075E28 sra 1 0x80075E2C sll 12 0x80075E30 div $v1(lens) 0x80075E34 mflo
  //   0x80075E3C sll 12                  0x80075E40 div $a1(gp+0x1140)     0x80075E44 mflo
  //   0x80075E74 sh -> record+14 (H)
  //   0x80075E54 addu 0x80075E60 sra 1    0x80075E64 sh -> record+16 (OFX)
  //   0x80075E68 addu 0x80075E6C sra 1    0x80075E70 sh -> record+18 (OFY)
  // 256<<12 / 2365 << 12 / 6574 is 276 exactly, which fixes the scale factor.
  static constexpr std::int32_t kRetailScale = 6574;

  static void retailProjection(Core &core) {
    const std::uint32_t record = core.mem_r32(spider::spider1::kViewportRecordCell);
    const std::int32_t far = core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar);
    const std::int32_t near = core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear);
    const std::int32_t lens = core.mem_r16(record + Spider1ViewportOffset::kLensDivisor);
    const std::int32_t top = core.mem_r16(record + Spider1ViewportOffset::kVerticalNear);
    const std::int32_t bottom = core.mem_r16(record + Spider1ViewportOffset::kVerticalFar);
    const std::int32_t half = (far - near) >> 1;
    const std::int32_t scaled = ((half << 12) / lens) << 12;
    core.mem_w16(record + Spider1ViewportOffset::kScreenDistance,
                 static_cast<std::uint16_t>(scaled / kRetailScale));
    core.mem_w16(record + Spider1ViewportOffset::kCentreX,
                 static_cast<std::uint16_t>((far + near) >> 1));
    core.mem_w16(record + Spider1ViewportOffset::kCentreY,
                 static_cast<std::uint16_t>((bottom + top) >> 1));
    // 0x80075DBC recomputes record[10] from gp+0x1104; the tail rewrites record[8], [10], [12] from
    // the view struct. The owner must not treat either as persistent input, so the fixture
    // reproduces both.
    core.mem_w16(record + Spider1ViewportOffset::kDepthUpper, kRetailDepthUpper);
  }

  static void retailDrawEnvironment(Core &core) {
    // 0x800884C0: RECT.x/$a1, RECT.y/$a2, RECT.w/$a3, RECT.h/stack, then u/v and flags.
    const std::uint32_t environment = core.r[4];
    core.mem_w16(environment + 0, static_cast<std::uint16_t>(core.r[5]));
    core.mem_w16(environment + 2, static_cast<std::uint16_t>(core.r[6]));
    core.mem_w16(environment + 4, static_cast<std::uint16_t>(core.r[7]));
    core.mem_w16(environment + 6, static_cast<std::uint16_t>(core.mem_r32(0x00100000u)));
  }

  // 0x8008736C stores RECT.w from $a3; RECT.h is a fifth stack argument. The body writes the
  // record.
  void publishDraw(Spider1Widescreen &owner) const {
    core->r[4] = kDrawEnvironmentAddress;
    core->r[5] = 0;
    core->r[6] = 256;
    core->r[7] = kRetailDrawWidth;
    core->mem_w32(0x00100000u, kRetailDrawHeight);
    owner.publishDrawEnvironment(*core, retailDrawEnvironment);
  }

  void publishProjection(Spider1Widescreen &owner) const {
    owner.publishProjection(*core, retailProjection);
  }
};

// 4:3 identity: snapshot the record and draw environment, publish at Standard4x3, compare every
// field.
void test_four_three_is_the_identity() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_4_3;

  std::array<std::uint16_t, Spider1ViewportOffset::kFieldCount> before{};
  for (int i = 0; i < Spider1ViewportOffset::kFieldCount; ++i) {
    before[static_cast<std::size_t>(i)] = fixture.readWord(i * 2);
  }
  const std::array<std::uint16_t, 5> environmentBefore{
      fixture.core->mem_r16(kDrawEnvironmentAddress + 0),
      fixture.core->mem_r16(kDrawEnvironmentAddress + 2),
      fixture.core->mem_r16(kDrawEnvironmentAddress + 4),
      fixture.core->mem_r16(kDrawEnvironmentAddress + 6),
      fixture.core->mem_r16(kDrawEnvironmentAddress + 8),
  };

  CHECK_EQ(owner.presentationAspect(*fixture.core), PresentationAspect::Standard4x3);
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);

  for (int i = 0; i < Spider1ViewportOffset::kFieldCount; ++i) {
    CHECK_EQ(fixture.readWord(i * 2), before[static_cast<std::size_t>(i)]);
  }
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 0), environmentBefore[0]);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 2), environmentBefore[1]);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 4), environmentBefore[2]);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 6), environmentBefore[3]);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 8), environmentBefore[4]);
  CHECK_EQ(owner.measuredDrawWidth(), kRetailDrawWidth);
  CHECK(!owner.published());
  CHECK(!owner.plan().widescreen());
  CHECK_EQ(owner.plan().projectionExtent.width, kRetailFar - kRetailNear);
  CHECK_EQ(owner.plan().guestDrawWidth, kRetailDrawWidth);
  CHECK_EQ(owner.plan().projectionCenterX, kRetailCentreX);
}

// The 16:9 plan, pinned against the framework's own pure builder.
void test_the_plan_comes_from_the_frameworks_own_rule() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;

  const GuestProjectionInputs inputs{
      .path = RenderPath::Gte,
      .requested = PresentationAspect::Wide16x9,
      .nativePresentation = {kRetailFar - kRetailNear, kRetailDrawHeight},
      .nativeProjection = {{kRetailFar - kRetailNear, kRetailDrawHeight}, kRetailDrawWidth},
      .sink = {1280, 720},
      .vramWidth = 0,
  };
  const GuestProjectionPlan expected = guest_projection_plan(inputs);

  fixture.publishDraw(owner);
  fixture.publishProjection(owner);

  CHECK_EQ(owner.plan().aspect, PresentationAspect::Wide16x9);
  CHECK_EQ(owner.plan().projectionExtent.width, expected.projectionExtent.width);
  CHECK_EQ(owner.plan().guestDrawWidth, expected.guestDrawWidth);
  CHECK_EQ(owner.plan().projectionCenterX, expected.projectionCenterX);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, expected.projectionHorizontalMargin);
  // 512 -> the smallest even width that does not undershoot 16:9.
  CHECK_EQ(expected.projectionExtent.width, 684);
  CHECK_EQ(expected.projectionCenterX, 342);
  CHECK_EQ(expected.projectionHorizontalMargin, 86);
  // The presentation extent comes from the GP1 display width, not the measured projection.
  CHECK_EQ(owner.plan().presentationExtent.width, 684);
  CHECK_EQ(owner.plan().nativeProjectionExtent.width, 512);
  CHECK_EQ(owner.plan().nativeGuestDrawWidth, kRetailDrawWidth);
  CHECK_EQ(owner.plan().guestClipRight, 683);
}

// Widening moves OFX outward and leaves H alone; a zoom leaves OFX at 256 and a lens rescale moves
// H.
void test_widening_moves_the_centre_and_leaves_the_focal_length_alone() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);

  CHECK(owner.plan().widescreen());
  CHECK(owner.published());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), owner.plan().projectionCenterX);
  CHECK(fixture.readWord(Spider1ViewportOffset::kCentreX) > kRetailCentreX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kScreenDistance), kRetailH);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kLensDivisor), kRetailLens);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreY), kRetailCentreY);

  // The window moved by exactly the plan's margin.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar),
           owner.retailWindow().horizontalFar + owner.plan().projectionHorizontalMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           owner.retailWindow().horizontalNear + owner.plan().projectionHorizontalMargin);
  // (far - near) is unchanged, which is why H is unchanged.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) -
               fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           kRetailFar - kRetailNear);
  // OFX/H grew by the canvas ratio (centre/window-width, H cancels): field of view, not central
  // scale.
  CHECK_EQ(static_cast<int>(fixture.readWord(Spider1ViewportOffset::kCentreX)) *
               (kRetailFar - kRetailNear),
           static_cast<int>(kRetailCentreX) * owner.plan().projectionExtent.width);
}

// Issue 0022: the publication runs every frame, so three publications must match the first.
void test_repeated_publication_is_idempotent() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);

  fixture.publishProjection(owner);
  std::array<std::uint16_t, Spider1ViewportOffset::kFieldCount> first{};
  for (int i = 0; i < Spider1ViewportOffset::kFieldCount; ++i) {
    first[static_cast<std::size_t>(i)] = fixture.readWord(i * 2);
  }
  for (int repeat = 0; repeat < 3; ++repeat) {
    fixture.publishProjection(owner);
  }
  for (int i = 0; i < Spider1ViewportOffset::kFieldCount; ++i) {
    CHECK_EQ(fixture.readWord(i * 2), first[static_cast<std::size_t>(i)]);
  }
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar),
           kRetailFar + owner.plan().projectionHorizontalMargin);
}

// An area or view change re-authors the window; the owner widens that window without accumulating.
void test_a_guest_rewritten_window_is_widened_from_the_new_retail_value() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);
  const int margin = owner.plan().projectionHorizontalMargin;

  // A 640-wide window centred on 320 re-derives the plan: 640 widens to 854, not 640+86, and the
  // shorter projection distance at lens 2365 is the title's to derive.
  fixture.writeWord(Spider1ViewportOffset::kHorizontalFar, 640);
  fixture.writeWord(Spider1ViewportOffset::kHorizontalNear, 0);
  fixture.writeWord(Spider1ViewportOffset::kDepthLower, kRetailDepthLower);
  const std::uint16_t distanceForThisViewBefore =
      fixture.readWord(Spider1ViewportOffset::kScreenDistance);
  fixture.publishProjection(owner);

  const int newMargin = owner.plan().projectionHorizontalMargin;
  CHECK(newMargin != margin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), 640u + newMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear), 0u + newMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), 320u + newMargin);
  CHECK_EQ(owner.retailWindow().horizontalFar, 640u);
  CHECK_EQ(owner.retailWindow().horizontalNear, 0u);
  // The window span is still what the guest authored.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) -
               fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           640u);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kLensDivisor), kRetailLens);
  // The title's derivation ran on the new window; no stale distance was restored.
  CHECK(fixture.readWord(Spider1ViewportOffset::kScreenDistance) != distanceForThisViewBefore);
}

// The horizontal cull window translates with the picture and the depth window does not move.
// It is in pre-shift guest coordinates (record[0], record[4]; 0x8007C2AC, 0x8007B9CC), so the
// band is [retail + margin, retail + width + margin), not [0, wide).
void test_the_horizontal_window_translates_and_the_depth_window_does_not() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);
  const int margin = owner.plan().projectionHorizontalMargin;

  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear), kRetailNear + margin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar + margin);
  // The window moved; it did not stretch to the frame.
  CHECK(fixture.readWord(Spider1ViewportOffset::kHorizontalNear) != 0u);
  CHECK(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) !=
        owner.plan().projectionExtent.width);
  // A guest x of 0 is now at +margin and stays inside the window.
  const std::uint32_t shiftedLeftEdge = static_cast<std::uint32_t>(margin);
  CHECK(shiftedLeftEdge >= fixture.readWord(Spider1ViewportOffset::kHorizontalNear));
  CHECK(shiftedLeftEdge < fixture.readWord(Spider1ViewportOffset::kHorizontalFar));

  // The depth window is a different axis: 0x8007C2AC and 0x8007B9CC compare record[8]/record[10]
  // against GTE IR1/SZ (outcode bits 4 and 5), and the publication rewrites both words itself.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kDepthLower), kRetailDepthLower);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kDepthUpper), kRetailDepthUpper);
  // The vertical window is likewise a different axis.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kVerticalNear), kRetailVerticalNear);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kVerticalFar), kRetailVerticalFar);
}

// Only RECT.w is the owner's; x, y, h and the u/v pair are the guest's.
void test_the_draw_clip_replaces_the_width_only() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);

  CHECK_EQ(fixture.drawWidth(), owner.plan().guestDrawWidth);
  CHECK(fixture.drawWidth() > kRetailDrawWidth);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 0), 0u);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 2), 256u);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 6), kRetailDrawHeight);
  CHECK_EQ(fixture.core->mem_r16(kDrawEnvironmentAddress + 8), 0x1234u);
  CHECK_EQ(fixture.drawWidth(), owner.plan().guestDrawWidth - 1 + 1);
}

// A republished plan must not be doubled, and an aspect change must reach the guest; the frame
// boundary calls this with no retail body, so it re-enters no guest code.
void test_synchronization_republishes_only_when_the_margin_moved() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);
  const int wideMargin = owner.plan().projectionHorizontalMargin;

  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, wideMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), owner.plan().projectionCenterX);
  // An unchanged aspect does not republish.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), 512u + wideMargin);

  // Re-latching only the plan would leave 512+86 beside a 4:3 plan.
  fixture.game->mods.aspect = ASPECT_4_3;
  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, 0);
  CHECK(!owner.plan().widescreen());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear), kRetailNear);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kScreenDistance), kRetailH);

  // And back out again.
  fixture.game->mods.aspect = ASPECT_16_9;
  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, wideMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), owner.plan().projectionCenterX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), 512u + wideMargin);
}

// The plan latched while the GP1 display width was still 320 described a 428-wide canvas for a
// 512-wide scene and the presenter refused the widening. A frame boundary must turn the stored plan
// into the 684-wide canvas once the display extent reaches 512, leaving the guest record untouched.
void test_the_frame_boundary_repairs_a_plan_latched_at_a_stale_display_extent() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  // The draw-environment site on a real boot.
  fixture.core->game->gpu.s_disp_w = 320;
  fixture.core->game->gpu.s_disp_h = 240;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  CHECK_EQ(owner.plan().presentationExtent.width, 428);
  // The guest 3D scene display mode, after graphical init.
  fixture.core->game->gpu.s_disp_w = kRetailDrawWidth;
  fixture.core->game->gpu.s_disp_h = kRetailDrawHeight;

  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().presentationExtent.width, 684);
  CHECK(owner.plan().widescreen());
  // The number the presenter reads is the one the announce line compares.
  CHECK(owner.plan().presentationExtent.width > owner.plan().nativeExtent.width);
  // The guest projection must not have moved.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kScreenDistance), kRetailH);
}

// A frame boundary before the guest published anything writes nothing.
void test_the_frame_boundary_is_silent_before_the_guest_publishes() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;

  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, 0);
  CHECK(!owner.plan().widescreen());
  CHECK(!owner.published());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);

  // The draw environment alone is not enough; the projection publication fixes the window.
  fixture.publishDraw(owner);
  owner.synchronizePresentation(*fixture.core);
  CHECK(!owner.published());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
}

// ASPECT_AUTO is not folded to 16:9: it resolves against the live sink, so a headless run gets 4:3.
void test_the_policy_maps_only_the_players_selection() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;

  fixture.game->mods.aspect = ASPECT_4_3;
  CHECK_EQ(owner.presentationAspect(*fixture.core), PresentationAspect::Standard4x3);
  fixture.game->mods.aspect = ASPECT_16_9;
  CHECK_EQ(owner.presentationAspect(*fixture.core), PresentationAspect::Wide16x9);
  fixture.game->mods.aspect = ASPECT_21_9;
  CHECK_EQ(owner.presentationAspect(*fixture.core), PresentationAspect::UltraWide21x9);
  fixture.game->mods.aspect = ASPECT_AUTO;
  CHECK_EQ(owner.presentationAspect(*fixture.core), PresentationAspect::MatchSink);
}

// With no draw environment published there is no measured draw width and the owner must not invent
// one; asserted through the exposed state because the refusal aborts.
void test_no_draw_width_means_no_measured_projection() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  CHECK(!owner.drawWidthMeasured());
  CHECK_EQ(owner.measuredDrawWidth(), 0u);
  CHECK(!owner.published());
  // The plan is derivable, with the window's own draw width; production refuses to latch it.
  const GuestProjectionGeometry geometry =
      Spider1Widescreen::measuredGeometry(kRetailFar - kRetailNear, kRetailDrawHeight, kRetailFar);
  CHECK_EQ(geometry.extent.width, 512);
  CHECK_EQ(geometry.drawWidth, 512);
}

// The pure geometry rule on the retail tuple; its refusal aborts, so it is exercised elsewhere.
void test_measured_geometry_carries_the_measured_span() {
  const GuestProjectionGeometry good = Spider1Widescreen::measuredGeometry(512, 240, 512);
  CHECK_EQ(good.extent.width, 512);
  CHECK_EQ(good.extent.height, 240);
  CHECK_EQ(good.drawWidth, 512);
  CHECK(good.valid());
}

// The guest-address bound against the guest's own boot addresses; 0x8009A6E4 was once refused.
void test_the_guest_address_bound_covers_the_titles_own_resident_data() {
  // 0x80061150/0x8006117C/0x80061194/0x800611AC publish the four environments; bss ends at
  // 0x800B5994.
  for (std::uint32_t address : {0x8009A6E4u,
                                0x8009A740u,
                                0x8009A75Cu,
                                0x8009A7B8u,
                                0x800B5918u,
                                0x800B47F4u,
                                0x801FFFFFu}) {
    CHECK(Spider1Widescreen::isGuestRam(address));
  }
  // The 2 MiB of parallel RAM at KSEG1 0xA0000000.
  CHECK(Spider1Widescreen::isGuestRam(0xA0000000u));
  CHECK(Spider1Widescreen::isGuestRam(0xA00FFFFFu));
  // Not guest RAM: null, scratchpad, and one past the end of each region.
  for (std::uint32_t address : {0x00000000u,
                                0x1F800000u,
                                0x1FFFFFFFu,
                                0x80200000u,
                                0xA0100000u,
                                0xB0000000u,
                                0xFFFFFFFFu}) {
    CHECK(!Spider1Widescreen::isGuestRam(address));
  }
}

// Every publication site as a named address, so a moved constant breaks the build here.
void test_the_publication_addresses_are_the_measured_ones() {
  CHECK_EQ(spider::spider1::kProjectionPublication, 0x80075D0Cu);
  CHECK_EQ(spider::spider1::kDrawEnvironmentConstructor, 0x800884C0u);
  CHECK_EQ(spider::spider1::kViewportRecordCell, 0x800B5918u);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kHorizontalFar), 0);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kVerticalNear), 2);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kHorizontalNear), 4);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kVerticalFar), 6);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kDepthLower), 8);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kDepthUpper), 10);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kLensDivisor), 12);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kScreenDistance), 14);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kCentreX), 16);
  CHECK_EQ(static_cast<int>(Spider1ViewportOffset::kCentreY), 18);
}

// Both leaves are image-scoped native overrides keyed by (image identity, guest address), not
// PlatformHlePlan bindings; the runtime answers platformHlePlan() == nullptr and they still land
// in the dispatch table.
void test_the_leaves_resolve_through_image_scoped_native_overrides() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  CHECK(fixture.runtime.platformHlePlan() == nullptr);

  const auto image =
      fixture.core->imageCatalog().activate("SLUS_008.75 resident", {0x00010000u, 0x000C65D4u}, 1u);
  spider::spider1::installSpider1Widescreen(*fixture.core);

  // Addresses from the executable, as literals.
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, 0x80075D0Cu}));
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, 0x800884C0u}));
  // And the constants the owner used.
  CHECK(fixture.core->nativeDispatcher().isInstalled(
      {image, spider::spider1::kProjectionPublication}));
  CHECK(fixture.core->nativeDispatcher().isInstalled(
      {image, spider::spider1::kDrawEnvironmentConstructor}));
  // Two leaves.
  CHECK_EQ(fixture.core->nativeDispatcher().isInstalled({image, 0x80075D10u}), false);
  CHECK_EQ(fixture.core->nativeDispatcher().isInstalled({image, 0x800884C4u}), false);
  // The frame boundary reaches the owner's policy through the checked downcast.
  CHECK(&Spider1Widescreen::from(*fixture.core) == &owner);
  CHECK(fixture.core->imageCatalog().deactivate(image));
}

} // namespace

int main() {
  RUN(four_three_is_the_identity);
  RUN(the_plan_comes_from_the_frameworks_own_rule);
  RUN(widening_moves_the_centre_and_leaves_the_focal_length_alone);
  RUN(repeated_publication_is_idempotent);
  RUN(a_guest_rewritten_window_is_widened_from_the_new_retail_value);
  RUN(the_horizontal_window_translates_and_the_depth_window_does_not);
  RUN(the_draw_clip_replaces_the_width_only);
  RUN(synchronization_republishes_only_when_the_margin_moved);
  RUN(the_frame_boundary_repairs_a_plan_latched_at_a_stale_display_extent);
  RUN(the_frame_boundary_is_silent_before_the_guest_publishes);
  RUN(the_policy_maps_only_the_players_selection);
  RUN(no_draw_width_means_no_measured_projection);
  RUN(measured_geometry_carries_the_measured_span);
  RUN(the_guest_address_bound_covers_the_titles_own_resident_data);
  RUN(the_publication_addresses_are_the_measured_ones);
  RUN(the_leaves_resolve_through_image_scoped_native_overrides);
  return pt_summary();
}
