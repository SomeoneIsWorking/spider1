// mesh_transform.cpp — executable-derived direct object-local-to-camera transform contract.
#include "mesh_transform.h"

#include <cstring>

namespace {

// The two jal FUN_80077D64 instructions at 80076E70/80076E80 resume at these addresses. Both are
// reached only after FUN_80076480 has rejected non-zero object rotation and flag bit 0x0200.
constexpr uint32_t kDirectMeshReturnA = 0x80076E78u;
constexpr uint32_t kDirectMeshReturnB = 0x80076E88u;
constexpr uint16_t kScaledObject = 0x0200u;

int32_t mipsSra12(int32_t value) {
  uint32_t encoded = 0;
  std::memcpy(&encoded, &value, sizeof(encoded));
  uint32_t shifted = encoded >> 12u;
  if (value < 0) {
    shifted |= 0xFFF00000u;
  }
  int32_t result = 0;
  std::memcpy(&result, &shifted, sizeof(result));
  return result;
}

int16_t lowHalf(uint32_t word) {
  const uint16_t encoded = static_cast<uint16_t>(word);
  int16_t result = 0;
  std::memcpy(&result, &encoded, sizeof(result));
  return result;
}

int16_t highHalf(uint32_t word) {
  const uint16_t encoded = static_cast<uint16_t>(word >> 16u);
  int16_t result = 0;
  std::memcpy(&result, &encoded, sizeof(result));
  return result;
}

} // namespace

MeshDirectTransformContract inspectMeshDirectTransform(const MeshDirectTransformInput &input) {
  MeshDirectTransformContract contract;
  for (uint32_t axis = 0; axis < contract.expectedRelative.size(); ++axis) {
    contract.expectedRelative[axis] =
        mipsSra12(input.objectPosition20p12[axis]) - input.cameraPosition[axis];
  }
  contract.cameraRotation = {
      lowHalf(input.cameraRotationWords[0]),
      highHalf(input.cameraRotationWords[0]),
      lowHalf(input.cameraRotationWords[1]),
      highHalf(input.cameraRotationWords[1]),
      lowHalf(input.cameraRotationWords[2]),
      highHalf(input.cameraRotationWords[2]),
      lowHalf(input.cameraRotationWords[3]),
      highHalf(input.cameraRotationWords[3]),
      lowHalf(input.cameraRotationWords[4]),
  };
  contract.sourcesPresent = input.ownerPresent && input.cameraPresent && input.relativePresent;
  contract.knownCallsite =
      input.returnAddress == kDirectMeshReturnA || input.returnAddress == kDirectMeshReturnB;
  contract.rotationIsIdentity =
      input.objectRotation[0] == 0 && input.objectRotation[1] == 0 && input.objectRotation[2] == 0;
  contract.scaleIsIdentity = (input.objectFlags & kScaledObject) == 0u;
  contract.relativeMatches = input.passedRelative == contract.expectedRelative;
  contract.directPathMatches = contract.sourcesPresent && contract.knownCallsite &&
                               contract.rotationIsIdentity && contract.scaleIsIdentity &&
                               contract.relativeMatches;
  return contract;
}

bool meshDirectTransformSelftest() {
  MeshDirectTransformInput baseline;
  baseline.ownerPresent = true;
  baseline.cameraPresent = true;
  baseline.relativePresent = true;
  baseline.returnAddress = kDirectMeshReturnA;
  baseline.objectFlags = 0x1000u;
  baseline.objectPosition20p12 = {0x00123000, -4097, -4096};
  baseline.cameraPosition = {100, -5, 7};
  baseline.cameraRotationWords = {0x00020001u, 0x00040003u, 0x00060005u, 0x00080007u, 0x00000009u};
  baseline.passedRelative = {191, 3, -8};

  const MeshDirectTransformContract accepted = inspectMeshDirectTransform(baseline);
  if (!accepted.directPathMatches || accepted.expectedRelative != baseline.passedRelative ||
      accepted.cameraRotation != std::array<int16_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9}) {
    return false;
  }

  MeshDirectTransformInput perturbed = baseline;
  ++perturbed.passedRelative[2];
  if (inspectMeshDirectTransform(perturbed).directPathMatches) {
    return false;
  }
  perturbed = baseline;
  perturbed.objectRotation[1] = 1;
  if (inspectMeshDirectTransform(perturbed).directPathMatches) {
    return false;
  }
  perturbed = baseline;
  perturbed.objectFlags |= kScaledObject;
  if (inspectMeshDirectTransform(perturbed).directPathMatches) {
    return false;
  }
  perturbed = baseline;
  perturbed.returnAddress += 4u;
  return !inspectMeshDirectTransform(perturbed).directPathMatches;
}
