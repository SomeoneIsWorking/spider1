#include "guest_frame_commit.h"

#include "core.h"
#include "game.h"
#include <lucent/log.h>

namespace spiderman::render {

void commitCapturedGuestFrame(Core *core) {
  // FUN_80061308 is Spider-Man's one retail submitFrame operation (RE-19). Its DrawOTag walk may
  // flush the render queue more than once, and FramePresenter accumulates all of those flushes
  // until the game declares the frame complete. The framework's native loop normally owns that
  // declaration; this port runs the guest loop, so this game-local runtime responsibility bridges
  // the already-proven engine boundary to the shared queue fence.
  core->game->presentation.commit(core, 0, core->game->temporalPresentation.get());

  static bool reported = false;
  if (!reported) {
    reported = true;
    lucent::info("framefence",
                 "captured guest render queue committed at FUN_80061308 submitFrame boundary");
  }
}

} // namespace spiderman::render
