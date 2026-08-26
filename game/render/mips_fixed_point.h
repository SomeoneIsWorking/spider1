// mips_fixed_point.h — exact signed decoding and shift semantics for retail packed render state.
#ifndef SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H
#define SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H

#include <cstdint>

int16_t mipsSignedHalf(uint16_t value);
int32_t mipsSignedWord(uint32_t value);
int16_t mipsArithmeticShiftRight4(int16_t value);

#endif // SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H
