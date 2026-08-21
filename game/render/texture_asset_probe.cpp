// texture_asset_probe.cpp — observe the retail .psx loader, VRAM uploads, and unload boundary.
#include "texture_asset_probe.h"

#include "asset_upload_ledger.h"
#include "core.h"
#include "game.h"
#include "mesh_asset_cook.h"
#include "mesh_face_format.h"
#include "override_registry.h"

#include <lucent/log.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

extern void gen_func_80069A60(Core *);
extern void gen_func_80068BB0(Core *);
extern void gen_func_800695D0(Core *);
extern void gen_func_80081C50(Core *);
int gpu_frame_no(Core *core);

namespace {

constexpr uint32_t kLoadPsxAsset = 0x80069A60u;
constexpr uint32_t kParsePsxAsset = 0x80068BB0u;
constexpr uint32_t kUnloadPsxAsset = 0x800695D0u;
constexpr uint32_t kLoadImage = 0x80081C50u;
constexpr uint32_t kAssetRegistry = 0x800A0904u;
constexpr uint32_t kAssetRecordBytes = 0x40u;
constexpr uint32_t kAssetBaseOffset = 0x14u;
constexpr uint32_t kMaxLoaderDepth = 8u;

struct ActiveAsset {
  std::array<char, 64> name{};
  uint32_t slot = 0;
  uint32_t base = 0;
  uint32_t bytes = 0;
  bool valid = false;
};

AssetUploadLedger g_ledger;
MeshAssetCookLedger g_cookLedger;
std::array<ActiveAsset, kMaxLoaderDepth> g_loaderStack{};
uint32_t g_loaderDepth = 0;
bool g_installed = false;

struct CookCaptureSummary {
  uint32_t meshCount = 0;
  uint32_t faceCount = 0;
  uint32_t refusedCount = 0;
};

bool rangeInside(uint32_t base, uint32_t bytes, uint32_t address, uint32_t length) {
  const uint64_t end = static_cast<uint64_t>(base) + bytes;
  return bytes != 0u && address >= base && static_cast<uint64_t>(address) + length <= end;
}

bool readFaceWords(Core *core,
                   uint32_t base,
                   uint32_t bytes,
                   uint32_t face,
                   std::array<uint32_t, MeshAssetCookLedger::kMaxFaceWords> &words,
                   std::size_t &wordCount) {
  if (!rangeInside(base, bytes, face, sizeof(uint32_t))) {
    return false;
  }
  const uint32_t header = core->mem_r32(face);
  const uint32_t recordBytes = (header >> 18u) * sizeof(uint32_t);
  if (recordBytes == 0u || recordBytes % sizeof(uint32_t) != 0u ||
      recordBytes > words.size() * sizeof(uint32_t) ||
      !rangeInside(base, bytes, face, recordBytes)) {
    return false;
  }
  wordCount = recordBytes / sizeof(uint32_t);
  for (std::size_t word = 0; word < wordCount; ++word) {
    words[word] = core->mem_r32(face + static_cast<uint32_t>(word * sizeof(uint32_t)));
  }
  return true;
}

CookCaptureSummary
captureFirstFaces(Core *core, uint32_t slot, uint32_t base, uint32_t bytes, bool relocated) {
  CookCaptureSummary summary;
  if (!rangeInside(base, bytes, base + 8u, sizeof(uint32_t))) {
    g_cookLedger.refuseCapture();
    summary.refusedCount = 1u;
    return summary;
  }
  const uint32_t sectionCount = core->mem_r32(base + 8u);
  const uint64_t table64 =
      static_cast<uint64_t>(base) + static_cast<uint64_t>(sectionCount) * 0x24u + 0x10u;
  if (table64 > UINT32_MAX) {
    g_cookLedger.refuseCapture();
    summary.refusedCount = 1u;
    return summary;
  }
  const uint32_t table = static_cast<uint32_t>(table64);
  if (table < base + sizeof(uint32_t) ||
      !rangeInside(base, bytes, table - sizeof(uint32_t), sizeof(uint32_t))) {
    g_cookLedger.refuseCapture();
    summary.refusedCount = 1u;
    return summary;
  }
  summary.meshCount = core->mem_r32(table - sizeof(uint32_t));
  if (summary.meshCount > MeshAssetCookLedger::kMaxFaces ||
      !rangeInside(base, bytes, table, summary.meshCount * sizeof(uint32_t))) {
    g_cookLedger.refuseCapture();
    summary.refusedCount = 1u;
    return summary;
  }

  for (uint32_t meshIndex = 0; meshIndex < summary.meshCount; ++meshIndex) {
    const uint32_t entry = core->mem_r32(table + meshIndex * sizeof(uint32_t));
    const uint64_t mesh64 = relocated ? entry : static_cast<uint64_t>(base) + entry;
    if (mesh64 > UINT32_MAX ||
        !rangeInside(base, bytes, static_cast<uint32_t>(mesh64), kSpiderMeshHeaderBytes)) {
      g_cookLedger.refuseCapture();
      ++summary.refusedCount;
      continue;
    }
    const uint32_t mesh = static_cast<uint32_t>(mesh64);
    const uint16_t vertexCount = core->mem_r16(mesh + kSpiderMeshVertexCountOffset);
    const uint16_t secondaryCount = core->mem_r16(mesh + kSpiderMeshSecondaryCountOffset);
    const uint16_t faceCount = core->mem_r16(mesh + kSpiderMeshFaceCountOffset);
    if (faceCount == 0u) {
      continue;
    }
    const MeshLayout layout = deriveMeshLayout(mesh, {vertexCount, secondaryCount, faceCount});
    const uint64_t face64 = layout.faces;
    if (face64 > UINT32_MAX) {
      g_cookLedger.refuseCapture();
      ++summary.refusedCount;
      continue;
    }
    const uint32_t face = static_cast<uint32_t>(face64);
    std::array<uint32_t, MeshAssetCookLedger::kMaxFaceWords> words{};
    std::size_t wordCount = 0;
    if (!readFaceWords(core, base, bytes, face, words, wordCount)) {
      g_cookLedger.refuseCapture();
      ++summary.refusedCount;
      continue;
    }
    const bool recorded =
        relocated
            ? g_cookLedger.recordCookedFace(slot, meshIndex, mesh, face, words.data(), wordCount)
            : g_cookLedger.recordRawFace(slot, meshIndex, mesh, face, words.data(), wordCount);
    if (recorded) {
      ++summary.faceCount;
    } else {
      ++summary.refusedCount;
    }
  }
  return summary;
}

const char *cookMatchName(MeshAssetCookMatch match) {
  switch (match) {
  case MeshAssetCookMatch::Missing:
    return "MISSING";
  case MeshAssetCookMatch::Match:
    return "MATCH";
  case MeshAssetCookMatch::Mismatch:
    return "MISMATCH";
  case MeshAssetCookMatch::Unloaded:
    return "UNLOADED";
  }
  return "UNKNOWN";
}

std::array<char, 64> readAssetName(Core *core, uint32_t address) {
  std::array<char, 64> result{};
  for (uint32_t i = 0; i + 1u < result.size(); ++i) {
    const uint8_t byte = core->mem_r8(address + i);
    if (byte == 0u) {
      break;
    }
    result[i] = byte >= 0x20u && byte <= 0x7Eu ? static_cast<char>(byte) : '?';
  }
  const std::size_t length = std::strlen(result.data());
  constexpr char suffix[] = ".psx";
  if (length + sizeof(suffix) <= result.size()) {
    std::memcpy(result.data() + length, suffix, sizeof(suffix));
  }
  return result;
}

uint32_t allocationBytes(Core *core, uint32_t base) {
  // FUN_80065584 and FUN_800654E8 both decode the allocator header as word>>4. FUN_80068BB0
  // shrinks the raw file allocation after it has uploaded textures, so reading this both before
  // and after the parser distinguishes transient file bytes from the retained live asset.
  return base >= 4u ? core->mem_r32(base - 4u) >> 4u : 0u;
}

ActiveAsset *activeAsset() {
  if (g_loaderDepth == 0u || !g_loaderStack[g_loaderDepth - 1u].valid) {
    return nullptr;
  }
  return &g_loaderStack[g_loaderDepth - 1u];
}

void loadPsxAsset(Core *core) {
  const std::array<char, 64> name = readAssetName(core, core->r[4]);
  if (g_loaderDepth >= g_loaderStack.size()) {
    lucent::error(
        "assetprobe", "loader nesting exceeded {} entries; refusing attribution", kMaxLoaderDepth);
    gen_func_80069A60(core);
    return;
  }
  ActiveAsset &asset = g_loaderStack[g_loaderDepth++];
  asset = {};
  asset.name = name;
  asset.valid = true;
  gen_func_80069A60(core);
  --g_loaderDepth;
}

void parsePsxAsset(Core *core) {
  ActiveAsset *asset = activeAsset();
  const uint32_t slot = core->r[4];
  const uint32_t record = kAssetRegistry + slot * kAssetRecordBytes;
  const uint32_t base = core->mem_r32(record + kAssetBaseOffset);
  const uint32_t bytes = allocationBytes(core, base);
  if (asset != nullptr) {
    asset->slot = slot;
    asset->base = base;
    asset->bytes = bytes;
    g_ledger.registerAsset(asset->name.data(), slot, base, bytes);
    g_cookLedger.beginAsset(asset->name.data(), slot);
  }
  const CookCaptureSummary raw =
      asset != nullptr ? captureFirstFaces(core, slot, base, bytes, false) : CookCaptureSummary{};
  gen_func_80068BB0(core);
  if (asset != nullptr) {
    const uint32_t retainedBytes = allocationBytes(core, base);
    g_ledger.registerAsset(asset->name.data(), slot, base, retainedBytes);
    const CookCaptureSummary cooked = captureFirstFaces(core, slot, base, retainedBytes, true);
    lucent::info("assetprobe",
                 "LOAD asset={} slot={} base={:08X} rawAllocationBytes={} retainedBytes={} "
                 "sequence={} meshCook(raw={}/{}, cooked={}/{}, structuralExact={}, refused={})",
                 asset->name.data(),
                 slot,
                 base,
                 bytes,
                 retainedBytes,
                 g_ledger.sequence(),
                 raw.faceCount,
                 raw.meshCount,
                 cooked.faceCount,
                 cooked.meshCount,
                 g_cookLedger.structuralExactCount(slot),
                 raw.refusedCount + cooked.refusedCount);
  }
}

void loadImage(Core *core) {
  ActiveAsset *asset = activeAsset();
  if (asset != nullptr && asset->base != 0u) {
    const uint32_t rectangle = core->r[4];
    const AssetVramRect destination{core->mem_r16(rectangle),
                                    core->mem_r16(rectangle + 2u),
                                    core->mem_r16(rectangle + 4u),
                                    core->mem_r16(rectangle + 6u)};
    g_ledger.recordUpload(asset->slot,
                          asset->name.data(),
                          asset->base,
                          asset->bytes,
                          destination,
                          core->r[5],
                          static_cast<uint32_t>(gpu_frame_no(core)));
  }
  gen_func_80081C50(core);
}

void unloadPsxAsset(Core *core) {
  const uint32_t slot = core->r[4];
  const AssetResidence residence = g_ledger.residenceForSlot(slot);
  gen_func_800695D0(core);
  g_ledger.unregisterAsset(slot);
  g_cookLedger.unregisterAsset(slot);
  if (residence.valid && residence.live) {
    lucent::info("assetprobe",
                 "UNLOAD asset={} slot={} base={:08X} bytes={} sequence={}",
                 residence.name.data(),
                 slot,
                 residence.base,
                 residence.bytes,
                 g_ledger.sequence());
  }
}

AssetVramRect textureRectangle(const MeshFt4TextureBinding &binding) {
  uint8_t minimumU = binding.uv[0].u;
  uint8_t maximumU = binding.uv[0].u;
  uint8_t minimumV = binding.uv[0].v;
  uint8_t maximumV = binding.uv[0].v;
  for (const MeshUv &uv : binding.uv) {
    minimumU = std::min(minimumU, uv.u);
    maximumU = std::max(maximumU, uv.u);
    minimumV = std::min(minimumV, uv.v);
    maximumV = std::max(maximumV, uv.v);
  }
  uint32_t pixelsPerHalfword = 1u;
  if (binding.bitsPerPixel == 4u) {
    pixelsPerHalfword = 4u;
  } else if (binding.bitsPerPixel == 8u) {
    pixelsPerHalfword = 2u;
  }
  const uint32_t x0 = binding.texturePageX + minimumU / pixelsPerHalfword;
  const uint32_t x1 = binding.texturePageX + maximumU / pixelsPerHalfword;
  return {static_cast<uint16_t>(x0),
          static_cast<uint16_t>(binding.texturePageY + minimumV),
          static_cast<uint16_t>(x1 - x0 + 1u),
          static_cast<uint16_t>(maximumV - minimumV + 1u)};
}

void reportOwner(const char *kind, const AssetVramRect &target) {
  const AssetUploadOwner owner = g_ledger.latestCovering(target);
  if (!owner.valid) {
    lucent::error("assetprobe",
                  "BINDING {} target=({},{} {}x{}) owner=MISSING across {} upload(s)",
                  kind,
                  target.x,
                  target.y,
                  target.width,
                  target.height,
                  g_ledger.uploadCount());
    return;
  }
  const AssetResidence residence = g_ledger.residenceForSlot(owner.slot);
  const uint64_t retainedEnd = static_cast<uint64_t>(residence.base) + residence.bytes;
  const bool sourceBytesLive = residence.valid && residence.live &&
                               owner.source >= residence.base &&
                               static_cast<uint64_t>(owner.source) < retainedEnd;
  const uint32_t sourceOffset =
      owner.source >= owner.assetBase ? owner.source - owner.assetBase : UINT32_MAX;
  lucent::info("assetprobe",
               "BINDING {} target=({},{} {}x{}) owner={} slot={} base={:08X} "
               "rawAllocationBytes={} retainedBytes={} source={:08X} sourceOffset=0x{:X} "
               "upload=({},{} {}x{}) uploadFrame={} uploadSequence={} assetSlotLive={} "
               "sourceBytesLive={}",
               kind,
               target.x,
               target.y,
               target.width,
               target.height,
               owner.assetName.data(),
               owner.slot,
               owner.assetBase,
               owner.assetBytes,
               residence.bytes,
               owner.source,
               sourceOffset,
               owner.destination.x,
               owner.destination.y,
               owner.destination.width,
               owner.destination.height,
               owner.frame,
               owner.sequence,
               residence.valid && residence.live ? "yes" : "no",
               sourceBytesLive ? "yes" : "no");
}

} // namespace

void spiderman_install_texture_asset_probe(Game *) {
  static const lucent::Channel channel{"assetprobe"};
  if (!channel) {
    return;
  }
  g_ledger.reset();
  g_cookLedger.reset();
  if (!assetUploadLedgerSelftest()) {
    lucent::error("assetprobe", "SELFTEST FAILED; wrappers NOT installed");
    return;
  }
  if (!meshAssetCookLedgerSelftest()) {
    lucent::error("assetprobe", "MESH COOK SELFTEST FAILED; wrappers NOT installed");
    return;
  }
  g_installed = true;
  engine_set_override_main(kLoadPsxAsset, loadPsxAsset, gen_func_80069A60);
  engine_set_override_main(kParsePsxAsset, parsePsxAsset, gen_func_80068BB0);
  engine_set_override_main(kUnloadPsxAsset, unloadPsxAsset, gen_func_800695D0);
  engine_set_override_main(kLoadImage, loadImage, gen_func_80081C50);
  lucent::info(
      "assetprobe",
      "SELFTEST PASS: latest exact upload selected, a one-word target perturbation returned "
      "MISSING, geometry residence resolved, retail mesh cook matched, a cooked-word "
      "perturbation mismatched, and unload changed both ownership contracts to unloaded");
  lucent::info("assetprobe",
               "ARMED retail .psx load/parse/unload and LoadImage boundaries; every wrapper "
               "super-calls the guest body");
}

void spiderman_report_mesh_asset_cook(uint32_t face, const uint32_t *words, std::size_t wordCount) {
  if (!g_installed) {
    return;
  }
  const MeshAssetCookResolution result = g_cookLedger.resolve(face, words, wordCount);
  lucent::info("assetprobe",
               "MESH_COOK face={:08X} asset={} slot={} mesh={:08X} recordBytes={} "
               "loadSequence={} structuralCook={} retainedSource={}",
               face,
               result.assetName.data(),
               result.slot,
               result.mesh,
               result.recordBytes,
               result.loadSequence,
               result.structuralCookExact ? "EXACT" : "REFUSED",
               cookMatchName(result.match));
}

void spiderman_report_texture_asset_binding(Core *,
                                            uint32_t mesh,
                                            const MeshFt4TextureBinding &binding) {
  if (!g_installed) {
    return;
  }
  const AssetResidence geometry = g_ledger.residenceForAddress(mesh);
  if (geometry.valid) {
    lucent::info("assetprobe",
                 "BINDING geometry mesh={:08X} owner={} slot={} base={:08X} bytes={} "
                 "offset=0x{:X} live={}",
                 mesh,
                 geometry.name.data(),
                 geometry.slot,
                 geometry.base,
                 geometry.bytes,
                 mesh - geometry.base,
                 geometry.live ? "yes" : "no");
  } else {
    lucent::error("assetprobe", "BINDING geometry mesh={:08X} owner=MISSING", mesh);
  }
  reportOwner("texture", textureRectangle(binding));
  if (binding.bitsPerPixel == 4u || binding.bitsPerPixel == 8u) {
    const uint16_t paletteEntries = binding.bitsPerPixel == 4u ? 16u : 256u;
    reportOwner("clut", {binding.clutX, binding.clutY, paletteEntries, 1u});
  }
}
