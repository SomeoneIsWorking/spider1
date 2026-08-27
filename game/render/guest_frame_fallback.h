// guest_frame_fallback.h — ownership gate for Spider-Man's temporary whole-guest-frame fallback.
//
// This is deliberately NOT a native display-list producer. When a scene has no native producer, the
// fallback may submit the guest's already-complete frame through FUN_80061308: the exact retail
// ResetGraph/PutDispEnv/PutDrawEnv/DrawOTag body. The frame is mutually exclusive with native
// production: no native envelope or geometry may have been submitted first. The guest submission is
// forced through RenderPath::Gte so the actual guest-time GTE/OT result reaches the PC rasterizer
// with native enhancements disabled.
//
// Interpolated presentation is categorically refused. Replaying a captured guest packet queue on an
// interpolated present would invent intermediate output that the guest never produced, which is
// outside the user's authorization and would turn this explicit debt into a guessed producer.
#ifndef SPIDER1_GAME_RENDER_GUEST_FRAME_FALLBACK_H
#define SPIDER1_GAME_RENDER_GUEST_FRAME_FALLBACK_H

#include "render_mode.h"

enum class GuestFrameFallbackDecision {
  SubmitGuestFrame,
  NativeProducerReady,
  Disabled,
  InterpolationForbidden,
  NativeOverlapForbidden,
};

struct GuestFrameFallbackInputs {
  bool enabled = false;
  bool nativeProducerReady = false;
  bool nativeSubmissionStarted = false;
  bool interpolationActive = false;
};

// Pure production decision seam. Tests drive this exact function through every answer, including
// the two safety refusals; the live render seam consumes the same verdict before drawing anything.
GuestFrameFallbackDecision decideGuestFrameFallback(const GuestFrameFallbackInputs &inputs);
const char *guestFrameFallbackDecisionName(GuestFrameFallbackDecision decision);

// During the guest body and its one presentation fence, Gte is the one mode whose definition is
// exactly "guest geometry from the OT, PC rasterizer, no native enhancements".
class GuestFrameFallbackModeScope {
public:
  explicit GuestFrameFallbackModeScope(RenderMode &mode);
  ~GuestFrameFallbackModeScope();

  GuestFrameFallbackModeScope(const GuestFrameFallbackModeScope &) = delete;
  GuestFrameFallbackModeScope &operator=(const GuestFrameFallbackModeScope &) = delete;

private:
  RenderMode &mMode;
  RenderPath mPrior;
};

#endif // SPIDER1_GAME_RENDER_GUEST_FRAME_FALLBACK_H
