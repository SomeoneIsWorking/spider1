// mesh_face_format.h — source-record semantics for Spider-Man's mesh face builder.
#ifndef SPIDER1_GAME_RENDER_MESH_FACE_FORMAT_H
#define SPIDER1_GAME_RENDER_MESH_FACE_FORMAT_H

#include <array>
#include <cstdint>

constexpr uint32_t kSpiderMeshHeaderBytes = 0x1Cu;
constexpr uint32_t kSpiderMeshRecordBytes = 8u;
constexpr uint32_t kSpiderMeshVertexCountOffset = 0x02u;
constexpr uint32_t kSpiderMeshSecondaryCountOffset = 0x04u;
constexpr uint32_t kSpiderMeshFaceCountOffset = 0x06u;

struct MeshLayoutCounts {
  uint16_t vertices = 0;
  uint16_t secondary = 0;
  uint16_t faces = 0;
};

struct MeshLayout {
  uint32_t vertices = 0;
  uint32_t secondary = 0;
  uint32_t faces = 0;
  uint16_t faceCount = 0;
};

struct MeshFaceHeader {
  uint32_t encoded = 0;
  uint32_t effective = 0;
  uint16_t recordBytes = 0;
  uint16_t flags = 0;
  std::array<uint8_t, 4> vertexIndices{};
  bool quad = false;
  bool directTexture = false;
};

struct MeshUv {
  uint8_t u = 0;
  uint8_t v = 0;
};

struct MeshFt4TextureBinding {
  std::array<MeshUv, 4> uv{};
  uint16_t clut = 0;
  uint16_t encodedTpage = 0;
  uint16_t tpage = 0;
  uint16_t clutX = 0;
  uint16_t clutY = 0;
  uint16_t texturePageX = 0;
  uint16_t texturePageY = 0;
  uint8_t blendMode = 0;
  uint8_t bitsPerPixel = 0;
};

struct MeshSourceVertex {
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  uint16_t flags = 0;
};

MeshFaceHeader decodeMeshFaceHeader(uint32_t encoded, uint32_t indices, uint32_t control);
MeshFt4TextureBinding
decodeMeshFt4TextureBinding(uint16_t faceFlags, uint32_t uvClut, uint32_t uvTpage, uint32_t uvPair);
MeshSourceVertex decodeMeshSourceVertex(uint32_t xy, uint32_t zFlags);
MeshLayout deriveMeshLayout(uint32_t mesh, const MeshLayoutCounts &counts);
bool meshLayoutArgsMatch(const MeshLayout &layout,
                         uint32_t secondary,
                         uint32_t faces,
                         uint32_t faceCount);
bool meshFaceFormatSelftest();

#endif // SPIDER1_GAME_RENDER_MESH_FACE_FORMAT_H
