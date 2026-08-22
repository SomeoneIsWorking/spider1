// face_builder_census.cpp — enumerate every executable callsite that owns FUN_8007C4D8 input.
#include "face_builder_census.h"

namespace {

// Ghidra reports exactly twelve direct calls to FUN_8007C4D8 in SLUS_008.75. MIPS JAL writes
// call-PC + 8 to ra, so the live return address distinguishes both calls in FUN_80077EC8. Keeping
// the call instruction beside it makes this table independently checkable against the executable.
constexpr std::array<FaceBuilderCallsite, 12> kCallsites{{
    {0x80028BF0u, 0x80028BF8u, "FUN_80028030"},
    {0x80077ADCu, 0x80077AE4u, "FUN_80077A48"},
    {0x80077BE8u, 0x80077BF0u, "FUN_80077B9C"},
    {0x80077CA0u, 0x80077CA8u, "FUN_80077C08"},
    {0x80077E38u, 0x80077E40u, "FUN_80077D64"},
    {0x80077EA8u, 0x80077EB0u, "FUN_80077E5C"},
    {0x8007872Cu, 0x80078734u, "FUN_80077EC8"},
    {0x800787ACu, 0x800787B4u, "FUN_80077EC8"},
    {0x8002F240u, 0x8002F248u, "FUN_8002EED4"},
    {0x8002FC88u, 0x8002FC90u, "FUN_8002F828"},
    {0x80079D78u, 0x80079D80u, "FUN_80079574"},
    {0x8007A608u, 0x8007A610u, "FUN_80079DB8"},
}};

void addObservation(FaceBuilderStats &stats, const FaceBuilderObservation &observation) {
  ++stats.calls;
  stats.faces += observation.faceCount;
  stats.contextualCalls += observation.hasMeshHeaderContext ? 1u : 0u;
  if (observation.outputDeltaValid) {
    stats.emittedBytes += observation.emittedBytes;
  } else {
    ++stats.invalidOutputDeltas;
  }
}

} // namespace

FaceBuilderRecord FaceBuilderCensus::record(const FaceBuilderObservation &observation) {
  addObservation(totals_, observation);
  for (std::size_t i = 0; i < kCallsites.size(); ++i) {
    if (kCallsites[i].returnAddress == observation.returnAddress) {
      const bool firstCall = byCallsite_[i].calls == 0u;
      addObservation(byCallsite_[i], observation);
      return {&kCallsites[i], byCallsite_[i], firstCall};
    }
  }

  const bool firstCall = unknown_.calls == 0u;
  addObservation(unknown_, observation);
  return {nullptr, unknown_, firstCall};
}

const FaceBuilderStats &FaceBuilderCensus::totals() const {
  return totals_;
}

const FaceBuilderStats &FaceBuilderCensus::unknown() const {
  return unknown_;
}

const FaceBuilderStats &FaceBuilderCensus::stats(std::size_t callsiteIndex) const {
  return byCallsite_[callsiteIndex];
}

const std::array<FaceBuilderCallsite, 12> &spiderFaceBuilderCallsites() {
  return kCallsites;
}

bool faceBuilderCensusSelftest() {
  FaceBuilderCensus census;
  const FaceBuilderRecord known = census.record({0x80077E40u, 1u, 480u, true, true});
  const FaceBuilderRecord otherKnown = census.record({0x80079D80u, 16u, 64u, false, true});
  const FaceBuilderRecord unknown = census.record({0x80000008u, 2u, 0u, false, false});
  const FaceBuilderRecord repeated = census.record({0x80077E40u, 3u, 32u, true, true});

  return known.callsite != nullptr && known.callsite->callInstruction == 0x80077E38u &&
         known.firstCall && otherKnown.callsite != nullptr && otherKnown.firstCall &&
         unknown.callsite == nullptr && unknown.firstCall && repeated.callsite == known.callsite &&
         !repeated.firstCall && repeated.stats.calls == 2u && repeated.stats.faces == 4u &&
         repeated.stats.emittedBytes == 512u && repeated.stats.contextualCalls == 2u &&
         census.totals().calls == 4u && census.totals().faces == 22u &&
         census.totals().emittedBytes == 576u && census.totals().invalidOutputDeltas == 1u &&
         census.unknown().calls == 1u && census.unknown().invalidOutputDeltas == 1u;
}
