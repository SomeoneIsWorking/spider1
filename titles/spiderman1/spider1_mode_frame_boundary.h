#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// Frame tail shared by the primary, menu and alternate modes. The return addresses are parameters
// because each mode resumes at the `jal` return address of its own call site.

// Deliver fields until the GPU reports the draw finished.
void modeDrainDrawFields(Core &core,
                         Spider1ModeHost &host,
                         uint32_t drawSyncReturnPc,
                         uint32_t fieldServiceReturnPc);

// Wait one more field if this frame's guest work did not already cross one.
void modeWaitUnadvancedField(Core &core, Spider1ModeHost &host, uint32_t vblankAtFrameStart);

// Service the submitted frame's display field once; the handshake word is cleared by the frame
// body.
void modeCompleteFrameHandshake(Core &core, uint32_t fieldServiceReturnPc);

} // namespace spider::spider1
