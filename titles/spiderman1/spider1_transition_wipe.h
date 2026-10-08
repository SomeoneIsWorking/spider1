// The authored 3D transition SLUS_008.75 plays between modes.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// The 3D transition: start half, per-pixel darkening, four-row scratch, published mode state.
// Presentation, not loading: it stays.
class Spider1TransitionWipe final {
public:
  // Where the wipe landed; the mode driver decides what follows.
  enum class Destination {
    // The title menu's load is under way.
    Menu,
    // A level name has been committed and the level route takes over.
    Level,
    // The outer cycle re-enters with the argument this route armed.
    OuterCycle,
  };

  explicit Spider1TransitionWipe(Spider1ModeHost &host);

  // `startOffset` is the vertical offset of the starting display half: 0 upper, one buffer height
  // lower.
  void begin(Destination continuation, uint16_t startOffset);

  // First field: copy the opposite half into the starting one and present it.
  void stepFirst(Core &core);

  // Second field: darken the frame row by row through the GPU and present it.
  Destination stepSecond(Core &core);

  Destination continuation() const {
    return continuation_;
  }
  uint16_t startOffset() const {
    return startOffset_;
  }

  // One pixel through the darkening; pure.
  static uint16_t darkenPixel(uint16_t pixel);

private:
  Spider1ModeHost &host_;
  Destination continuation_ = Destination::OuterCycle;
  uint16_t startOffset_ = 0;
};

} // namespace spider::spider1
