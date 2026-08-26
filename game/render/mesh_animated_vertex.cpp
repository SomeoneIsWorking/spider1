// mesh_animated_vertex.cpp — executable-derived animated vertex staging semantics.
#include "mesh_animated_vertex.h"

#include "mips_fixed_point.h"

namespace {

// FUN_8007B798/8007B9CC test these bits at 8007B864/8007BA68 and 8007B918/8007BB28.
constexpr uint16_t kReuseProjectedVertex = 0x0002u;
constexpr uint16_t kRetainProjectedVertex = 0x0001u;

// At 8007B804..8007B808 and 8007BA38..8007BA3C, the retail code defines the reuse-cache
// endpoint as DAT_800B58F0 + 0x1F38. A flagged record subtracts its entire first word from that
// endpoint; it does not interpret the word's halves as X/Y coordinates.
constexpr uint32_t kReuseCacheEndpointOffset = 0x1F38u;

// FUN_80077C08 at 80077C88..80077C8C rejects a mesh when any of these common clip bits remains.
constexpr uint16_t kFaceRejectMask = 0x00BFu;

} // namespace

AnimatedVertexRecord
decodeAnimatedVertexRecord(uint32_t xy, uint32_t zFlags, AnimatedProjectionScale scale) {
  AnimatedVertexRecord result;
  result.source = {mipsSignedHalf(static_cast<uint16_t>(xy)),
                   mipsSignedHalf(static_cast<uint16_t>(xy >> 16u)),
                   mipsSignedHalf(static_cast<uint16_t>(zFlags))};
  result.flags = static_cast<uint16_t>(zFlags >> 16u);
  result.reuseKey = xy;
  result.reusesProjectedVertex = (result.flags & kReuseProjectedVertex) != 0u;
  result.retainsProjectedVertex = (result.flags & kRetainProjectedVertex) != 0u;
  result.requiresProjection = !result.reusesProjectedVertex;

  if (!result.requiresProjection) {
    return result;
  }
  if (scale == AnimatedProjectionScale::FarPreScaled) {
    for (uint32_t axis = 0; axis < result.source.size(); ++axis) {
      result.projectionInput[axis] = mipsArithmeticShiftRight4(result.source[axis]);
    }
  } else {
    result.projectionInput = result.source;
    result.projectionOutputShift = 4u;
  }
  return result;
}

uint32_t animatedVertexReuseAddress(uint32_t projectedVertexBase, uint32_t reuseKey) {
  // Unsigned arithmetic intentionally reproduces MIPS addu/subu wraparound.
  return projectedVertexBase + kReuseCacheEndpointOffset - reuseKey;
}

void accumulateAnimatedVertex(AnimatedVertexSummary &summary, const AnimatedVertexRecord &record) {
  ++summary.total;
  summary.projected += record.requiresProjection ? 1u : 0u;
  summary.reused += record.reusesProjectedVertex ? 1u : 0u;
  summary.retained += record.retainsProjectedVertex ? 1u : 0u;
}

bool animatedFacesAllowed(bool submissionSuppressed, uint16_t commonClipMask) {
  return !submissionSuppressed && (commonClipMask & kFaceRejectMask) == 0u;
}

bool meshAnimatedVertexSelftest() {
  const uint32_t xy = static_cast<uint32_t>(static_cast<uint16_t>(-17)) |
                      (static_cast<uint32_t>(static_cast<uint16_t>(31)) << 16u);
  const uint32_t zFlags = static_cast<uint32_t>(static_cast<uint16_t>(-33)) |
                          (static_cast<uint32_t>(kRetainProjectedVertex) << 16u);
  const AnimatedVertexRecord far =
      decodeAnimatedVertexRecord(xy, zFlags, AnimatedProjectionScale::FarPreScaled);
  const AnimatedVertexRecord near =
      decodeAnimatedVertexRecord(xy, zFlags, AnimatedProjectionScale::NearPostScaled);
  if (!far.requiresProjection || far.reusesProjectedVertex || !far.retainsProjectedVertex ||
      far.projectionInput != std::array<int16_t, 3>{-2, 1, -3} || far.projectionOutputShift != 0u ||
      near.projectionInput != near.source || near.projectionOutputShift != 4u) {
    return false;
  }

  const AnimatedVertexRecord reused =
      decodeAnimatedVertexRecord(0x00000138u,
                                 static_cast<uint32_t>(kReuseProjectedVertex) << 16u,
                                 AnimatedProjectionScale::NearPostScaled);
  if (reused.requiresProjection || !reused.reusesProjectedVertex ||
      animatedVertexReuseAddress(0x800A625Cu, reused.reuseKey) != 0x800A805Cu) {
    return false;
  }

  AnimatedVertexSummary summary;
  accumulateAnimatedVertex(summary, far);
  accumulateAnimatedVertex(summary, reused);
  return summary.total == 2u && summary.projected == 1u && summary.reused == 1u &&
         summary.retained == 1u && animatedFacesAllowed(false, 0x0040u) &&
         !animatedFacesAllowed(false, 0x0001u) && !animatedFacesAllowed(true, 0u);
}
