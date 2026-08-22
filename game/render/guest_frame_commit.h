#pragma once

struct Core;

namespace spiderman::render {

// End one retail display-list submission in psxport's unified render queue. Spider-Man owns this
// frame boundary because it runs the game's recompiled frame loop instead of native_step_frame.
void commitCapturedGuestFrame(Core *core);

} // namespace spiderman::render
