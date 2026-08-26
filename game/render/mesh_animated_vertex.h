// mesh_animated_vertex.h — source-side contract for Spider-Man's animated vertex records.
#ifndef SPIDER1_GAME_RENDER_MESH_ANIMATED_VERTEX_H
#define SPIDER1_GAME_RENDER_MESH_ANIMATED_VERTEX_H

#include <array>
#include <cstdint>

enum class AnimatedProjectionScale {
  // FUN_8007B798 shifts each signed source coordinate before RTPS and retains MAC1..3 verbatim.
  FarPreScaled,
  // FUN_8007B9CC submits the full source coordinates and shifts MAC1..3 after RTPS.
  NearPostScaled,
};

struct AnimatedVertexRecord {
  std::array<int16_t, 3> source{};
  std::array<int16_t, 3> projectionInput{};
  uint32_t reuseKey = 0;
  uint16_t flags = 0;
  uint8_t projectionOutputShift = 0;
  bool requiresProjection = false;
  bool reusesProjectedVertex = false;
  bool retainsProjectedVertex = false;
};

struct AnimatedVertexSummary {
  uint32_t total = 0;
  uint32_t projected = 0;
  uint32_t reused = 0;
  uint32_t retained = 0;
};

AnimatedVertexRecord
decodeAnimatedVertexRecord(uint32_t xy, uint32_t zFlags, AnimatedProjectionScale scale);
uint32_t animatedVertexReuseAddress(uint32_t projectedVertexBase, uint32_t reuseKey);
void accumulateAnimatedVertex(AnimatedVertexSummary &summary, const AnimatedVertexRecord &record);
bool animatedFacesAllowed(bool submissionSuppressed, uint16_t commonClipMask);
bool meshAnimatedVertexSelftest();

#endif // SPIDER1_GAME_RENDER_MESH_ANIMATED_VERTEX_H
