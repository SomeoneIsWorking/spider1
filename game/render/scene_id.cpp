// scene_id.cpp — the port of the engine's level-name -> scene-id encoder (guest FUN_8005A734).
//
// Byte-exact with the guest: the folds below are the four `sltiu` range tests at 0x8005A764 /
// 0x8005A77C / 0x8005A794 in order, and the two subtractions the guest does in wrapping 32-bit
// arithmetic are done in uint32_t here for the same reason.
#include "scene_id.h"
#include "core.h"

// The letter folds. 'A' (0x41) - 0x31 == 0x10 and 'a' (0x61) - 0x51 == 0x10, so the encoder is
// case-insensitive and letters occupy 0x10..0x29 — NOT 10..35 as a "digits then letters" reading
// would suggest. Named so the surprise is on the page instead of in a magic constant.
static constexpr uint8_t kLetterBias = 0x31u; // uppercase: name[1] - 0x31
static constexpr uint8_t kLowerBias = 0x51u;  // lowercase: name[1] - 0x51
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
    level = c1 - kAsciiZero; // '0'..'9'
  } else if (c1 - uint32_t('A') < 26u) {
    level = c1 - kLetterBias; // 'A'..'Z'
  } else if (c1 - uint32_t('a') < 26u) {
    level = c1 - kLowerBias; // 'a'..'z'
  } else {
    level = c1; // unfolded, as the guest leaves it
  }

  return (level << 8) | (c3 - kAsciiZero); // wrapping, exactly as `addiu $v0,$v0,-0x30` does
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
