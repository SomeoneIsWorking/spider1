#include "spider1_mode_frame_boundary.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_mode_decisions.h"

#include "core.h"

namespace spider {

void spider1ModeDrainDrawFields(Core &core,
                                Spider1ModeHost &host,
                                uint32_t drawSyncReturnPc,
                                uint32_t fieldServiceReturnPc) {
  while (true) {
    host.waitFields(core, spider1::oneField);
    core.r[4] = 1;
    spider1CallGuest(core, spider1::drawSync, drawSyncReturnPc);
    if (spider1ModeDrawSyncComplete(core.r[2])) {
      return;
    }
    spider1CallGuest(core, spider1::fieldService, fieldServiceReturnPc);
  }
}

void spider1ModeWaitUnadvancedField(Core &core,
                                    Spider1ModeHost &host,
                                    uint32_t vblankAtFrameStart) {
  if (spider1ModeFrameNeedsFieldWait(vblankAtFrameStart, core.mem_r32(spider1::gameVblankCount))) {
    host.waitFields(core, spider1::oneField);
  }
}

void spider1ModeCompleteFrameHandshake(Core &core, uint32_t fieldServiceReturnPc) {
  if (core.mem_r32(spider1::frameHandshake) != 0) {
    return;
  }
  spider1CallGuest(core, spider1::fieldService, fieldServiceReturnPc);
  core.mem_w32(spider1::frameHandshake, 1);
}

} // namespace spider
