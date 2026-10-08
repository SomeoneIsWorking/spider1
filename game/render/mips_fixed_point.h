#ifndef SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H
#define SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H

#include <cstdint>

namespace spider::render {

int16_t mipsSignedHalf(uint16_t value);
int32_t mipsSignedWord(uint32_t value);
int16_t mipsArithmeticShiftRight4(int16_t value);

} // namespace spider::render

#endif // SPIDER1_GAME_RENDER_MIPS_FIXED_POINT_H
