// Falsifiers for SLUS_008.75's guest-widescreen owner.
//
// Every case here pins a PRODUCTION contract of `spider::Spider1Widescreen`, and each is written so
// that the mutation it claims to catch makes it fail. Nothing in this file reimplements the plan:
// the latch is the framework's own pure `guest_projection_plan`, so a change to the framework's
// widening rule moves these expectations with it instead of letting a title's copy of the
// arithmetic rot.
//
// The measured inputs are the ones read out of the authenticated executable:
//   0x80061140  addiu $a3,$zero,512 / addiu $s1,$zero,240   -> the draw RECT the guest publishes
//   0x80075E18  lhu $v0,0($s4) ; 0x80075E1C lhu $a0,4($s4)  -> the 512-wide horizontal window
//   0x80075E64  sh $v1,16($s4)                              -> OFX = (far+near)>>1
// so a 512-wide window centred on 256 is the retail geometry, not an invention of this file.

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

using spider::Spider1ViewportOffset;
using spider::Spider1Widescreen;

inline constexpr std::uint32_t kRecordAddress = 0x800B0000u;
// The retail 512x240 window read at 0x80075D90-0x80075E74: near/far 0..512, cull first 0, lens
// 2365, H 276, OFX 256, OFY 120. 276 and 256 are also the values RE-17's runtime gate printed from
// the real boot, so this is a cross-checked tuple rather than a plausible one.
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

// The minimum a `GameRuntime` must answer for the framework's latch to consult it at all. It adds
// nothing to the owner under test: its only job is to hand the latch the same policy the shipping
// `Spider1Runtime` returns, so the latch is exercised through the production seam rather than
// around it.
class TestRuntime final : public GameRuntime {
public:
  explicit TestRuntime(const GuestWidescreenProjection *policy) : policy_(policy) {}
  void setPolicy(const GuestWidescreenProjection *policy) {
    policy_ = policy;
  }
  const GuestWidescreenProjection *guestWidescreenProjection() const override {
    return policy_;
  }
  // The four lifecycle hooks a GameRuntime must answer. This fixture drives the owner directly, so
  // they have nothing to do and are named rather than left undefined.
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
    // The framework's latch reads BOTH the Core's render path and the title's own policy, and it
    // ignores the requested aspect when either is missing. `RenderPath::Gte` is this title's
    // SHIPPING path (RenderCapabilities::widescreenOnly()), and the policy is the very owner under
    // test, so the fixture installs the same wiring a real run has — the runtime BEFORE the Game,
    // because `Game`'s constructor is what captures it. A latch driven without it is a latch that
    // can never widen anything, and every expectation below would be vacuous.
    owner = std::make_unique<Spider1Widescreen>(gpu_vk_latch_guest_projection);
    runtime.setPolicy(owner.get());
    psxport_install_game(runtime);
    game = std::make_unique<Game>();
    core = &game->core;
    core->rsub.mode.setPath(RenderPath::Gte);
    // The framework's latch takes the PRESENTATION extent from the GP1 display width, and
    // 0x80061140 gave the display environment a 512-wide RECT on the real boot. A default 320 here
    // would make the plan describe a different game.
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
    core->mem_w32(spider::kViewportRecordCell, kRecordAddress);
    // The guest's own draw environment, as 0x800884C0 wrote it.
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

  // The guest's own body, recovered instruction by instruction from 0x80075D0C. Recomputing the
  // outputs rather than writing them is the point: it means a test can tell the difference between
  // "the owner wrote the widened centre" and "the owner's widening made the title's own derivation
  // produce the widened centre", and only the second is the mechanism this port is allowed to use.
  //
  // The arithmetic is the guest's, including its two-stage fixed-point division, because a
  // simplified formula would not reproduce H = 276 and every focal-length expectation here would be
  // resting on a model instead of on the executable:
  //   0x80075E24 subu 0x80075E28 sra 1 0x80075E2C sll 12 0x80075E30 div $v1(lens) 0x80075E34 mflo
  //   0x80075E3C sll 12                  0x80075E40 div $a1(gp+0x1140)     0x80075E44 mflo
  //   0x80075E74 sh -> record+14 (H)
  //   0x80075E54 addu 0x80075E60 sra 1    0x80075E64 sh -> record+16 (OFX)
  //   0x80075E68 addu 0x80075E6C sra 1    0x80075E70 sh -> record+18 (OFY)
  // The retail run printed H = 276 and OFX = 256 with a 512-wide window, and 256<<12 / 2365 << 12 /
  // 6574 is 276 exactly, which is what fixes the scale factor this fixture uses.
  static constexpr std::int32_t kRetailScale = 6574;

  static void retailProjection(Core &core) {
    const std::uint32_t record = core.mem_r32(spider::kViewportRecordCell);
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
    // 0x80075DBC: the publication also recomputes record[10] from its own scroll (gp+0x1104)
    // rather than from the record, and its tail rewrites record[8], record[10] and record[12] from
    // the view struct it was handed. The owner must not treat either word as a persistent input —
    // the guest overwrites both on every call — so a fixture that did not reproduce the write would
    // make the owner look cumulative when it is not, and one that shifted them would look correct
    // here while corrupting a depth band the real guest overwrites anyway.
    core.mem_w16(record + Spider1ViewportOffset::kDepthUpper, kRetailDepthUpper);
  }

  static void retailDrawEnvironment(Core &core) {
    // 0x800884C0: RECT.x/$a1, RECT.y/$a2, RECT.w/$a3, RECT.h/the stack argument, then the u/v pair
    // and the flags. The owner must leave every field but RECT.w alone, so the body writes all of
    // them.
    const std::uint32_t environment = core.r[4];
    core.mem_w16(environment + 0, static_cast<std::uint16_t>(core.r[5]));
    core.mem_w16(environment + 2, static_cast<std::uint16_t>(core.r[6]));
    core.mem_w16(environment + 4, static_cast<std::uint16_t>(core.r[7]));
    core.mem_w16(environment + 6, static_cast<std::uint16_t>(core.mem_r32(0x00100000u)));
  }

  // The retail draw-environment constructor takes RECT.w in $a3 (0x8008736C stores it) and RECT.h
  // as a fifth stack argument. The arguments are set here and the BODY writes the record, exactly
  // as the guest does, so the owner is measured against a rectangle the guest really published.
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

// 4:3 IDENTITY. The strongest form of this claim is byte-identity: snapshot the whole record and
// the whole draw environment, run both publications at Standard4x3, and compare every field. A
// mutation that writes even one guest byte on the 4:3 leg fails here.
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

// The 16:9 plan this geometry produces, pinned against the framework's OWN pure builder rather than
// a copy of it, so a change in the framework's widening rule is visible here instead of being
// masked.
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
  // 512 -> the smallest even width that does not undershoot 16:9, per the framework's own rule.
  CHECK_EQ(expected.projectionExtent.width, 684);
  CHECK_EQ(expected.projectionCenterX, 342);
  CHECK_EQ(expected.projectionHorizontalMargin, 86);
  // The PRESENTATION extent is a separate fact: it comes from the GP1 display width the guest
  // published, not from the projection the owner measured. 0x80061140/0x800884C0 gave the display
  // environment a 512-wide RECT, so the framework widens THAT to the same 684. Asserting it here
  // keeps the two extents from being confused, which is the mistake the contract calls out by name
  // ("Those values are not interchangeable").
  CHECK_EQ(owner.plan().presentationExtent.width, 684);
  CHECK_EQ(owner.plan().nativeProjectionExtent.width, 512);
  CHECK_EQ(owner.plan().nativeGuestDrawWidth, kRetailDrawWidth);
  CHECK_EQ(owner.plan().guestClipRight, 683);
}

// THE WIDENING, pinned on the mechanism rather than the outcome: OFX must move outward, H must not.
// A mutation that instead shrinks the window (a ZOOM) leaves OFX at 256 and fails; a mutation that
// rescales the lens divisor to force H leaves H at 276 and fails.
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

  // The window is what the title's own derivation consumed, and it moved by exactly the plan's
  // margin.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar),
           owner.retailWindow().horizontalFar + owner.plan().projectionHorizontalMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           owner.retailWindow().horizontalNear + owner.plan().projectionHorizontalMargin);
  // ...and (far - near) is therefore UNCHANGED, which is the whole reason H is unchanged.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) -
               fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           kRetailFar - kRetailNear);
  // OFX/H grew by exactly the canvas ratio: the horizontal field of view, not central scale. The
  // relation is centre/window-width, because H cancels — which is the whole reason a smaller H
  // would have been a zoom instead.
  CHECK_EQ(static_cast<int>(fixture.readWord(Spider1ViewportOffset::kCentreX)) *
               (kRetailFar - kRetailNear),
           static_cast<int>(kRetailCentreX) * owner.plan().projectionExtent.width);
}

// Issue 0022's defect, as a test. The publication runs once per rendered frame, so a widening that
// adds its margin to whatever it finds would grow 512 -> 684 -> 912 -> 1024. Three consecutive
// publications must be byte-identical to the first.
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

// An area or view change re-authors the window. The owner must widen THAT window, from the value
// the guest just wrote, and still not accumulate.
void test_a_guest_rewritten_window_is_widened_from_the_new_retail_value() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);
  const int margin = owner.plan().projectionHorizontalMargin;

  // The guest re-authors a 640-wide window centred on 320. A wider native window is a DIFFERENT
  // view, so the plan is re-derived from it and its margin is its own — 640 widens to 854, not
  // 640+86. The case being pinned is that the owner re-captures the retail tuple from what the
  // guest just wrote instead of adding the OLD margin to the NEW window, and that the new window's
  // own focal length is the title's to derive: 640 wide at lens 2365 is a shorter projection
  // distance, and the owner must neither prevent that nor pretend otherwise.
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
  // The window's span is still exactly what the guest authored, so the owner's contribution to this
  // view's focal length is still nil.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) -
               fixture.readWord(Spider1ViewportOffset::kHorizontalNear),
           640u);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kLensDivisor), kRetailLens);
  // The title's own derivation ran on the new window; the owner did not restore a stale distance.
  CHECK(fixture.readWord(Spider1ViewportOffset::kScreenDistance) != distanceForThisViewBefore);
}

// The cull window is stated in PRE-SHIFT guest coordinates, so it must translate with the picture.
// The non-obvious consequence, from the contract: the widened band is [retail_first + margin,
// The HORIZONTAL cull window translates with the picture, and the record's DEPTH window does not
// move at all. The window is stated in PRE-SHIFT guest coordinates (record[0] and record[4], the
// two horizontal edges, as 0x8007C2AC and 0x8007B9CC compare them), so the widened band is [retail
// + margin, retail + width + margin) and NOT [0, wide). An object the retail frame drew at the
// extreme left is now at +margin, and a cull left at its retail edge discards it.
void test_the_horizontal_window_translates_and_the_depth_window_does_not() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  fixture.publishProjection(owner);
  const int margin = owner.plan().projectionHorizontalMargin;

  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear), kRetailNear + margin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar + margin);
  // Not [0, wide): the window moved, it did not stretch to the frame.
  CHECK(fixture.readWord(Spider1ViewportOffset::kHorizontalNear) != 0u);
  CHECK(fixture.readWord(Spider1ViewportOffset::kHorizontalFar) !=
        owner.plan().projectionExtent.width);
  // A guest x of 0 is now at +margin and must remain inside the window.
  const std::uint32_t shiftedLeftEdge = static_cast<std::uint32_t>(margin);
  CHECK(shiftedLeftEdge >= fixture.readWord(Spider1ViewportOffset::kHorizontalNear));
  CHECK(shiftedLeftEdge < fixture.readWord(Spider1ViewportOffset::kHorizontalFar));

  // THE DEPTH WINDOW IS A DIFFERENT AXIS AND IS NOT THE OWNER'S TO MOVE. Measured: 0x8007C2AC and
  // 0x8007B9CC compare record[8]/record[10] against a GTE IR1/SZ value, not against a screen
  // coordinate (outcode bits 4 and 5), and nine further sites range-test the same pair. A
  // horizontal margin widens nothing in a depth band — and the publication re-asserts both words
  // itself (0x80075DBC writes record[10] from gp+0x1104; the function's tail rewrites record[8],
  // record[10] and record[12] from the view struct), so shifting them was both wrong in kind and
  // undone or overwritten. This case is the executable's word, and an earlier revision of this
  // owner failed it.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kDepthLower), kRetailDepthLower);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kDepthUpper), kRetailDepthUpper);
  // The vertical window likewise, for the same reason: it is a different axis.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kVerticalNear), kRetailVerticalNear);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kVerticalFar), kRetailVerticalFar);
}

// Only RECT.w is the owner's. x, y, h and the u/v pair the retail body wrote are the guest's.
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

// A presentation that already re-published its plan must not be doubled, and an aspect that changes
// must reach the guest rather than only the host canvas. The frame boundary calls this with NO
// retail body, which is the load-bearing part: it re-enters no guest code, so it is safe to run on
// every frame.
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
  // An unchanged aspect must not republish: the guest record is already what the plan says.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), 512u + wideMargin);

  // The load-bearing half. A synchronization that only re-latched the plan would leave this at
  // 512+86 with a 4:3 plan beside it — the host canvas back to 4:3 around a guest frustum that is
  // still wide. That is the half that is easy to write and the half that actually has to be there.
  fixture.game->mods.aspect = ASPECT_4_3;
  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, 0);
  CHECK(!owner.plan().widescreen());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalNear), kRetailNear);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kScreenDistance), kRetailH);

  // And back out again, so the boundary is not one-way.
  fixture.game->mods.aspect = ASPECT_16_9;
  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().projectionHorizontalMargin, wideMargin);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), owner.plan().projectionCenterX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), 512u + wideMargin);
}

// THE DEFECT THIS OWNER HAD, AS A TEST. Measured 2026-09-27 on a real-disc run: the guest published
// its draw environment while the framework's decoded GP1 display width was still 320, so the plan
// latched there described a 428-wide HOST canvas for a 512-wide 3D scene. The presenter then
// refused the widening — `present_display_width` returns the native width whenever the wide width
// does not exceed it — and the run printed
//     [wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=512
//     [wide:warn] a wide picture was REQUESTED and did not happen ...
// with the guest draw clip correctly widened to 684. The guest projection was right and the PICTURE
// was still 4:3.
//
// So the case pins the whole chain: a plan latched at a stale display extent, a live display extent
// that has since reached the scene's 512, and a frame boundary that must turn the stored plan into
// the 684-wide canvas — while the guest record is left exactly as the publication left it.
void test_the_frame_boundary_repairs_a_plan_latched_at_a_stale_display_extent() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  // What the real boot looks like at the draw-environment site.
  fixture.core->game->gpu.s_disp_w = 320;
  fixture.core->game->gpu.s_disp_h = 240;
  fixture.game->mods.aspect = ASPECT_16_9;
  fixture.publishDraw(owner);
  CHECK_EQ(owner.plan().presentationExtent.width, 428);
  // The guest's own 3D scene display mode, reached after graphical init.
  fixture.core->game->gpu.s_disp_w = kRetailDrawWidth;
  fixture.core->game->gpu.s_disp_h = kRetailDrawHeight;

  owner.synchronizePresentation(*fixture.core);
  CHECK_EQ(owner.plan().presentationExtent.width, 684);
  CHECK(owner.plan().widescreen());
  // And the number the presenter actually reads is the one the announce line compares.
  CHECK(owner.plan().presentationExtent.width > owner.plan().nativeExtent.width);
  // The guest projection was already right and must not have moved.
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kHorizontalFar), kRetailFar);
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kScreenDistance), kRetailH);
}

// A frame boundary before the guest has published anything must write NOTHING — no plan, no record,
// no captured retail. A synchronizer that latched a plan from a default geometry here would
// advertise a widening for a game that has not stated one.
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

  // The draw environment alone is still not enough: the projection publication is what fixes the
  // window this owner widens.
  fixture.publishDraw(owner);
  owner.synchronizePresentation(*fixture.core);
  CHECK(!owner.published());
  CHECK_EQ(fixture.readWord(Spider1ViewportOffset::kCentreX), kRetailCentreX);
}

// The aspect policy maps the player's selection and nothing else. ASPECT_AUTO is deliberately NOT
// folded to 16:9: it resolves against the live sink inside the plan builder, and a headless run
// with no wide sink must resolve to 4:3 rather than claim a widening it did not perform.
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

// The refusal that keeps a claim honest: with no draw environment published there is no measured
// guest draw width, and substituting the window's own width would be inventing one. This cannot be
// observed without aborting, so it is asserted through the state the owner exposes instead — a
// projection publication that ran first leaves the owner with no measured width and no published
// window.
void test_no_draw_width_means_no_measured_projection() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  fixture.game->mods.aspect = ASPECT_16_9;
  CHECK(!owner.drawWidthMeasured());
  CHECK_EQ(owner.measuredDrawWidth(), 0u);
  CHECK(!owner.published());
  // The plan this geometry would produce is still derivable, and its draw width is the window's own
  // — which is precisely why the production path refuses to latch it before the guest states one.
  const GuestProjectionGeometry geometry =
      Spider1Widescreen::measuredGeometry(kRetailFar - kRetailNear, kRetailDrawHeight, kRetailFar);
  CHECK_EQ(geometry.extent.width, 512);
  CHECK_EQ(geometry.drawWidth, 512);
}

// The pure geometry rule, on the retail tuple. Its refusal path aborts, so it is exercised where a
// refusal is legal (a projection publication with no measured draw width) rather than by trapping.
void test_measured_geometry_carries_the_measured_span() {
  const GuestProjectionGeometry good = Spider1Widescreen::measuredGeometry(512, 240, 512);
  CHECK_EQ(good.extent.width, 512);
  CHECK_EQ(good.extent.height, 240);
  CHECK_EQ(good.drawWidth, 512);
  CHECK(good.valid());
}

// The guest-address bound, against the addresses the guest's OWN boot code uses. This case exists
// because the first live run refused 0x8009A6E4 as "not guest memory" — an address 0x9A6E4 bytes
// into a 2 MiB image, plainly inside main RAM and inside the title's own resident data. A fixture
// that only ever used low addresses could not have caught it, so the bound is pinned against the
// real ones.
void test_the_guest_address_bound_covers_the_titles_own_resident_data() {
  // 0x80061150/0x8006117C/0x80061194/0x800611AC publish the four environments here, and the bss the
  // port's own GuestProgramImage records ends at 0x800B5994.
  for (std::uint32_t address : {0x8009A6E4u,
                                0x8009A740u,
                                0x8009A75Cu,
                                0x8009A7B8u,
                                0x800B5918u,
                                0x800B47F4u,
                                0x801FFFFFu}) {
    CHECK(Spider1Widescreen::isGuestRam(address));
  }
  // The 2 MiB of parallel RAM at KSEG1 0xA0000000, which a few titles use for a second buffer.
  CHECK(Spider1Widescreen::isGuestRam(0xA0000000u));
  CHECK(Spider1Widescreen::isGuestRam(0xA00FFFFFu));
  // And what is NOT guest RAM: a null, a scratchpad address, and one past the end of each region.
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

// Every measured publication site, restated as an address the code names. A rename or a moved
// constant must break the build here rather than silently install an override at the wrong guest
// address.
void test_the_publication_addresses_are_the_measured_ones() {
  CHECK_EQ(spider::kProjectionPublication, 0x80075D0Cu);
  CHECK_EQ(spider::kDrawEnvironmentConstructor, 0x800884C0u);
  CHECK_EQ(spider::kViewportRecordCell, 0x800B5918u);
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

// WHICH MECHANISM THE TWO LEAVES RESOLVE THROUGH, and it is answered here rather than asserted in a
// comment. Both are installed as IMAGE-SCOPED NATIVE OVERRIDES, keyed by (image identity, guest
// address) in the framework's own dispatch table — and NOT as `PlatformHlePlan::bindings[]`
// entries.
//
// The distinction is not cosmetic. It is the defect psxport fixed in 7860e20a and proved with
// tests/test_vsync_ownership.cpp: `resolveHostDispatch` used to consult the HLE table BEFORE the
// image-scoped native override table, which made a title-owned override UNREACHABLE for any address
// the HLE table also claimed. A title that installed its projection owner as an HLE binding that
// replaced a framework standard handler would be relying on that ordering. This owner installs
// native overrides, so it does not depend on the ordering at all — and the fixture's runtime
// answers `platformHlePlan() == nullptr`, so the two installs below happen with NO HLE plan in
// existence and still land in the dispatch table. That is the mechanical form of the claim.
//
// It also closes a real coverage gap: before this case, the suite pinned the two addresses only as
// header constants, which a mutation of the INSTALL SITE could have moved without anything
// noticing.
void test_the_leaves_resolve_through_image_scoped_native_overrides() {
  Fixture fixture;
  Spider1Widescreen &owner = *fixture.owner;
  // The fixture's TestRuntime declares no platform HLE plan at all.
  CHECK(fixture.runtime.platformHlePlan() == nullptr);

  const auto image =
      fixture.core->imageCatalog().activate("SLUS_008.75 resident", {0x00010000u, 0x000C65D4u}, 1u);
  spider::installSpider1Widescreen(*fixture.core);

  // The addresses read out of the executable, as literals, against the table the install filled.
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, 0x80075D0Cu}));
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, 0x800884C0u}));
  // And the constants the owner used, which is the pair a moved install site would change.
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, spider::kProjectionPublication}));
  CHECK(fixture.core->nativeDispatcher().isInstalled({image, spider::kDrawEnvironmentConstructor}));
  // Two leaves, not one and not three.
  CHECK_EQ(fixture.core->nativeDispatcher().isInstalled({image, 0x80075D10u}), false);
  CHECK_EQ(fixture.core->nativeDispatcher().isInstalled({image, 0x800884C4u}), false);
  // The owner's own policy is what the frame boundary reaches, through the checked downcast.
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
