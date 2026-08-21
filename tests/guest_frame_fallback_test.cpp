#include "guest_frame_fallback.h"

namespace {

bool expect(GuestFrameFallbackDecision got, GuestFrameFallbackDecision want) {
  return got == want;
}

} // namespace

int main() {
  bool ok = true;

  ok &= expect(decideGuestFrameFallback({.enabled = true}),
               GuestFrameFallbackDecision::SubmitGuestFrame);
  ok &= expect(decideGuestFrameFallback({.enabled = false}), GuestFrameFallbackDecision::Disabled);
  ok &= expect(decideGuestFrameFallback({.enabled = true, .nativeProducerReady = true}),
               GuestFrameFallbackDecision::NativeProducerReady);
  ok &= expect(decideGuestFrameFallback({.enabled = true, .nativeSubmissionStarted = true}),
               GuestFrameFallbackDecision::NativeOverlapForbidden);
  ok &= expect(decideGuestFrameFallback({.enabled = true, .interpolationActive = true}),
               GuestFrameFallbackDecision::InterpolationForbidden);

  RenderMode mode;
  mode.setPath(RenderPath::Native);
  {
    GuestFrameFallbackModeScope scope(mode);
    ok &= mode.path() == RenderPath::Gte;
    ok &= mode.psxRender();
    ok &= !mode.softGpu();
    ok &= !mode.enhancementsAllowed();
  }
  ok &= mode.path() == RenderPath::Native;

  return ok ? 0 : 1;
}
