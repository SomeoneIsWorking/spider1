#pragma once

class Core;

namespace spider {

// Yield a retail StGetNext dry poll to Spider-Man 1's native field owner. The guest still receives
// its original "not ready" result; this only gives the host the display field during which the
// asynchronous drive and SPU would progress on hardware.
void spider1_stream_wait_field(Core *core);

} // namespace spider
