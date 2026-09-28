// spider1_invalid_selector_input.h — the gate the title shows when its outer selector is not one
// the jump table knows.
#pragma once

#include <cstdint>

class Core;

namespace spider {

// The invalid-selector input wait, as one concept.
//
// What the mode driver would otherwise carry inline: a start field, two latched press flags, a
// setup call, and a per-poll read whose only job is to decide whether to go back to the outer
// cycle. It is separable because it owns ALL of that state and touches nothing else: it neither
// presents a field nor submits a frame nor writes a mode word, so a reader can hold the whole rule
// in their head without the mode state machine around it.
//
// The rule itself — what "released" and "timed out" mean — is `spider1ModeInvalidInputSatisfied` in
// `spider1_mode_decisions.h`, which is a pure function of the observations. This class supplies the
// observations, in the retail order: it performs the guest pad read first, then reads the pad
// bytes.
class Spider1InvalidSelectorInput final {
public:
  // Ask the guest to arm the wait, and record the field it started on. `fieldsToWait` is the
  // argument the guest's own setup call is given, and it is NOT the timeout: the timeout is a field
  // count compared against the title's counter, and the two are separate numbers on purpose.
  void begin(Core &core, uint32_t fieldsToWait);

  // One poll. Returns true when the gate opens, which is the mode driver's signal to re-enter the
  // outer cycle; false leaves the mode waiting, and the caller is expected to present a field.
  bool poll(Core &core);

  // The poll that last happened, as the raw pad bytes it observed. Exposed so a diagnostic or a
  // test can read what the gate saw without re-deriving the pad block's offsets.
  uint8_t lastSecondKeyByte() const {
    return lastSecondKey_;
  }
  uint8_t lastFirstKeyByte() const {
    return lastFirstKey_;
  }
  uint32_t startField() const {
    return startField_;
  }

private:
  uint32_t startField_ = 0;
  uint8_t lastSecondKey_ = 0;
  uint8_t lastFirstKey_ = 0;
  bool sawSecondKeyPressed_ = false;
  bool sawFirstKeyPressed_ = false;
};

} // namespace spider
