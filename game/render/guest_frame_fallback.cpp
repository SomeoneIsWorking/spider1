#include "guest_frame_fallback.h"

GuestFrameFallbackDecision decideGuestFrameFallback(const GuestFrameFallbackInputs &inputs) {
  if (inputs.nativeProducerReady) {
    return GuestFrameFallbackDecision::NativeProducerReady;
  }
  if (inputs.nativeSubmissionStarted) {
    return GuestFrameFallbackDecision::NativeOverlapForbidden;
  }
  if (inputs.interpolationActive) {
    return GuestFrameFallbackDecision::InterpolationForbidden;
  }
  if (!inputs.enabled) {
    return GuestFrameFallbackDecision::Disabled;
  }
  return GuestFrameFallbackDecision::SubmitGuestFrame;
}

const char *guestFrameFallbackDecisionName(GuestFrameFallbackDecision decision) {
  switch (decision) {
  case GuestFrameFallbackDecision::SubmitGuestFrame:
    return "SUBMIT_GUEST_FRAME";
  case GuestFrameFallbackDecision::NativeProducerReady:
    return "NATIVE_PRODUCER_READY";
  case GuestFrameFallbackDecision::Disabled:
    return "DISABLED";
  case GuestFrameFallbackDecision::InterpolationForbidden:
    return "INTERPOLATION_FORBIDDEN";
  case GuestFrameFallbackDecision::NativeOverlapForbidden:
    return "NATIVE_OVERLAP_FORBIDDEN";
  }
  return "INVALID";
}

GuestFrameFallbackModeScope::GuestFrameFallbackModeScope(RenderMode &mode)
    : mMode(mode), mPrior(mode.path()) {
  mMode.setPath(RenderPath::Gte);
}

GuestFrameFallbackModeScope::~GuestFrameFallbackModeScope() {
  mMode.setPath(mPrior);
}
