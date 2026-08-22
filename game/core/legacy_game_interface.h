#pragma once

struct GameConfig;
struct GameHooks;

namespace spider::legacy {

// These tables are bounded compatibility debt for generic framework algorithms that still read
// Core::cfg/Core::hooks. SpiderRuntime is the title's ownership seam; new behavior and policy
// belong there, while measured literals stay here until psxport exposes narrow typed fact
// interfaces.
extern const GameConfig &measuredConfig;
extern const GameHooks &compatibilityHooks;

} // namespace spider::legacy
