// mesh_asset_cook.cpp — copied pre/post records from the retail .psx mesh cook boundary.
#include "mesh_asset_cook.h"

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

bool validWords(const uint32_t *words, std::size_t wordCount) {
  return words != nullptr && wordCount != 0u && wordCount <= MeshAssetCookLedger::kMaxFaceWords;
}

bool exactRetailCook(const uint32_t *raw, const uint32_t *cooked, std::size_t wordCount) {
  if (!validWords(raw, wordCount) || !validWords(cooked, wordCount)) {
    return false;
  }
  if ((raw[0] >> 18u) != wordCount || (cooked[0] >> 18u) != wordCount) {
    return false;
  }
  const uint32_t expectedHeader = (raw[0] & 0x40u) == 0u ? raw[0] ^ 0x80u : raw[0];
  if (cooked[0] != expectedHeader) {
    return false;
  }
  if (wordCount > 2u) {
    const uint32_t expectedWord2 = (raw[0] & 0x800u) == 0u ? raw[2] & 0x00FFFFFFu : raw[2];
    if (cooked[2] != expectedWord2) {
      return false;
    }
  }
  if (wordCount > 3u) {
    const uint32_t expectedWord3 = (raw[3] & 0xFFFF0000u) | ((raw[3] & 0xFFFFu) << 3u & 0xFFFFu);
    if (cooked[3] != expectedWord3) {
      return false;
    }
  }
  return true;
}

} // namespace

void MeshAssetCookLedger::reset() {
  assets_ = {};
  faces_ = {};
  sequence_ = 0;
  refusalCount_ = 0;
}

void MeshAssetCookLedger::beginAsset(const char *name, uint32_t slot) {
  if (slot >= assets_.size()) {
    ++refusalCount_;
    return;
  }
  AssetState &asset = assets_[slot];
  asset = {};
  copyName(asset.name, name);
  asset.loadSequence = ++sequence_;
  asset.live = true;
}

void MeshAssetCookLedger::unregisterAsset(uint32_t slot) {
  if (slot >= assets_.size() || !assets_[slot].live) {
    ++refusalCount_;
    return;
  }
  assets_[slot].live = false;
  ++sequence_;
}

bool MeshAssetCookLedger::recordRawFace(uint32_t slot,
                                        uint32_t meshIndex,
                                        uint32_t mesh,
                                        uint32_t face,
                                        const uint32_t *words,
                                        std::size_t wordCount) {
  if (slot >= assets_.size() || !assets_[slot].live || mesh == 0u || face == 0u ||
      !validWords(words, wordCount)) {
    ++refusalCount_;
    return false;
  }
  FaceState *destination = nullptr;
  for (FaceState &candidate : faces_) {
    if (!candidate.valid) {
      destination = &candidate;
      break;
    }
  }
  if (destination == nullptr) {
    ++refusalCount_;
    return false;
  }
  *destination = {};
  std::copy_n(words, wordCount, destination->raw.begin());
  destination->slot = slot;
  destination->meshIndex = meshIndex;
  destination->mesh = mesh;
  destination->face = face;
  destination->wordCount = static_cast<uint32_t>(wordCount);
  destination->loadSequence = assets_[slot].loadSequence;
  destination->valid = true;
  return true;
}

bool MeshAssetCookLedger::recordCookedFace(uint32_t slot,
                                           uint32_t meshIndex,
                                           uint32_t mesh,
                                           uint32_t face,
                                           const uint32_t *words,
                                           std::size_t wordCount) {
  if (slot >= assets_.size() || !assets_[slot].live || !validWords(words, wordCount)) {
    ++refusalCount_;
    return false;
  }
  for (FaceState &candidate : faces_) {
    if (candidate.valid && candidate.slot == slot && candidate.meshIndex == meshIndex &&
        candidate.loadSequence == assets_[slot].loadSequence && candidate.mesh == mesh &&
        candidate.face == face && candidate.wordCount == wordCount) {
      std::copy_n(words, wordCount, candidate.cooked.begin());
      candidate.cookedValid = true;
      candidate.structuralCookExact = exactRetailCook(candidate.raw.data(), words, wordCount);
      return true;
    }
  }
  ++refusalCount_;
  return false;
}

MeshAssetCookResolution
MeshAssetCookLedger::resolve(uint32_t face, const uint32_t *words, std::size_t wordCount) const {
  const FaceState *latest = nullptr;
  for (const FaceState &candidate : faces_) {
    if (candidate.valid && candidate.cookedValid && candidate.face == face &&
        (latest == nullptr || candidate.loadSequence > latest->loadSequence)) {
      latest = &candidate;
    }
  }
  if (latest == nullptr) {
    return {};
  }
  MeshAssetCookResolution result;
  result.assetName = assets_[latest->slot].name;
  result.slot = latest->slot;
  result.mesh = latest->mesh;
  result.face = latest->face;
  result.recordBytes = latest->wordCount * sizeof(uint32_t);
  result.loadSequence = latest->loadSequence;
  result.structuralCookExact = latest->structuralCookExact;
  if (!assets_[latest->slot].live || assets_[latest->slot].loadSequence != latest->loadSequence) {
    result.match = MeshAssetCookMatch::Unloaded;
  } else if (!validWords(words, wordCount) || wordCount != latest->wordCount) {
    result.match = MeshAssetCookMatch::Mismatch;
  } else {
    result.match = std::equal(words, words + wordCount, latest->cooked.begin())
                       ? MeshAssetCookMatch::Match
                       : MeshAssetCookMatch::Mismatch;
  }
  return result;
}

uint32_t MeshAssetCookLedger::cookedFaceCount(uint32_t slot) const {
  uint32_t count = 0;
  for (const FaceState &face : faces_) {
    count += face.valid && face.cookedValid && face.slot == slot && slot < assets_.size() &&
                     face.loadSequence == assets_[slot].loadSequence
                 ? 1u
                 : 0u;
  }
  return count;
}

uint32_t MeshAssetCookLedger::structuralExactCount(uint32_t slot) const {
  uint32_t count = 0;
  for (const FaceState &face : faces_) {
    count += face.valid && face.cookedValid && face.structuralCookExact && face.slot == slot &&
                     slot < assets_.size() && face.loadSequence == assets_[slot].loadSequence
                 ? 1u
                 : 0u;
  }
  return count;
}

uint64_t MeshAssetCookLedger::refusalCount() const {
  return refusalCount_;
}

void MeshAssetCookLedger::refuseCapture() {
  ++refusalCount_;
}

bool meshAssetCookLedgerSelftest() {
  constexpr std::array<uint32_t, 7> raw{
      0x001C1003u, 0x02030001u, 0x2C000000u, 0u, 0u, 0x07000707u, 0x00000007u};
  constexpr std::array<uint32_t, 7> cooked{
      0x001C1083u, 0x02030001u, 0u, 0u, 0x00E2FD3Bu, 0x0008FD34u, 0xF634F63Bu};
  MeshAssetCookLedger ledger;
  ledger.beginAsset("dem1_g.psx", 3u);
  const bool rawAccepted =
      ledger.recordRawFace(3u, 0u, 0x8018BC38u, 0x8018BC7Cu, raw.data(), raw.size());
  std::array<uint32_t, 7> recorded = cooked;
  const bool cookedAccepted =
      ledger.recordCookedFace(3u, 0u, 0x8018BC38u, 0x8018BC7Cu, recorded.data(), recorded.size());
  recorded[4] ^= 1u;
  const MeshAssetCookResolution exact = ledger.resolve(0x8018BC7Cu, cooked.data(), cooked.size());
  const MeshAssetCookResolution opposite =
      ledger.resolve(0x8018BC7Cu, recorded.data(), recorded.size());
  const MeshAssetCookResolution missing = ledger.resolve(0x8018BC80u, cooked.data(), cooked.size());
  ledger.unregisterAsset(3u);
  const MeshAssetCookResolution unloaded =
      ledger.resolve(0x8018BC7Cu, cooked.data(), cooked.size());

  std::array<uint32_t, 7> preservedRaw = raw;
  preservedRaw[0] |= 0x800u;
  preservedRaw[2] = 0xAB000001u;
  std::array<uint32_t, 7> preservedCooked = cooked;
  preservedCooked[0] = preservedRaw[0] ^ 0x80u;
  preservedCooked[2] = preservedRaw[2];
  MeshAssetCookLedger preservedLedger;
  preservedLedger.beginAsset("preserved.psx", 4u);
  const bool preservedAccepted = preservedLedger.recordRawFace(
      4u, 0u, 0x80190000u, 0x80190044u, preservedRaw.data(), preservedRaw.size());
  const bool preservedCookedAccepted = preservedLedger.recordCookedFace(
      4u, 0u, 0x80190000u, 0x80190044u, preservedCooked.data(), preservedCooked.size());
  const MeshAssetCookResolution preserved =
      preservedLedger.resolve(0x80190044u, preservedCooked.data(), preservedCooked.size());

  return rawAccepted && cookedAccepted && exact.match == MeshAssetCookMatch::Match &&
         exact.structuralCookExact && opposite.match == MeshAssetCookMatch::Mismatch &&
         missing.match == MeshAssetCookMatch::Missing &&
         unloaded.match == MeshAssetCookMatch::Unloaded && ledger.cookedFaceCount(3u) == 1u &&
         ledger.structuralExactCount(3u) == 1u && ledger.refusalCount() == 0u &&
         preservedAccepted && preservedCookedAccepted &&
         preserved.match == MeshAssetCookMatch::Match && preserved.structuralCookExact;
}
