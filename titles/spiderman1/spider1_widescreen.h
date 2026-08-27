#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace spider {

struct Spider1ViewportHorizontal {
  uint16_t left = 0;
  uint16_t right = 0;
  uint16_t lensDivisor = 0;

  bool operator==(const Spider1ViewportHorizontal &) const = default;
};

// Widen the viewport around its existing low edge while scaling the title's lens divisor by the
// same ratio. FUN_80075D0C computes H from width/lensDivisor, so this preserves focal length while
// expanding the horizontal cull/projection span.
Spider1ViewportHorizontal spider1ProjectViewport(Spider1ViewportHorizontal native,
                                                 int projectedWidth);

class Spider1Widescreen final : public GuestWidescreenProjection {
public:
  PresentationAspect presentationAspect(const Core &) const override;
  void install() const;

private:
  void publishAndRender(Core &core) const;
  static void publishAndRenderOverride(Core *core);

  mutable bool initialized_ = false;
  mutable Spider1ViewportHorizontal native_{};
  mutable Spider1ViewportHorizontal applied_{};
};

} // namespace spider
