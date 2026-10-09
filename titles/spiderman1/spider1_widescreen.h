// Spider-Man 1 (SLUS_008.75) guest widescreen: projection, culling-window and draw-clip owner.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// Sole caller of the two libgte projection leaves: SetGeomScreen (0x80076180) and SetGeomOffset
// (0x80076190) read H, OFX and OFY out of the record, so they are outputs.
inline constexpr std::uint32_t kProjectionPublication = 0x80075D0Cu;

// The record is the publication's $a1 argument. The body caches it here (gp+0x1124) and sixteen
// other functions read it, so the cell is zero until the first publication has run.
inline constexpr std::uint32_t kViewportRecordCell = 0x800B5918u;

// Guest draw-environment constructor; 0x8008735C builds the display environment. The clip is the
// one PutDrawEnv 0x80081F40 submits and RECT.w arrives in $a3.
inline constexpr std::uint32_t kDrawEnvironmentConstructor = 0x800884C0u;

// The record is three windows: horizontal (offsets 0 and 4), vertical (2 and 6) and depth (8 and
// 10, re-asserted by the publication at 0x80075DBC). This owner touches the horizontal pair only.
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

// Policy and publication owner: selects the aspect and applies the plan to the title's guest state.
class Spider1Widescreen final : public GuestWidescreenProjection {
public:
  // The framework latch, injected for tests.
  using Latch = GuestProjectionPlan (*)(Core *, GuestProjectionGeometry);
  // An original guest body executed through Lightrec; injected so tests need no guest image.
  using RetailBody = void (*)(Core &);

  explicit Spider1Widescreen(Latch latch);

  // Site 1, 0x80075D0C, record in $a1: shifts the record's horizontal window by the plan's margin,
  // then runs the title's own derivation. H is unchanged, so OFX becoming retail centre + margin
  // widens the FOV.
  void publishProjection(Core &core, const RetailBody &retail);

  // Site 2, the guest draw clip 0x800884C0: replaces RECT.w only; the retail body owns the rest.
  void publishDrawEnvironment(Core &core, const RetailBody &retail);

  const GuestProjectionPlan &plan() const {
    return plan_;
  }

  // True once the draw clip has published a real RECT; the projection publication measures against
  // it.
  bool drawWidthMeasured() const {
    return measuredDrawWidth_ != 0;
  }

  std::uint32_t measuredDrawWidth() const {
    return measuredDrawWidth_;
  }

  // The retail tuple being widened.
  struct RetailWindow {
    std::uint16_t horizontalFar = 0;
    std::uint16_t horizontalNear = 0;
    std::uint16_t centreX = 0;
  };

  const RetailWindow &retailWindow() const {
    return retail_;
  }

  bool published() const {
    return published_;
  }

  // The window published for a plan's margin; pure.
  static GuestProjectionGeometry measuredGeometry(std::uint16_t horizontalSpan,
                                                  std::uint16_t verticalSpan,
                                                  std::uint32_t drawWidth);

  // Frame boundary: re-latches the plan for the current aspect and live display extent and applies
  // it to the guest record; the GP1 display width is 320 during graphical init and 512 in the 3D
  // scene, so a latch taken at the draw-environment site would size the canvas wrongly.
  void synchronizePresentation(Core &core);

  // Guest RAM bound check for the owner's reads and writes.
  static bool isGuestRam(std::uint32_t address);

  // This title's owner, reached from a Core running it; refuses by name if another title's policy
  // is installed.
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

// Installs the two publication overrides on one Core; PlatformHle does not cover them.
void installSpider1Widescreen(Core &core);

} // namespace spider::spider1
