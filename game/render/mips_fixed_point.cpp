// mips_fixed_point.cpp — exact signed decoding and shift semantics for retail packed render state.
#include "mips_fixed_point.h"

#include <bit>

int16_t mipsSignedHalf(uint16_t value) {
  return std::bit_cast<int16_t>(value);
}

int32_t mipsSignedWord(uint32_t value) {
  return std::bit_cast<int32_t>(value);
}

int16_t mipsArithmeticShiftRight4(int16_t value) {
  uint16_t shifted = static_cast<uint16_t>(std::bit_cast<uint16_t>(value) >> 4u);
  if (value < 0) {
    shifted = static_cast<uint16_t>(shifted | 0xF000u);
  }
  return mipsSignedHalf(shifted);
}
