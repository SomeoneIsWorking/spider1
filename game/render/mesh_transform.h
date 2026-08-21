// mesh_transform.h — source-side contract for Spider-Man's direct mesh transform path.
#ifndef SPIDER1_GAME_RENDER_MESH_TRANSFORM_H
#define SPIDER1_GAME_RENDER_MESH_TRANSFORM_H

#include <array>
#include <cstdint>

struct MeshDirectTransformInput {
  bool ownerPresent = false;
  bool cameraPresent = false;
  bool relativePresent = false;
  uint32_t returnAddress = 0;
  uint16_t objectFlags = 0;
  std::array<int32_t, 3> objectPosition20p12{};
  std::array<int16_t, 3> objectRotation{};
  std::array<int32_t, 3> cameraPosition{};
  std::array<uint32_t, 5> cameraRotationWords{};
  std::array<int32_t, 3> passedRelative{};
};

struct MeshDirectTransformContract {
  std::array<int32_t, 3> expectedRelative{};
  std::array<int16_t, 9> cameraRotation{};
  bool sourcesPresent = false;
  bool knownCallsite = false;
  bool rotationIsIdentity = false;
  bool scaleIsIdentity = false;
  bool relativeMatches = false;
  bool directPathMatches = false;
};

MeshDirectTransformContract inspectMeshDirectTransform(const MeshDirectTransformInput &input);
bool meshDirectTransformSelftest();

#endif // SPIDER1_GAME_RENDER_MESH_TRANSFORM_H
