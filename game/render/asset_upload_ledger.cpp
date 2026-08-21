// asset_upload_ledger.cpp — bounded provenance and lifetime state for authored PSX assets.
#include "asset_upload_ledger.h"

#include <algorithm>
#include <cstring>

namespace {

void copyName(std::array<char, 64> &destination, const char *source) {
  destination.fill('\0');
  if (source == nullptr) {
    return;
  }
  const std::size_t count = std::min(destination.size() - 1u, std::strlen(source));
  std::memcpy(destination.data(), source, count);
}

bool contains(const AssetVramRect &outer, const AssetVramRect &inner) {
  const uint32_t outerRight = static_cast<uint32_t>(outer.x) + outer.width;
  const uint32_t outerBottom = static_cast<uint32_t>(outer.y) + outer.height;
  const uint32_t innerRight = static_cast<uint32_t>(inner.x) + inner.width;
  const uint32_t innerBottom = static_cast<uint32_t>(inner.y) + inner.height;
  return inner.width != 0u && inner.height != 0u && outer.width != 0u && outer.height != 0u &&
         inner.x >= outer.x && inner.y >= outer.y && innerRight <= outerRight &&
         innerBottom <= outerBottom;
}

bool addressInside(const AssetResidence &asset, uint32_t address) {
  const uint64_t end = static_cast<uint64_t>(asset.base) + asset.bytes;
  return asset.valid && asset.live && address >= asset.base && address < end;
}

} // namespace

void AssetUploadLedger::reset() {
  assets_ = {};
  uploads_ = {};
  nextUpload_ = 0;
  uploadCount_ = 0;
  sequence_ = 0;
}

void AssetUploadLedger::registerAsset(const char *name,
                                      uint32_t slot,
                                      uint32_t base,
                                      uint32_t bytes) {
  if (slot >= assets_.size()) {
    return;
  }
  AssetResidence &asset = assets_[slot];
  asset = {};
  copyName(asset.name, name);
  asset.slot = slot;
  asset.base = base;
  asset.bytes = bytes;
  asset.loadSequence = ++sequence_;
  asset.valid = base != 0u && bytes != 0u;
  asset.live = asset.valid;
}

void AssetUploadLedger::unregisterAsset(uint32_t slot) {
  if (slot >= assets_.size() || !assets_[slot].valid || !assets_[slot].live) {
    return;
  }
  assets_[slot].live = false;
  assets_[slot].unloadSequence = ++sequence_;
}

void AssetUploadLedger::recordUpload(uint32_t slot,
                                     const char *assetName,
                                     uint32_t assetBase,
                                     uint32_t assetBytes,
                                     const AssetVramRect &destination,
                                     uint32_t source,
                                     uint32_t frame) {
  AssetUploadOwner &upload = uploads_[nextUpload_];
  upload = {};
  upload.destination = destination;
  copyName(upload.assetName, assetName);
  upload.slot = slot;
  upload.assetBase = assetBase;
  upload.assetBytes = assetBytes;
  upload.source = source;
  upload.frame = frame;
  upload.sequence = ++sequence_;
  upload.valid = destination.width != 0u && destination.height != 0u;
  nextUpload_ = (nextUpload_ + 1u) % uploads_.size();
  ++uploadCount_;
}

AssetResidence AssetUploadLedger::residenceForAddress(uint32_t address) const {
  AssetResidence result;
  for (const AssetResidence &asset : assets_) {
    if (addressInside(asset, address) &&
        (!result.valid || asset.loadSequence > result.loadSequence)) {
      result = asset;
    }
  }
  return result;
}

AssetResidence AssetUploadLedger::residenceForSlot(uint32_t slot) const {
  return slot < assets_.size() ? assets_[slot] : AssetResidence{};
}

AssetUploadOwner AssetUploadLedger::latestCovering(const AssetVramRect &target) const {
  AssetUploadOwner result;
  for (const AssetUploadOwner &upload : uploads_) {
    if (upload.valid && contains(upload.destination, target) &&
        (!result.valid || upload.sequence > result.sequence)) {
      result = upload;
    }
  }
  return result;
}

uint64_t AssetUploadLedger::uploadCount() const {
  return uploadCount_;
}

uint64_t AssetUploadLedger::sequence() const {
  return sequence_;
}

bool assetUploadLedgerSelftest() {
  AssetUploadLedger ledger;
  ledger.registerAsset("old.psx", 1u, 0x80100000u, 0x100u);
  ledger.recordUpload(1u, "old.psx", 0x80100000u, 0x100u, {525u, 246u, 2u, 8u}, 0x80100040u, 10u);
  ledger.registerAsset("dem1_l.psx", 2u, 0x801539D4u, 0x2B84u);
  ledger.recordUpload(
      2u, "dem1_l.psx", 0x801539D4u, 0x2B84u, {525u, 246u, 2u, 8u}, 0x80156470u, 20u);
  ledger.registerAsset("dem1_g.psx", 3u, 0x8018BB84u, 0x1EA8u);

  const AssetUploadOwner exact = ledger.latestCovering({525u, 246u, 2u, 8u});
  const AssetUploadOwner perturbed = ledger.latestCovering({527u, 246u, 1u, 8u});
  const AssetResidence geometry = ledger.residenceForAddress(0x8018BC38u);
  ledger.unregisterAsset(2u);
  const AssetResidence unloaded = ledger.residenceForSlot(2u);

  return exact.valid && std::strcmp(exact.assetName.data(), "dem1_l.psx") == 0 &&
         exact.slot == 2u && exact.source == 0x80156470u && !perturbed.valid && geometry.valid &&
         geometry.live && std::strcmp(geometry.name.data(), "dem1_g.psx") == 0 &&
         geometry.base == 0x8018BB84u && unloaded.valid && !unloaded.live &&
         unloaded.unloadSequence > unloaded.loadSequence && ledger.uploadCount() == 2u;
}
