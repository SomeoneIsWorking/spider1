// The level-name string the engine keeps in RAM, and the scene id encoded from it by FUN_8005A734.
#pragma once
#include <stdint.h>

class Core;

namespace spider::render {

// Guest 8005A734 reads name[0] at 0x800A568C through a buffer PAIR at 0x800A5688 and folds the
// first, second and fourth bytes; name[2] is skipped. Written by FUN_8002C354 case 3.
class SceneName {
public:
  // Guest address of name[0]. name[2] is unused by the encoder; kept for diagnostics.
  static constexpr uint32_t kAddr = 0x800A568Cu;
  static constexpr int kBytes = 4;

  // Nothing read yet: all bytes zero and text "....".
  SceneName();
  explicit SceneName(Core *c);

  // Non-printable bytes render as '.'; rawByte() is unfiltered.
  const char *text() const {
    return mText;
  }
  uint8_t rawByte(int i) const {
    return mRaw[i];
  }

  // True when all 4 bytes are printable ASCII; code() still encodes whatever is there, as the guest
  // does.
  bool printable() const {
    return mPrintable;
  }

  // All four bytes zero: no level yet (FUN_80061140's boot-init submit).
  bool unset() const;

  // FUN_8005A734: (level << 8) | sub, the value FUN_80062CE0 switches on. 'd'/'D' in name[0]
  // selects the demo level, otherwise name[1] digits and letters (either case) map to ranges and
  // any other byte is kept raw. sub is name[3] - '0', wrapping 32-bit like the guest's addiu.
  uint32_t code() const;

  bool sameAs(const SceneName &o) const;

private:
  uint8_t mRaw[kBytes];
  char mText[kBytes + 1];
  bool mPrintable;
};

} // namespace spider::render
