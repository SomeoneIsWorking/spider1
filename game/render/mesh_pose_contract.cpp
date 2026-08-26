// mesh_pose_contract.cpp — executable-derived animated pose-composition inputs.
#include "mesh_pose_contract.h"

#include "mips_fixed_point.h"

namespace {

template <std::size_t WordCount>
std::array<int16_t, 9> decodeRotation(const std::array<uint32_t, WordCount> &words) {
  static_assert(WordCount >= 5u);
  std::array<int16_t, 9> rotation{};
  for (std::size_t element = 0; element < rotation.size(); ++element) {
    const uint32_t word = words[element / 2u];
    const uint32_t shift = static_cast<uint32_t>(element % 2u) * 16u;
    rotation[element] = mipsSignedHalf(static_cast<uint16_t>(word >> shift));
  }
  return rotation;
}

uint64_t hashWord(uint64_t hash, uint32_t word) {
  // FNV-1a is only an observation correlation key; correctness never depends on collision freedom.
  for (uint32_t byte = 0; byte < sizeof(word); ++byte) {
    hash ^= static_cast<uint8_t>(word >> (byte * 8u));
    hash *= 1099511628211ull;
  }
  return hash;
}

uint64_t poseSignature(const MeshPoseInput &input) {
  uint64_t hash = 1469598103934665603ull;
  hash = hashWord(hash, input.key.owner);
  hash = hashWord(hash, input.key.sourcePose);
  hash = hashWord(hash, input.scale == AnimatedProjectionScale::FarPreScaled ? 0u : 1u);
  for (uint32_t word : input.baseTransformWords) {
    hash = hashWord(hash, word);
  }
  for (uint32_t word : input.secondaryRotationWords) {
    hash = hashWord(hash, word);
  }
  for (uint32_t word : input.sourcePoseWords) {
    hash = hashWord(hash, word);
  }
  return hash;
}

} // namespace

bool MeshPoseKey::valid() const {
  return owner != 0u && sourcePose != 0u;
}

MeshPoseContract decodeMeshPoseInput(const MeshPoseInput &input) {
  MeshPoseContract result;
  result.key = input.key;
  result.scale = input.scale;
  result.baseRotation = decodeRotation(input.baseTransformWords);
  for (uint32_t axis = 0; axis < result.baseTranslation.size(); ++axis) {
    result.baseTranslation[axis] = mipsSignedWord(input.baseTransformWords[5u + axis]);
  }
  result.secondaryRotation = decodeRotation(input.secondaryRotationWords);
  result.sourceRotation = decodeRotation(input.sourcePoseWords);
  for (uint32_t axis = 0; axis < result.sourceTranslation.size(); ++axis) {
    const uint32_t byteOffset = 18u + axis * sizeof(uint16_t);
    const uint32_t word = input.sourcePoseWords[byteOffset / sizeof(uint32_t)];
    const uint32_t shift = (byteOffset % sizeof(uint32_t)) * 8u;
    result.sourceTranslation[axis] = mipsSignedHalf(static_cast<uint16_t>(word >> shift));
    result.compositionTranslation[axis] =
        input.scale == AnimatedProjectionScale::FarPreScaled
            ? mipsArithmeticShiftRight4(result.sourceTranslation[axis])
            : result.sourceTranslation[axis];
  }
  result.signature = poseSignature(input);
  result.valid = input.key.valid();
  return result;
}

bool meshPoseSamplesCanInterpolate(const MeshPoseTemporalSample &previous,
                                   const MeshPoseTemporalSample &current) {
  // Identity comes from the retail display object and its stable authored pose record, before any
  // GTE state exists. The near/far precision branch is deliberately not identity: a native
  // composer owns one floating transform even if retail crosses its 1000-unit threshold.
  return previous.pose.valid && current.pose.valid && previous.pose.key == current.pose.key &&
         previous.simulationFrame < current.simulationFrame;
}

bool meshPoseContractSelftest() {
  MeshPoseInput nearInput;
  nearInput.key = {0x80123400u, 0x80124500u};
  nearInput.scale = AnimatedProjectionScale::NearPostScaled;
  nearInput.baseTransformWords = {
      0xFFFE0001u,
      0xFFFC0003u,
      0xFFFA0005u,
      0xFFF80007u,
      0xAAAA0009u,
      0x0000000Au,
      0xFFFFFFECu,
      0x0000001Eu,
  };
  nearInput.secondaryRotationWords = {
      0x00020001u,
      0x00040003u,
      0x00060005u,
      0x00080007u,
      0xBBBB0009u,
  };
  nearInput.sourcePoseWords = {
      0x0014000Au,
      0x0028001Eu,
      0x003C0032u,
      0x00500046u,
      0xFFEF005Au,
      0xFFDF001Fu,
  };

  const MeshPoseContract nearPose = decodeMeshPoseInput(nearInput);
  if (!nearPose.valid ||
      nearPose.baseRotation != std::array<int16_t, 9>{1, -2, 3, -4, 5, -6, 7, -8, 9} ||
      nearPose.baseTranslation != std::array<int32_t, 3>{10, -20, 30} ||
      nearPose.sourceRotation != std::array<int16_t, 9>{10, 20, 30, 40, 50, 60, 70, 80, 90} ||
      nearPose.sourceTranslation != std::array<int16_t, 3>{-17, 31, -33} ||
      nearPose.compositionTranslation != nearPose.sourceTranslation) {
    return false;
  }

  MeshPoseInput farInput = nearInput;
  farInput.scale = AnimatedProjectionScale::FarPreScaled;
  const MeshPoseContract farPose = decodeMeshPoseInput(farInput);
  if (farPose.compositionTranslation != std::array<int16_t, 3>{-2, 1, -3} ||
      farPose.signature == nearPose.signature) {
    return false;
  }

  const MeshPoseTemporalSample previous{nearPose, 41u};
  const MeshPoseTemporalSample current{farPose, 42u};
  if (!meshPoseSamplesCanInterpolate(previous, current)) {
    return false;
  }
  MeshPoseTemporalSample wrongOwner = current;
  ++wrongOwner.pose.key.owner;
  if (meshPoseSamplesCanInterpolate(previous, wrongOwner)) {
    return false;
  }
  MeshPoseTemporalSample sameFrame = current;
  sameFrame.simulationFrame = previous.simulationFrame;
  return !meshPoseSamplesCanInterpolate(previous, sameFrame);
}
