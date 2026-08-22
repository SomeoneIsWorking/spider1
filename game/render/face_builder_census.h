// face_builder_census.h — binary-derived ownership census for FUN_8007C4D8 submissions.
#ifndef SPIDER1_GAME_RENDER_FACE_BUILDER_CENSUS_H
#define SPIDER1_GAME_RENDER_FACE_BUILDER_CENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>

struct FaceBuilderCallsite {
  uint32_t callInstruction = 0;
  uint32_t returnAddress = 0;
  const char *ownerFunction = nullptr;
};

struct FaceBuilderObservation {
  uint32_t returnAddress = 0;
  uint32_t faceCount = 0;
  uint32_t emittedBytes = 0;
  bool hasMeshHeaderContext = false;
  bool outputDeltaValid = true;
};

struct FaceBuilderStats {
  uint64_t calls = 0;
  uint64_t faces = 0;
  uint64_t emittedBytes = 0;
  uint64_t contextualCalls = 0;
  uint64_t invalidOutputDeltas = 0;
};

struct FaceBuilderRecord {
  const FaceBuilderCallsite *callsite = nullptr;
  FaceBuilderStats stats;
  bool firstCall = false;
};

class FaceBuilderCensus {
public:
  FaceBuilderRecord record(const FaceBuilderObservation &observation);

  const FaceBuilderStats &totals() const;
  const FaceBuilderStats &unknown() const;
  const FaceBuilderStats &stats(std::size_t callsiteIndex) const;

private:
  std::array<FaceBuilderStats, 12> byCallsite_{};
  FaceBuilderStats unknown_{};
  FaceBuilderStats totals_{};
};

const std::array<FaceBuilderCallsite, 12> &spiderFaceBuilderCallsites();
bool faceBuilderCensusSelftest();

#endif // SPIDER1_GAME_RENDER_FACE_BUILDER_CENSUS_H
