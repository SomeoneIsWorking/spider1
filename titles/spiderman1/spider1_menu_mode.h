// spider1_menu_mode.h — Spider-Man 1's title menu mode.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider {

// The title menu, as one owner: the menu object's lifetime, its per-field UI script, and the result
// its exit test produced.
//
// It is separable because it owns ALL of that state. The menu object is allocated by the mode and
// released by the mode, the frame counter is the mode's, and the accepted/cancelled result is the
// mode's — the mode driver only ever needs to ask which of the two happened, and it needs that
// answer AFTER the menu has torn itself down.
//
// The guest entry points it calls are named in `spider1_guest_layout.h`. Two of them are worth
// noting here rather than in the layout, because they are what make the menu a MENU and not a list
// of calls: `uiExitTest` is the call whose result word decides the menu's outcome, and the final
// call goes through the object's own vtable rather than through a fixed entry, so the object's
// class decides what ending it is.
class Spider1MenuMode final {
public:
  explicit Spider1MenuMode(Spider1ModeHost &host) : host_(host) {}

  // What one menu field asked for. The menu presents its own field before returning, so a caller
  // that gets either answer has already reached exactly one fence. WHICH end it was is not in here:
  // it is `accepted()`, because a mode driver needs that fact one step later, after the menu has
  // torn itself down and no longer has a state machine of its own.
  enum class Outcome {
    // The menu is still running.
    Continue,
    // The menu ran its exit test, tore itself down, and published its exit state. Whether the
    // player
    // accepted is `accepted()`.
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
  // Present the menu's last field, release the object through its own vtable, and publish the exit
  // state. Called from `step` on the field that ends the menu, while the guest stack window that
  // field is using is still live.
  void finish(Core &core);

  Spider1ModeHost &host_;
  uint32_t object_ = 0;
  uint32_t fieldCount_ = 0;
  bool accepted_ = false;
};

} // namespace spider
