// spider1_mode_decisions.h — the rules the mode drivers DECIDE by, as pure functions of what they
// observed.
//
// WHY THESE ARE HERE AND NOT INLINE IN THE MODES. Four of them were open-coded more than once
// across the two drivers, and a rule written out three times is a rule that will be changed in one
// place and left behind in the others. Each is a `constexpr` function of its inputs — no memory, no
// `Core`, no state — so each can be tested over its whole input space instead of over one run of
// the game, and so the mode bodies read as a sequence of named questions.
//
// Each function's comment states the contract it owns, INCLUDING the contract it had before,
// because a test that only pins the new shape is a test that would let a behaviour change through.
#pragma once

#include <cstdint>

namespace spider {

// Did this frame's own guest work cross a display field? Every mode that finishes a frame body asks
// exactly this, and the answer decides whether the mode waits a whole extra field before
// presenting.
//
// THE CONTRACT, stated exactly: a frame that crossed NO field still needs one, and a frame that
// crossed one or several does not. The rule reads the field COUNTER rather than a boolean, so
// "several" cannot be mistaken for "none" — which is the reason this is a named function and not
// the bare comparison written out at three call sites.
constexpr bool spider1ModeFrameNeedsFieldWait(uint32_t vblankAtFrameStart, uint32_t vblankCount) {
  return vblankCount == vblankAtFrameStart;
}

// Did the draw-sync call report that the GPU has finished? The field-service loop keeps servicing
// fields while it has NOT, and submits the frame the first time it has. The guest's own result word
// is zero when it has.
constexpr bool spider1ModeDrawSyncComplete(uint32_t guestResult) {
  return guestResult == 0u;
}

// The invalid-selector gate: has the player released a button it pressed, or has the wait given up?
//
// THE CONTRACT, precisely, because a reader is meant to be able to get it wrong on purpose:
//   * `sawSecondKeyPressed` and `sawFirstKeyPressed` latch the first poll on which the
//     corresponding pad byte read ZERO, so they are "this player has pressed and let go", not "this
//     player is holding".
//   * A latched key releases the gate on the first LATER poll that reads non-zero.
//   * A key that was never latched can never release the gate, however long it reads non-zero. This
//     is the asymmetry that makes the gate a press-and-release and not a "some button" test, and it
//     is the whole of the invalid-selector behaviour.
//
// `fieldsElapsed` is the title's field counter minus the field the gate started on, and it
// saturates by subtraction, so a gate that has run long past its timeout still times out.
constexpr bool spider1ModeInvalidInputSatisfied(bool sawSecondKeyPressed,
                                                uint8_t secondKey,
                                                bool sawFirstKeyPressed,
                                                uint8_t firstKey,
                                                uint32_t fieldsElapsed,
                                                uint32_t timeoutFields) {
  const bool released =
      (sawSecondKeyPressed && secondKey != 0u) || (sawFirstKeyPressed && firstKey != 0u);
  return released || fieldsElapsed >= timeoutFields;
}

// Why an alternate-mode frame leaves the mode, which is not one answer but two.
//
// `byPad` is the case that is ALSO CONSUMED before the mode acts: the press that ends the attract
// screen has its pad bytes cleared, so it cannot end the pass that follows it. A mode that ran out
// of its own flag exits SILENTLY — no sound, nothing cleared — so the two cases are genuinely
// different and a caller must be able to tell them apart. Getting that wrong is a behaviour change,
// not a style question, which is why the rule returns both facts instead of a bare bool.
struct Spider1AlternateExit {
  // The mode is finished, whether by its flag or by a press.
  bool exits = false;
  // The finish was a press, and that press must be consumed before the mode acts on it.
  bool byPad = false;
};

// The mode runs while its flag is set. A cleared flag ends it silently; a pad press ends it and is
// consumed. A press while the flag is already clear ends nothing further, because the mode is
// already on its way out.
constexpr Spider1AlternateExit spider1ModeAlternateExit(bool alternateFlag, bool padPressed) {
  if (!alternateFlag) {
    return {.exits = true, .byPad = false};
  }
  return {.exits = padPressed, .byPad = padPressed};
}

// Does the alternate mode run again after its finish? The object decides by its own repeat byte,
// and the port only repeats when the file re-read afterwards actually produced something. Both
// halves are required: the byte alone would loop forever on a file the drive could no longer
// supply.
constexpr bool spider1ModeAlternateRepeats(bool objectAskedToRepeat, uint32_t reloadedBytes) {
  return objectAskedToRepeat && reloadedBytes != 0u;
}

// Saturating visit counter: a level's counter stops at 255 rather than wrapping, because a wrapped
// counter would read as a level nobody has visited.
constexpr uint8_t spider1ModeSaturatingIncrement(uint8_t counter) {
  return counter < 255u ? static_cast<uint8_t>(counter + 1u) : counter;
}

} // namespace spider
