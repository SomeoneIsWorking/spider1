// mesh_face_format.cpp — executable-derived Spider-Man mesh face decoding.
#include "mesh_face_format.h"

namespace {

uint32_t effectiveFaceWord(uint32_t encoded, uint32_t control) {
  // FUN_8007C4D8 executes sra(control, 16), ANDs that with the encoded word, then ORs the
  // control's low half. Reconstruct the arithmetic shift explicitly so this stays portable.
  uint32_t andMask = control >> 16u;
  if ((control & 0x80000000u) != 0u) {
    andMask |= 0xFFFF0000u;
  }
  return (encoded & andMask) | (control & 0xFFFFu);
}

MeshUv decodeUv(uint16_t packed) {
  return {static_cast<uint8_t>(packed), static_cast<uint8_t>(packed >> 8u)};
}

uint8_t textureBitsPerPixel(uint16_t tpage) {
  constexpr std::array<uint8_t, 4> kDepths{4u, 8u, 16u, 0u};
  return kDepths[(tpage >> 7u) & 0x3u];
}

} // namespace

MeshFaceHeader decodeMeshFaceHeader(uint32_t encoded, uint32_t indices, uint32_t control) {
  MeshFaceHeader result;
  result.encoded = encoded;
  result.effective = effectiveFaceWord(encoded, control);
  result.recordBytes = static_cast<uint16_t>(result.effective >> 16u);
  result.flags = static_cast<uint16_t>(result.effective);
  for (uint32_t i = 0; i < result.vertexIndices.size(); ++i) {
    result.vertexIndices[i] = static_cast<uint8_t>(indices >> (i * 8u));
  }
  result.quad = (result.flags & 0x10u) == 0u;
  result.directTexture = (result.flags & 0x1u) != 0u;
  return result;
}

MeshFt4TextureBinding decodeMeshFt4TextureBinding(uint16_t faceFlags,
                                                  uint32_t uvClut,
                                                  uint32_t uvTpage,
                                                  uint32_t uvPair) {
  MeshFt4TextureBinding result;
  result.uv[0] = decodeUv(static_cast<uint16_t>(uvClut));
  result.uv[1] = decodeUv(static_cast<uint16_t>(uvTpage));
  result.uv[2] = decodeUv(static_cast<uint16_t>(uvPair));
  result.uv[3] = decodeUv(static_cast<uint16_t>(uvPair >> 16u));
  result.clut = static_cast<uint16_t>(uvClut >> 16u);
  result.encodedTpage = static_cast<uint16_t>(uvTpage >> 16u);
  result.tpage = static_cast<uint16_t>(result.encodedTpage | ((faceFlags & 0x180u) >> 2u));
  result.clutX = static_cast<uint16_t>((result.clut & 0x3Fu) * 16u);
  result.clutY = static_cast<uint16_t>((result.clut >> 6u) & 0x1FFu);
  result.texturePageX = static_cast<uint16_t>((result.tpage & 0xFu) * 64u);
  result.texturePageY = (result.tpage & 0x10u) != 0u ? 256u : 0u;
  result.blendMode = static_cast<uint8_t>((result.tpage >> 5u) & 0x3u);
  result.bitsPerPixel = textureBitsPerPixel(result.tpage);
  return result;
}

MeshSourceVertex decodeMeshSourceVertex(uint32_t xy, uint32_t zFlags) {
  return {static_cast<int16_t>(xy),
          static_cast<int16_t>(xy >> 16u),
          static_cast<int16_t>(zFlags),
          static_cast<uint16_t>(zFlags >> 16u)};
}

MeshLayout deriveMeshLayout(uint32_t mesh, const MeshLayoutCounts &counts) {
  MeshLayout layout;
  layout.vertices = mesh + kSpiderMeshHeaderBytes;
  layout.secondary =
      layout.vertices + static_cast<uint32_t>(counts.vertices) * kSpiderMeshRecordBytes;
  layout.faces =
      layout.secondary + static_cast<uint32_t>(counts.secondary) * kSpiderMeshRecordBytes;
  layout.faceCount = counts.faces;
  return layout;
}

bool meshLayoutArgsMatch(const MeshLayout &layout,
                         uint32_t secondary,
                         uint32_t faces,
                         uint32_t faceCount) {
  return secondary == layout.secondary && faces == layout.faces && faceCount == layout.faceCount;
}

bool meshFaceFormatSelftest() {
  const MeshFaceHeader known = decodeMeshFaceHeader(0x001C1083u, 0x02030001u, 0xFFFF0000u);
  const MeshFaceHeader controlled = decodeMeshFaceHeader(0x001C1083u, 0x02030001u, 0xFFFC0001u);
  const MeshFt4TextureBinding texture =
      decodeMeshFt4TextureBinding(known.flags, 0x00E2FD3Bu, 0x0008FD34u, 0xF634F63Bu);
  const MeshFt4TextureBinding plainTexture =
      decodeMeshFt4TextureBinding(0u, 0x00E2FD3Bu, 0x0008FD34u, 0xF634F63Bu);
  const MeshSourceVertex vertex = decodeMeshSourceVertex(0xFFFE0001u, 0x90000003u);
  const MeshLayout layout = deriveMeshLayout(0x80100000u, {4u, 1u, 1u});
  return known.effective == 0x001C1083u && known.recordBytes == 0x1Cu && known.flags == 0x1083u &&
         known.quad && known.directTexture &&
         known.vertexIndices == std::array<uint8_t, 4>{1u, 0u, 3u, 2u} &&
         controlled.effective == 0x001C1081u && texture.uv[0].u == 59u && texture.uv[0].v == 253u &&
         texture.uv[3].u == 52u && texture.uv[3].v == 246u && texture.clutX == 544u &&
         texture.clutY == 3u && texture.encodedTpage == 0x0008u && texture.tpage == 0x0028u &&
         texture.texturePageX == 512u && texture.texturePageY == 0u && texture.blendMode == 1u &&
         texture.bitsPerPixel == 4u && plainTexture.tpage == 0x0008u &&
         plainTexture.blendMode == 0u && vertex.x == 1 && vertex.y == -2 && vertex.z == 3 &&
         vertex.flags == 0x9000u && meshLayoutArgsMatch(layout, 0x8010003Cu, 0x80100044u, 1u) &&
         !meshLayoutArgsMatch(layout, 0x8010003Cu, 0x80100048u, 1u);
}
