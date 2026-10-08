#pragma once

#include <cstdint>

class Core;

namespace spider::spider1 {

// Owns the start field and latched press flags; the release/timeout rule is
// `modeInvalidInputSatisfied`. The guest pad read must run before the pad bytes are read.
class Spider1InvalidSelectorInput final {
public:
  // `fieldsToWait` is the guest setup argument, not the timeout.
  void begin(Core &core, uint32_t fieldsToWait);

  // One poll; true when the gate opens.
  bool poll(Core &core);

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

} // namespace spider::spider1
