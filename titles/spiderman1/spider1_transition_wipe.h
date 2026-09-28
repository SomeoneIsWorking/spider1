// spider1_transition_wipe.h — the authored 3D transition SLUS_008.75 plays between modes.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider {

// The 3D transition, as one owner: which half of the frame it starts from, the per-pixel darkening
// it applies, the four-row scratch it streams through, and the mode state it publishes when it is
// done.
//
// It is separable because none of that is the mode driver's business. The driver decides WHICH mode
// comes after the wipe; the wipe decides how the picture gets there, and it says where it landed
// with one enum value. Keeping the row loop and the channel arithmetic beside the calls that feed
// them is also what makes the wipe readable at all: as part of the mode driver it was a 130-line
// method whose `256` and `1024` and `4096` had to be looked up somewhere else.
//
// IT IS PRESENTATION, NOT LOADING. The wipe is an authored transition and stays: it is two fields
// long, it draws, and it is what the player sees between the logo, the menu, and a level. Nothing
// here waits on a load or skips ahead.
class Spider1TransitionWipe final {
public:
  // Where the wipe landed, which is the mode driver's decision to make and not the wipe's. The wipe
  // publishes its own completion state; whatever the destination does with it is the driver's step,
  // and it happens after.
  enum class Destination {
    // The title menu's load is under way.
    Menu,
    // A level name has been committed and the level route takes over.
    Level,
    // The outer cycle re-enters with the argument this route armed.
    OuterCycle,
  };

  explicit Spider1TransitionWipe(Spider1ModeHost &host);

  // Arm the wipe. `startOffset` is the vertical offset of the display half the wipe starts from:
  // zero for the upper half, one buffer height for the lower one.
  void begin(Destination continuation, uint16_t startOffset);

  // The wipe's first field: copy the opposite half into the starting one and present it.
  void stepFirst(Core &core);

  // The wipe's second field: darken the frame a row at a time through the GPU and present it.
  // Returns where the title goes next.
  Destination stepSecond(Core &core);

  // The continuation this wipe was armed with, for a caller that wants to name it.
  Destination continuation() const {
    return continuation_;
  }
  uint16_t startOffset() const {
    return startOffset_;
  }

  // One 16-bit pixel through the wipe's darkening. Pure, and public because it is the rule rather
  // than the plumbing: the two channels that keep the smaller share of the total are the ones that
  // make the image redden as it darkens.
  static uint16_t darkenPixel(uint16_t pixel);

private:
  Spider1ModeHost &host_;
  Destination continuation_ = Destination::OuterCycle;
  uint16_t startOffset_ = 0;
};

} // namespace spider
