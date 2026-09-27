// Spider-Man 1 (SLUS_008.75) guest widescreen: the title's own projection, culling-window and
// draw-clip owner. Every number and address below was read out of the authenticated USA executable;
// the address that carries each fact is named in the comment that states it. Nothing here is a
// tuned constant.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace spider {

// ---- The measured guest facts this owner acts on ------------------------------------------------
//
// The image's SOLE caller of the two libgte projection leaves, and the only writer of the values
// both of them publish. A whole-image scan of all 524288 words for every `jal`/`j` encoding of the
// leaf addresses finds exactly one each, and Ghidra's reference model agrees: 1 CALL reference and
// 0 other references to each. Its body derives the projection into a u16 record rather than being
// handed one:
//     0x80075E18  lhu  $v0, 0($s4)    /  0x80075E1C  lhu $a0, 4($s4)   -- the horizontal window
//     0x80075E24  subu / 0x80075E28 sra 1 / 0x80075E2C sll 12 / 0x80075E30 div $v1
//     0x80075E64  sh   $v1, 16($s4)                                     -- OFX = (far+near)>>1
//     0x80075E70  sh   $a1, 18($s4)                                     -- OFY = (far+near)>>1
//     0x80075E74  sh   $v0, 14($s4)                                     -- H
//     0x80076180  jal 0x8008BF14 (SetGeomScreen)   with $a0 = H
//     0x80076190  jal 0x8008BF24 (SetGeomOffset)   with $a0 = OFX, $a1 = OFY
// so H, OFX and OFY are all OUTPUTS, and the focal length is a function of the window's own width
// and the per-view lens divisor rather than a literal.
inline constexpr std::uint32_t kProjectionPublication = 0x80075D0Cu;

// The record's own address is passed to it, not fixed, and cached for the rest of the image at
// gp+0x1124 = 0x800B5918 by 0x80075DB0. Eleven other functions read that cell (27 references), so
// the record is the engine's per-view geometry, not a private temporary.
inline constexpr std::uint32_t kViewportRecordCell = 0x800B5918u;

// The guest libgpu draw-environment constructor. It is NOT the libgpu leaf of the same name that
// the eye expects, and the executable settles it: 0x800884C0 writes RECT, then `sh $a3` at +4, then
// u/v at +8/+0xA from the same two arguments that formed RECT.x/y, then isize and a height-derived
// flag at +0x14/+0x17 — the DRAWENV layout. 0x8008735C writes only 20 bytes and is the DISPLAY
// environment. The clip that discards geometry is the one `PutDrawEnv` (0x80081F40) submits, and
// 0x80061328 calls it with ctx+0x00 — the record 0x800884C0 fills. So this is the guest draw-clip
// owner, and its RECT.w arrives in $a3. Three `jal` references, all from 0x80061140 (graphical
// init) and the 0x80065708 allocator-failure report; none through a register or a table.
inline constexpr std::uint32_t kDrawEnvironmentConstructor = 0x800884C0u;

// The record's ten u16 fields, as byte offsets.
//
// THE RECORD IS THREE WINDOWS, NOT ONE. Read out of the executable, not inferred: the 6-bit
// visibility outcode built at 0x8007C2AC and again at 0x8007B9CC wraps a projected vertex as
//     bit0  record[0] < SX      bit1  SX < record[4]        <- HORIZONTAL window
//     bit2  record[2] < SY      bit3  SY < record[6]        <- VERTICAL window
//     bit4  depth < record[8]   bit5  record[10] < depth    <- DEPTH window
// and five further sites (0x80027868, 0x80032880 x2, 0x80060A48, 0x8007F278, 0x80032F04 x2,
// 0x800301E4 x4) range-test the same two words against a GTE IR1/SZ value. The depth pair is
// therefore NOT a horizontal cull, and a horizontal margin has nothing to widen in it. It is also
// RE-ASSERTED by the publication itself: 0x80075DBC writes record[10] from gp+0x1104, and the
// function's tail rewrites record[8], record[10] and record[12] from the view struct it was handed.
// This owner touches the horizontal pair and nothing else; an earlier revision of this file shifted
// all three, which was a no-op for record[8] (the body overwrote it) and a corruption of
// record[10].
struct Spider1ViewportOffset {
  static constexpr int kHorizontalFar = 0;   // one horizontal bound
  static constexpr int kVerticalNear = 2;    // one vertical bound
  static constexpr int kHorizontalNear = 4;  // the other horizontal bound
  static constexpr int kVerticalFar = 6;     // the other vertical bound
  static constexpr int kDepthLower = 8;      // depth window, re-asserted by the publication
  static constexpr int kDepthUpper = 10;     // depth window, re-asserted by the publication
  static constexpr int kLensDivisor = 12;    // divides the window's half-width into H
  static constexpr int kScreenDistance = 14; // H, an output
  static constexpr int kCentreX = 16;        // OFX, an output
  static constexpr int kCentreY = 18;        // OFY, an output
  static constexpr int kFieldCount = 10;
};

// Process-lifetime policy AND publication owner. It answers which aspect the player selected and it
// applies the matching plan to the title's own guest state; declaration alone cannot widen a frame
// (external/psxport/docs/presentation-contract.md, "Title-owned guest widescreen"). The plan itself
// is per-Game in the framework's own latch, so this holds no per-frame state of its own.
class Spider1Widescreen final : public GuestWidescreenProjection {
public:
  // The framework's own latch, injected so a hermetic test drives the production path.
  using Latch = GuestProjectionPlan (*)(Core *, GuestProjectionGeometry);
  // An authenticated original guest body, executed through Lightrec. Injected so a test can observe
  // the transformation without a guest image.
  using RetailBody = void (*)(Core &);

  explicit Spider1Widescreen(Latch latch);

  PresentationAspect presentationAspect(const Core &core) const override;

  // --- site 1: the projection publication, 0x80075D0C
  // --------------------------------------------- Shifts the record's horizontal window by the
  // plan's horizontal margin, then lets the title's OWN derivation run: a uniform shift leaves (far
  // - near) untouched, so H — and therefore the focal length, the vertical field of view and
  // central scale — is bit-identical while the derived OFX becomes retail_centre + margin. On a PSX
  // the horizontal field of view is the ratio OFX/H, not H, so moving OFX outward at unchanged H IS
  // the widening; substituting a smaller H would be a zoom
  // (external/psxport/docs/presentation-contract.md, "What counts as a widening").
  //
  // The cull window's two HORIZONTAL edges shift by the same margin, for the same reason. They are
  // stated in PRE-SHIFT guest coordinates, so every projected X moves by exactly +margin when OFX
  // does; a window that did not move with them would discard the very geometry the widened frame
  // can now show. Measured coverage: every horizontal cull owner reads these two words FROM THIS
  // RECORD (0x8007C2AC, 0x8007B9CC, 0x8007B798, 0x8007B628, 0x8007BE90 as a six-way outcode;
  // 0x80060A48, 0x8007BBD4, 0x8007BD04, 0x8007F278 as a packed 0xBFFF sign-mask pair), so widening
  // the record in place widens all nine at once. The record's DEPTH window is deliberately left
  // alone; see Spider1ViewportOffset.
  void publishProjection(Core &core, const RetailBody &retail);

  // --- site 2: the guest draw clip, 0x800884C0
  // ---------------------------------------------------- Replaces RECT.w only. The retail body
  // remains the authority for RECT.x, RECT.y, RECT.h and every non-RECT field, and it is the body
  // that writes the record the plan is measured from — so the measured retail width is the guest's
  // own, never a literal.
  void publishDrawEnvironment(Core &core, const RetailBody &retail);

  const GuestProjectionPlan &plan() const {
    return plan_;
  }

  // True once the draw clip has published a real RECT. The projection publication measures its
  // geometry against that width, so it refuses rather than guessing one.
  bool drawWidthMeasured() const {
    return measuredDrawWidth_ != 0;
  }

  std::uint32_t measuredDrawWidth() const {
    return measuredDrawWidth_;
  }

  // The retail tuple this owner is widening. Exposed for the tests that pin 4:3 identity and for
  // the log line; not an input to anything.
  struct RetailWindow {
    std::uint16_t horizontalFar = 0;
    std::uint16_t horizontalNear = 0;
    std::uint16_t centreX = 0;
  };

  const RetailWindow &retailWindow() const {
    return retail_;
  }

  // True once a widened window has been published.
  bool published() const {
    return published_;
  }

  // The window the owner publishes, given a plan's margin. Pure, so the tests can pin the rule and
  // this file keeps no second copy of the arithmetic.
  static GuestProjectionGeometry measuredGeometry(std::uint16_t horizontalSpan,
                                                  std::uint16_t verticalSpan,
                                                  std::uint32_t drawWidth);

  // The frame boundary. Re-latches the plan for the CURRENT aspect and live display extent, and
  // applies it to the guest record when the projection centre it demands is not the centre the
  // record already holds.
  //
  // WHY IT MUST NOT RE-ENTER THE GUEST. The plan's PRESENTATION extent — the host canvas width, and
  // the only number the presenter reads — is derived by the framework latch from the live GP1
  // display width, which is NOT constant: measured on the real boot it is 320 while graphical init
  // publishes the draw environment and 512 in the 3D scene. A latch taken at the draw-environment
  // site therefore describes a 428-wide canvas for a 512-wide scene, and the presenter then
  // REFUSES the widening (`present_display_width` returns the native width whenever the wide width
  // does not exceed it). Re-latching per frame is what fixes it, and it needs no guest call: the
  // record is the title's own state, and the value written is the same one the publication derives.
  //
  // Measured 2026-09-27 on the run that found this: the first leg printed
  //   [wide] native picture: aspect=1 wide_engine=0 native_width=512 render_width=512
  //   [wide:warn] a wide picture was REQUESTED and did not happen ...
  // with the guest draw clip correctly widened to 684, because the stored plan still described the
  // 320-wide display extent captured at boot.
  void synchronizePresentation(Core &core);

  // Is this a guest RAM address the owner may read and write? Pure, and public because the bound is
  // exactly the kind of thing a fixture using only low addresses cannot catch: the first live run
  // refused 0x8009A6E4 — inside the title's own resident data — as not guest memory.
  static bool isGuestRam(std::uint32_t address);

  // This title's owner, reached from a Core that is running it. The checked downcast lives here so
  // that no other file repeats the rule "the policy the runtime returns is the owner that published
  // the picture", and so a Core running another title's policy is a named refusal instead of a
  // silent no-op. Same idiom as `Spider1FrameDriver::from`.
  static Spider1Widescreen &from(Core &core);

private:
  GuestProjectionPlan relatch(Core &core, GuestProjectionGeometry geometry);
  // Remember the guest's own window and centre, from the record as it stands. Called only where
  // the record is known to hold retail values.
  void captureRetail(Core &core, std::uint32_t record);

  Latch latch_;
  GuestProjectionPlan plan_;
  RetailWindow retail_;
  std::uint32_t measuredDrawWidth_ = 0;
  int appliedMargin_ = -1;
  bool retailCaptured_ = false;
  bool published_ = false;
};

// Install this title's two measured publication overrides on one Core. Not reachable through
// PlatformHle, which covers the stock library services; the framework's builtin table holds the two
// libgte leaves (it records their CR24/25/26 result) but neither the engine's projection
// publication nor its draw-environment constructor, so the title owns them.
void installSpider1Widescreen(Core &core);

} // namespace spider
