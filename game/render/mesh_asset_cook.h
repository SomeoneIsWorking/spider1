// mesh_asset_cook.h — retained ownership of retail-cooked mesh face records.
#ifndef SPIDER1_GAME_RENDER_MESH_ASSET_COOK_H
#define SPIDER1_GAME_RENDER_MESH_ASSET_COOK_H

#include <array>
#include <cstddef>
#include <cstdint>

enum class MeshAssetCookMatch {
  Missing,
  Match,
  Mismatch,
  Unloaded,
};

struct MeshAssetCookResolution {
  std::array<char, 64> assetName{};
  uint32_t slot = 0;
  uint32_t mesh = 0;
  uint32_t face = 0;
  uint32_t recordBytes = 0;
  uint64_t loadSequence = 0;
  MeshAssetCookMatch match = MeshAssetCookMatch::Missing;
  bool structuralCookExact = false;
};

class MeshAssetCookLedger {
public:
  static constexpr std::size_t kMaxFaceWords = 8;
  static constexpr std::size_t kMaxFaces = 512;

  void reset();
  void beginAsset(const char *name, uint32_t slot);
  void unregisterAsset(uint32_t slot);
  bool recordRawFace(uint32_t slot,
                     uint32_t meshIndex,
                     uint32_t mesh,
                     uint32_t face,
                     const uint32_t *words,
                     std::size_t wordCount);
  bool recordCookedFace(uint32_t slot,
                        uint32_t meshIndex,
                        uint32_t mesh,
                        uint32_t face,
                        const uint32_t *words,
                        std::size_t wordCount);

  [[nodiscard]] MeshAssetCookResolution
  resolve(uint32_t face, const uint32_t *words, std::size_t wordCount) const;
  [[nodiscard]] uint32_t cookedFaceCount(uint32_t slot) const;
  [[nodiscard]] uint32_t structuralExactCount(uint32_t slot) const;
  [[nodiscard]] uint64_t refusalCount() const;
  void refuseCapture();

private:
  static constexpr std::size_t kMaxAssets = 40;
  struct AssetState {
    std::array<char, 64> name{};
    uint64_t loadSequence = 0;
    bool live = false;
  };

  struct FaceState {
    std::array<uint32_t, kMaxFaceWords> raw{};
    std::array<uint32_t, kMaxFaceWords> cooked{};
    uint32_t slot = 0;
    uint32_t meshIndex = 0;
    uint32_t mesh = 0;
    uint32_t face = 0;
    uint32_t wordCount = 0;
    uint64_t loadSequence = 0;
    bool valid = false;
    bool cookedValid = false;
    bool structuralCookExact = false;
  };

  std::array<AssetState, kMaxAssets> assets_{};
  std::array<FaceState, kMaxFaces> faces_{};
  uint64_t sequence_ = 0;
  uint64_t refusalCount_ = 0;
};

bool meshAssetCookLedgerSelftest();

#endif // SPIDER1_GAME_RENDER_MESH_ASSET_COOK_H
