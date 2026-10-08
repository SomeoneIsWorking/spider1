// Spider-Man 1's title menu mode.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// The title menu: owns the menu object, its frame counter and the accepted/cancelled result, which
// the mode driver reads after the menu has torn itself down.
class Spider1MenuMode final {
public:
  explicit Spider1MenuMode(Spider1ModeHost &host) : host_(host) {}

  // The menu presents its own field before returning; `accepted()` says which way it ended.
  enum class Outcome {
    Continue,
    Finished,
  };

  // Build the menu object and begin its first field. Presents nothing itself.
  void begin(Core &core);

  // One menu field, including its own present.
  Outcome step(Core &core);

  // What the menu's exit test decided. Only meaningful after a `Finished` field.
  bool accepted() const {
    return accepted_;
  }
  // The object the mode owns, for a diagnostic that wants to name it. Zero before `begin`.
  uint32_t object() const {
    return object_;
  }
  // How many fields the menu has run, for the same reason.
  uint32_t fieldCount() const {
    return fieldCount_;
  }

private:
  // Present the last field, release the object through its vtable and publish the exit state.
  void finish(Core &core);

  Spider1ModeHost &host_;
  uint32_t object_ = 0;
  uint32_t fieldCount_ = 0;
  bool accepted_ = false;
};

} // namespace spider::spider1
