// The attract/title screen mode for levels with no playable route.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// Owns the alternate mode's object, per-field UI script, keep-running flag and repeat probe.
// Repeat needs both the released object's repeat byte and a file that produced something.
class Spider1AlternateMode final {
public:
  explicit Spider1AlternateMode(Spider1ModeHost &host) : host_(host) {}

  // What one alternate field asked for; a continuing field has already presented.
  enum class Outcome {
    Continue,
    // The title re-enters the primary mode.
    RestartPrimary,
  };

  // Arm the flag and mark the object for its begin call on the next field.
  void begin();

  // One field, presenting itself when it continues; an ending field presents the last frame.
  Outcome step(Core &core);

  // Whether the mode flag is still set.
  bool running() const {
    return flag_;
  }
  // Whether the object still needs its begin call (the object asked to repeat).
  bool objectNeedsBegin() const {
    return needsInit_;
  }

private:
  // Present the last field, release the object, read its file; true when the mode runs again.
  // Called from `step` while the field's guest stack window is live.
  bool finish(Core &core);

  Spider1ModeHost &host_;
  bool flag_ = true;
  bool needsInit_ = false;
};

} // namespace spider::spider1
