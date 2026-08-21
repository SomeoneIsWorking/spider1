// asset_upload_ledger.h — source-owner ledger for authored PSX texture uploads.
#ifndef SPIDER1_GAME_RENDER_ASSET_UPLOAD_LEDGER_H
#define SPIDER1_GAME_RENDER_ASSET_UPLOAD_LEDGER_H

#include <array>
#include <cstdint>

struct AssetVramRect {
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t width = 0;
  uint16_t height = 0;
};

struct AssetResidence {
  std::array<char, 64> name{};
  uint32_t slot = 0;
  uint32_t base = 0;
  uint32_t bytes = 0;
  uint64_t loadSequence = 0;
  uint64_t unloadSequence = 0;
  bool valid = false;
  bool live = false;
};

struct AssetUploadOwner {
  AssetVramRect destination;
  std::array<char, 64> assetName{};
  uint32_t slot = 0;
  uint32_t assetBase = 0;
  uint32_t assetBytes = 0;
  uint32_t source = 0;
  uint32_t frame = 0;
  uint64_t sequence = 0;
  bool valid = false;
};

class AssetUploadLedger {
public:
  void reset();
  void registerAsset(const char *name, uint32_t slot, uint32_t base, uint32_t bytes);
  void unregisterAsset(uint32_t slot);
  void recordUpload(uint32_t slot,
                    const char *assetName,
                    uint32_t assetBase,
                    uint32_t assetBytes,
                    const AssetVramRect &destination,
                    uint32_t source,
                    uint32_t frame);

  [[nodiscard]] AssetResidence residenceForAddress(uint32_t address) const;
  [[nodiscard]] AssetResidence residenceForSlot(uint32_t slot) const;
  [[nodiscard]] AssetUploadOwner latestCovering(const AssetVramRect &target) const;
  [[nodiscard]] uint64_t uploadCount() const;
  [[nodiscard]] uint64_t sequence() const;

private:
  static constexpr uint32_t kMaxAssets = 40;
  static constexpr uint32_t kMaxUploads = 512;

  std::array<AssetResidence, kMaxAssets> assets_{};
  std::array<AssetUploadOwner, kMaxUploads> uploads_{};
  uint32_t nextUpload_ = 0;
  uint64_t uploadCount_ = 0;
  uint64_t sequence_ = 0;
};

bool assetUploadLedgerSelftest();

#endif // SPIDER1_GAME_RENDER_ASSET_UPLOAD_LEDGER_H
