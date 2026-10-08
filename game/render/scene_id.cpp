// Level-name -> scene-id encoder, port of guest FUN_8005A734 (sltiu tests at 0x8005A764/77C).
#include "scene_id.h"
#include "core.h"

namespace spider::render {

// 'A' - 0x31 == 'a' - 0x51 == 0x10: letters are case-insensitive and occupy 0x10..0x29.
static constexpr uint8_t kLetterBias = 0x31u;
static constexpr uint8_t kLowerBias = 0x51u;
static constexpr uint32_t kDemoLevel = 0x99u; // name[0] == 'd'/'D' selects this level index
static constexpr uint8_t kAsciiZero = 0x30u;

SceneName::SceneName() {
  for (int i = 0; i < kBytes; ++i) {
    mRaw[i] = 0;
    mText[i] = '.';
  }
  mText[kBytes] = '\0';
  mPrintable = false;
}

SceneName::SceneName(Core *c) {
  mPrintable = true;
  for (int i = 0; i < kBytes; ++i) {
    mRaw[i] = (uint8_t)c->mem_r8(kAddr + (uint32_t)i);
    const bool ok = mRaw[i] >= 0x20u && mRaw[i] < 0x7Fu;
    mPrintable = mPrintable && ok;
    mText[i] = ok ? (char)mRaw[i] : '.';
  }
  mText[kBytes] = '\0';
}

uint32_t SceneName::code() const {
  const uint32_t c0 = mRaw[0], c1 = mRaw[1], c3 = mRaw[3];

  uint32_t level;
  if (c0 == 'd' || c0 == 'D') {
    level = kDemoLevel;
  } else if (c1 - kAsciiZero < 10u) {
    level = c1 - kAsciiZero;
  } else if (c1 - uint32_t('A') < 26u) {
    level = c1 - kLetterBias;
  } else if (c1 - uint32_t('a') < 26u) {
    level = c1 - kLowerBias;
  } else {
    level = c1; // unfolded, as the guest leaves it
  }

  return (level << 8) | (c3 - kAsciiZero); // wraps like the guest's addiu $v0,$v0,-0x30
}

bool SceneName::unset() const {
  for (int i = 0; i < kBytes; ++i) {
    if (mRaw[i] != 0) {
      return false;
    }
  }
  return true;
}

bool SceneName::sameAs(const SceneName &o) const {
  for (int i = 0; i < kBytes; ++i) {
    if (mRaw[i] != o.mRaw[i]) {
      return false;
    }
  }
  return true;
}

} // namespace spider::render
