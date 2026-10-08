// Pure rules the mode drivers decide by.
#pragma once

#include <cstdint>

namespace spider::spider1 {

// Did this frame's own guest work cross a display field? A frame that crossed none still needs one.
constexpr bool modeFrameNeedsFieldWait(uint32_t vblankAtFrameStart, uint32_t vblankCount) {
  return vblankCount == vblankAtFrameStart;
}

// Did the draw-sync call report the GPU finished? The guest's result word is zero when it has.
constexpr bool modeDrawSyncComplete(uint32_t guestResult) {
  return guestResult == 0u;
}

// Invalid-selector gate: has the player released a pressed button, or has the wait timed out?
// A key latches on the first poll that reads zero and releases the gate on a later non-zero
// poll; a key that never latched cannot release it. `fieldsElapsed` saturates by subtraction.
constexpr bool modeInvalidInputSatisfied(bool sawSecondKeyPressed,
                                         uint8_t secondKey,
                                         bool sawFirstKeyPressed,
                                         uint8_t firstKey,
                                         uint32_t fieldsElapsed,
                                         uint32_t timeoutFields) {
  const bool released =
      (sawSecondKeyPressed && secondKey != 0u) || (sawFirstKeyPressed && firstKey != 0u);
  return released || fieldsElapsed >= timeoutFields;
}

// Why an alternate-mode frame leaves the mode. A press is consumed (pad bytes cleared) before the
// mode acts; a cleared flag exits silently.
struct Spider1AlternateExit {
  // The mode is finished, whether by its flag or by a press.
  bool exits = false;
  // The finish was a press, and that press must be consumed before the mode acts on it.
  bool byPad = false;
};

// The mode runs while its flag is set; a press while the flag is already clear ends nothing
// further.
constexpr Spider1AlternateExit modeAlternateExit(bool alternateFlag, bool padPressed) {
  if (!alternateFlag) {
    return {.exits = true, .byPad = false};
  }
  return {.exits = padPressed, .byPad = padPressed};
}

// The alternate mode repeats only if the object's repeat byte is set and the re-read file produced
// something.
constexpr bool modeAlternateRepeats(bool objectAskedToRepeat, uint32_t reloadedBytes) {
  return objectAskedToRepeat && reloadedBytes != 0u;
}

// Saturates at 255 so a wrapped counter never reads as an unvisited level.
constexpr uint8_t modeSaturatingIncrement(uint8_t counter) {
  return counter < 255u ? static_cast<uint8_t>(counter + 1u) : counter;
}

} // namespace spider::spider1
