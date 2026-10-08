#include "spider1_mode_frame_boundary.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_mode_decisions.h"

#include "core.h"

namespace spider::spider1 {

void modeDrainDrawFields(Core &core,
                         Spider1ModeHost &host,
                         uint32_t drawSyncReturnPc,
                         uint32_t fieldServiceReturnPc) {
  while (true) {
    host.waitFields(core, oneField);
    core.r[4] = 1;
    enterGuestCall(core, drawSync, drawSyncReturnPc);
    if (modeDrawSyncComplete(core.r[2])) {
      return;
    }
    enterGuestCall(core, fieldService, fieldServiceReturnPc);
  }
}

void modeWaitUnadvancedField(Core &core, Spider1ModeHost &host, uint32_t vblankAtFrameStart) {
  if (modeFrameNeedsFieldWait(vblankAtFrameStart, core.mem_r32(gameVblankCount))) {
    host.waitFields(core, oneField);
  }
}

void modeCompleteFrameHandshake(Core &core, uint32_t fieldServiceReturnPc) {
  if (core.mem_r32(frameHandshake) != 0) {
    return;
  }
  enterGuestCall(core, fieldService, fieldServiceReturnPc);
  core.mem_w32(frameHandshake, 1);
}

} // namespace spider::spider1
