#pragma once

#include <cstdint>

class Core;

// Build-derived, executable-authenticated STR body. The original generated super stays compiled;
// this runtime-override body differs only at the three measured field waits.
void spider1_native_movie_body(Core *core);

namespace spider {

// Called only by the build-derived body at the original VSync return PCs. The title frame driver
// turns this into a cooperative fiber yield; it never dispatches or emulates guest VSync.
void spider1_movie_field(Core *core, uint32_t returnPc);

} // namespace spider
