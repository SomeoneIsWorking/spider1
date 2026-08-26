// mesh_pose_contract.h — source-side contract at Spider-Man's animated pose composer.
#ifndef SPIDER1_GAME_RENDER_MESH_POSE_CONTRACT_H
#define SPIDER1_GAME_RENDER_MESH_POSE_CONTRACT_H

#include "mesh_animated_vertex.h"

#include <array>
#include <cstdint>

struct MeshPoseKey {
  uint32_t owner = 0;
  uint32_t sourcePose = 0;

  bool valid() const;
  bool operator==(const MeshPoseKey &) const = default;
};

struct MeshPoseInput {
  MeshPoseKey key;
  AnimatedProjectionScale scale = AnimatedProjectionScale::FarPreScaled;
  // FUN_8007FB1C/8007FD1C receive these exact three game-state records in a0/a1/a2.
  std::array<uint32_t, 8> baseTransformWords{};
  std::array<uint32_t, 5> secondaryRotationWords{};
  std::array<uint32_t, 6> sourcePoseWords{};
};

struct MeshPoseContract {
  MeshPoseKey key;
  std::array<int16_t, 9> baseRotation{};
  std::array<int32_t, 3> baseTranslation{};
  std::array<int16_t, 9> secondaryRotation{};
  std::array<int16_t, 9> sourceRotation{};
  std::array<int16_t, 3> sourceTranslation{};
  // The only semantic difference between retail's near/far composition entries: far shifts this
  // signed source translation by four before the final MVMVA; near submits it at full precision.
  std::array<int16_t, 3> compositionTranslation{};
  AnimatedProjectionScale scale = AnimatedProjectionScale::FarPreScaled;
  uint64_t signature = 0;
  bool valid = false;
};

struct MeshPoseTemporalSample {
  MeshPoseContract pose;
  uint64_t simulationFrame = 0;
};

MeshPoseContract decodeMeshPoseInput(const MeshPoseInput &input);
bool meshPoseSamplesCanInterpolate(const MeshPoseTemporalSample &previous,
                                   const MeshPoseTemporalSample &current);
bool meshPoseContractSelftest();

#endif // SPIDER1_GAME_RENDER_MESH_POSE_CONTRACT_H
