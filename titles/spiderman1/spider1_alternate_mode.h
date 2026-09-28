// spider1_alternate_mode.h — Spider-Man 1's alternate mode, the attract/title screen the outer
// selector enters when the chosen level has no playable route.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider {

// The alternate mode, as one owner: its object's lifetime, its per-field UI script, its own
// "keep running" flag, and the repeat probe that decides whether it runs again or hands the title
// back to the primary mode.
//
// It is separable because it owns ALL of that state. The mode driver needs two things from it — the
// entry point, and whether the last field finished for good — and nothing else.
//
// THE MODE'S OWN REPEAT RULE, which is two conditions and not one: the released object sets its own
// repeat byte, AND the file it asks to re-open must actually produce something. Requiring both is
// what stops an alternate mode from looping forever on a file the drive can no longer supply, and
// it is `spider1ModeAlternateRepeats` in `spider1_mode_decisions.h`.
class Spider1AlternateMode final {
public:
  explicit Spider1AlternateMode(Spider1ModeHost &host) : host_(host) {}

  // What one alternate field asked for. A field that continues has already presented.
  enum class Outcome {
    // The attract screen is still running.
    Continue,
    // The attract screen is done and the title is to re-enter the primary mode.
    RestartPrimary,
  };

  // Enter the mode: arm the flag and mark the object for its begin call on the next field.
  void begin();

  // One alternate field, including its own present when it continues. A field that ends the mode
  // presents the mode's last frame instead.
  Outcome step(Core &core);

  // Whether the mode is still running, for a diagnostic that wants to name the mode's own flag.
  bool running() const {
    return flag_;
  }
  // Whether the object still needs its begin call, which is what "the object asked to repeat" means
  // on the wire.
  bool objectNeedsBegin() const {
    return needsInit_;
  }

private:
  // Present the mode's last field, release the object, and read the file it asked for. Returns true
  // when the mode is to run again. Called from `step` while that field's guest stack window is
  // live, because the object read writes its result into that window.
  bool finish(Core &core);

  Spider1ModeHost &host_;
  bool flag_ = true;
  bool needsInit_ = false;
};

} // namespace spider
