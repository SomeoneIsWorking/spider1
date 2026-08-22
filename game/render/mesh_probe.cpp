// mesh_probe.cpp — live structural evidence for Spider-Man's first native display-list producer.
//
// Static RE establishes the chain and the layout arithmetic:
//
//   FUN_80076480(displayObject)
//     -> FUN_80077D64(meshHeader, objectRelativeTranslation)
//       -> FUN_8007C4D8(faceStream, secondaryRecords, faceCount)
//
// FUN_80077D64 derives all three FUN_8007C4D8 arguments from the mesh header. This observe-only
// probe checks that derivation against a running game and names the actual object, camera matrix,
// and relative translation that own each mesh. Those are producer inputs; unlike the ordering
// table, projected GTE output, or live GTE registers, a future native renderer is allowed to
// consume them.
#include "mesh_probe.h"

#include "core.h"
#include "face_builder_census.h"
#include "game.h"
#include "mesh_face_format.h"
#include "mesh_transform.h"
#include "override_registry.h"
#include "texture_asset_probe.h"

#include <lucent/log.h>

#include <cstdint>
#include <cstring>

extern void gen_func_80076480(Core *);
extern void gen_func_80077C08(Core *);
extern void gen_func_80077D64(Core *);
extern void gen_func_8007C4D8(Core *);
int gpu_frame_no(Core *c);

namespace {

constexpr uint32_t kRenderDisplayObject = 0x80076480u;
constexpr uint32_t kSubmitAnimatedMesh = 0x80077C08u;
constexpr uint32_t kSubmitMesh = 0x80077D64u;
constexpr uint32_t kBuildFaces = 0x8007C4D8u;
constexpr uint32_t kFaceControl = 0x1F8003F4u;
constexpr uint32_t kCameraGlobal = 0x1128u;
constexpr uint32_t kCameraRotation = 0x74u;
constexpr uint32_t kMipsS2 = 18u;

// FUN_80077D64's mesh layout, from its instruction-exact pointer arithmetic:
//   +0x02 u16 source-vertex count
//   +0x04 u16 secondary-record count
//   +0x06 u16 face count
//   +0x1C source vertices, 8 bytes each
//   then secondary records, 8 bytes each
//   then the variable-length face stream handed to FUN_8007C4D8

struct ActiveSubmission {
  enum class Kind {
    None,
    Direct,
    Animated,
  };

  Kind kind = Kind::None;
  uint32_t listHead = 0;
  uint32_t owner = 0;
  uint32_t returnAddress = 0;
  uint32_t camera = 0;
  uint32_t mesh = 0;
  uint32_t relativeTranslation = 0;
  MeshDirectTransformInput transformInput;
  MeshDirectTransformContract transform;
};

struct SeenContext {
  uint32_t owner = 0;
  uint32_t mesh = 0;
};

ActiveSubmission g_active;
uint64_t g_objectCalls = 0;
uint64_t g_meshCalls = 0;
uint64_t g_layoutMismatches = 0;
uint64_t g_transformMismatches = 0;
FaceBuilderCensus g_faceBuilderCensus;
SeenContext g_seen[64];
uint32_t g_seenCount = 0;
uint32_t g_orphanLines = 0;
uint32_t g_transformMismatchLines = 0;

bool firstContext(uint32_t owner, uint32_t mesh) {
  for (uint32_t i = 0; i < g_seenCount; ++i) {
    if (g_seen[i].owner == owner && g_seen[i].mesh == mesh) {
      return false;
    }
  }
  if (g_seenCount == 64u) {
    return false;
  }
  g_seen[g_seenCount++] = {owner, mesh};
  return true;
}

int32_t signedWord(uint32_t word) {
  int32_t result = 0;
  std::memcpy(&result, &word, sizeof(result));
  return result;
}

int16_t signedHalf(uint16_t half) {
  int16_t result = 0;
  std::memcpy(&result, &half, sizeof(result));
  return result;
}

const char *submissionName(ActiveSubmission::Kind kind) {
  switch (kind) {
  case ActiveSubmission::Kind::Direct:
    return "direct";
  case ActiveSubmission::Kind::Animated:
    return "animated";
  case ActiveSubmission::Kind::None:
    return "none";
  }
  return "invalid";
}

uint32_t faceWord(Core *c, uint32_t faces, uint32_t faceCount, uint32_t byteOffset) {
  return faces != 0u && faceCount != 0u ? c->mem_r32(faces + byteOffset) : 0u;
}

void logDecodedSourceFace(
    Core *c, uint32_t faces, uint32_t faceCount, uint32_t vertices, uint16_t vertexCount) {
  std::array<uint32_t, 7> words{};
  words[0] = faceWord(c, faces, faceCount, 0u);
  const uint32_t control = c->mem_r32(kFaceControl);
  MeshFaceHeader header = decodeMeshFaceHeader(words[0], 0u, control);
  const bool hasIndices = header.recordBytes >= 8u;
  if (hasIndices) {
    words[1] = faceWord(c, faces, faceCount, 4u);
    header = decodeMeshFaceHeader(words[0], words[1], control);
  }
  for (uint32_t i = 2u; i < words.size(); ++i) {
    if (header.recordBytes >= (i + 1u) * sizeof(uint32_t)) {
      words[i] = faceWord(c, faces, faceCount, i * sizeof(uint32_t));
    }
  }
  const bool textureValid = header.quad && header.directTexture && header.recordBytes >= 28u;
  const MeshFt4TextureBinding texture =
      textureValid ? decodeMeshFt4TextureBinding(header.flags, words[4], words[5], words[6])
                   : MeshFt4TextureBinding{};
  std::array<MeshSourceVertex, 4> sourceVertices{};
  const uint32_t usedVertexCount = header.quad ? 4u : 3u;
  bool indicesInRange = hasIndices && vertices != 0u;
  for (uint32_t i = 0; i < usedVertexCount; ++i) {
    const uint32_t index = header.vertexIndices[i];
    indicesInRange = indicesInRange && index < vertexCount;
    if (vertices != 0u && index < vertexCount) {
      const uint32_t source = vertices + index * kSpiderMeshRecordBytes;
      sourceVertices[i] = decodeMeshSourceVertex(c->mem_r32(source), c->mem_r32(source + 4u));
    }
  }
  lucent::debug("meshprobe",
                "sourceFace raw=[{:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}] "
                "control={:08X} effective={:08X} stride={} flags={:04X} shape={} directTexture={} "
                "textureValid={} indices=[{},{},{},{}] inRange={} "
                "vertices=[({},{},{},{:04X}) ({},{},{},{:04X}) ({},{},{},{:04X}) "
                "({},{},{},{:04X})] uv=[({},{}) ({},{}) ({},{}) ({},{})] "
                "clut={:04X}@({}, {}) tpageRaw={:04X} tpage={:04X}@({}, {}) bpp={} blend={}",
                words[0],
                words[1],
                words[2],
                words[3],
                words[4],
                words[5],
                words[6],
                control,
                header.effective,
                header.recordBytes,
                header.flags,
                header.quad ? "quad" : "triangle",
                header.directTexture,
                textureValid,
                header.vertexIndices[0],
                header.vertexIndices[1],
                header.vertexIndices[2],
                header.vertexIndices[3],
                indicesInRange,
                sourceVertices[0].x,
                sourceVertices[0].y,
                sourceVertices[0].z,
                sourceVertices[0].flags,
                sourceVertices[1].x,
                sourceVertices[1].y,
                sourceVertices[1].z,
                sourceVertices[1].flags,
                sourceVertices[2].x,
                sourceVertices[2].y,
                sourceVertices[2].z,
                sourceVertices[2].flags,
                sourceVertices[3].x,
                sourceVertices[3].y,
                sourceVertices[3].z,
                sourceVertices[3].flags,
                texture.uv[0].u,
                texture.uv[0].v,
                texture.uv[1].u,
                texture.uv[1].v,
                texture.uv[2].u,
                texture.uv[2].v,
                texture.uv[3].u,
                texture.uv[3].v,
                texture.clut,
                texture.clutX,
                texture.clutY,
                texture.encodedTpage,
                texture.tpage,
                texture.texturePageX,
                texture.texturePageY,
                texture.bitsPerPixel,
                texture.blendMode);
  if (textureValid) {
    spiderman_report_mesh_asset_cook(faces, words.data(), header.recordBytes / sizeof(uint32_t));
    spiderman_report_texture_asset_binding(c, g_active.mesh, texture);
  }
}

void renderDisplayObject(Core *c) {
  const ActiveSubmission previous = g_active;
  g_active.listHead = c->r[4];
  ++g_objectCalls;
  gen_func_80076480(c);
  g_active = previous;
}

void submitMesh(Core *c) {
  const ActiveSubmission previous = g_active;
  g_active.kind = ActiveSubmission::Kind::Direct;
  g_active.owner = c->r[19]; // s3: FUN_80076480's current object in its internal list walk.
  g_active.returnAddress = c->r[31];
  g_active.mesh = c->r[4];
  g_active.relativeTranslation = c->r[5];

  MeshDirectTransformInput &input = g_active.transformInput;
  input = {};
  input.ownerPresent = g_active.owner != 0u;
  input.relativePresent = g_active.relativeTranslation != 0u;
  input.returnAddress = g_active.returnAddress;
  if (input.ownerPresent) {
    input.objectFlags = c->mem_r16(g_active.owner);
    for (uint32_t axis = 0; axis < 3u; ++axis) {
      input.objectPosition20p12[axis] =
          signedWord(c->mem_r32(g_active.owner + 4u + axis * sizeof(uint32_t)));
      input.objectRotation[axis] =
          signedHalf(c->mem_r16(g_active.owner + 0x10u + axis * sizeof(uint16_t)));
    }
  }

  g_active.camera = c->mem_r32(c->r[28] + kCameraGlobal);
  input.cameraPresent = g_active.camera != 0u;
  if (input.cameraPresent) {
    for (uint32_t axis = 0; axis < 3u; ++axis) {
      input.cameraPosition[axis] =
          signedWord(c->mem_r32(g_active.camera + 4u + axis * sizeof(uint32_t)));
    }
    for (uint32_t word = 0; word < input.cameraRotationWords.size(); ++word) {
      input.cameraRotationWords[word] =
          c->mem_r32(g_active.camera + kCameraRotation + word * sizeof(uint32_t));
    }
  }
  if (g_active.relativeTranslation != 0u) {
    for (uint32_t axis = 0; axis < 3u; ++axis) {
      input.passedRelative[axis] = signedWord(c->mem_r32(
          g_active.relativeTranslation + axis * static_cast<uint32_t>(sizeof(uint32_t))));
    }
  }
  g_active.transform = inspectMeshDirectTransform(input);
  if (!g_active.transform.directPathMatches) {
    ++g_transformMismatches;
  }
  ++g_meshCalls;
  gen_func_80077D64(c);
  g_active = previous;
}

void submitAnimatedMesh(Core *c) {
  const ActiveSubmission previous = g_active;
  g_active = {};
  g_active.kind = ActiveSubmission::Kind::Animated;
  g_active.listHead = previous.listHead;
  g_active.owner = c->r[kMipsS2]; // FUN_80077198's display object at callsite 800778B4.
  g_active.returnAddress = c->r[31];
  g_active.mesh = c->r[4];
  g_active.camera = c->mem_r32(c->r[28] + kCameraGlobal);
  g_active.transformInput.ownerPresent = g_active.owner != 0u;
  if (g_active.transformInput.ownerPresent) {
    g_active.transformInput.objectFlags = c->mem_r16(g_active.owner);
  }
  ++g_meshCalls;
  gen_func_80077C08(c);
  g_active = previous;
}

void logFaceSample(Core *c,
                   uint64_t faceCall,
                   uint32_t builderReturnAddress,
                   uint32_t faces,
                   uint32_t secondary,
                   uint32_t faceCount,
                   uint32_t vertices,
                   uint32_t expectedSecondary,
                   uint32_t expectedFaces,
                   uint16_t vertexCount,
                   uint16_t secondaryCount,
                   uint16_t headerFaceCount,
                   bool layoutMatches) {
  lucent::debug(
      "meshprobe",
      "faceCall={} frame={} submission={} listHead={:08X} owner={:08X} objectFlags={:04X} "
      "directMeshReturnAddress={:08X} builderReturnAddress={:08X} mesh={:08X} camera={:08X} "
      "objectPosition20p12=({},{},{}) objectRotation=({},{},{}) cameraPosition=({},{},{}) "
      "relTrans={:08X} passedRel=({},{},{}) expectedRel=({},{},{}) "
      "cameraRotation=[{},{},{};{},{},{};{},{},{}] transform={} "
      "headerCounts(v={}, secondary={}, faces={}) derived(vertices={:08X}, secondary={:08X}, "
      "faces={:08X}) args(secondary={:08X}, faces={:08X}, count={}) layout={} firstFace=[{:08X} "
      "{:08X}]",
      faceCall,
      gpu_frame_no(c),
      submissionName(g_active.kind),
      g_active.listHead,
      g_active.owner,
      g_active.transformInput.objectFlags,
      g_active.returnAddress,
      builderReturnAddress,
      g_active.mesh,
      g_active.camera,
      g_active.transformInput.objectPosition20p12[0],
      g_active.transformInput.objectPosition20p12[1],
      g_active.transformInput.objectPosition20p12[2],
      g_active.transformInput.objectRotation[0],
      g_active.transformInput.objectRotation[1],
      g_active.transformInput.objectRotation[2],
      g_active.transformInput.cameraPosition[0],
      g_active.transformInput.cameraPosition[1],
      g_active.transformInput.cameraPosition[2],
      g_active.relativeTranslation,
      g_active.transformInput.passedRelative[0],
      g_active.transformInput.passedRelative[1],
      g_active.transformInput.passedRelative[2],
      g_active.transform.expectedRelative[0],
      g_active.transform.expectedRelative[1],
      g_active.transform.expectedRelative[2],
      g_active.transform.cameraRotation[0],
      g_active.transform.cameraRotation[1],
      g_active.transform.cameraRotation[2],
      g_active.transform.cameraRotation[3],
      g_active.transform.cameraRotation[4],
      g_active.transform.cameraRotation[5],
      g_active.transform.cameraRotation[6],
      g_active.transform.cameraRotation[7],
      g_active.transform.cameraRotation[8],
      g_active.mesh == 0u ? "NO-CONTEXT"
                          : (g_active.kind == ActiveSubmission::Kind::Animated
                                 ? "ANIMATED"
                                 : (g_active.transform.directPathMatches ? "MATCH" : "MISMATCH")),
      vertexCount,
      secondaryCount,
      headerFaceCount,
      vertices,
      expectedSecondary,
      expectedFaces,
      secondary,
      faces,
      faceCount,
      layoutMatches ? "MATCH" : (g_active.mesh ? "MISMATCH" : "NO-CONTEXT"),
      faceWord(c, faces, faceCount, 0u),
      faceWord(c, faces, faceCount, 4u));
}

void logProgress() {
  const FaceBuilderStats &totals = g_faceBuilderCensus.totals();
  lucent::info(
      "meshprobe",
      "PROGRESS faceCalls={} faces={} emittedBytes={} meshHeaderContext={} otherBuilders={} "
      "unknownCallsite={} invalidOutputDeltas={} layoutMismatches={} transformMismatches={} "
      "objectCalls={} meshCalls={} uniqueContexts={}",
      totals.calls,
      totals.faces,
      totals.emittedBytes,
      totals.contextualCalls,
      totals.calls - totals.contextualCalls,
      g_faceBuilderCensus.unknown().calls,
      totals.invalidOutputDeltas,
      g_layoutMismatches,
      g_transformMismatches,
      g_objectCalls,
      g_meshCalls,
      g_seenCount);

  const auto &callsites = spiderFaceBuilderCallsites();
  for (std::size_t i = 0; i < callsites.size(); ++i) {
    const FaceBuilderStats &stats = g_faceBuilderCensus.stats(i);
    if (stats.calls != 0u) {
      lucent::info("meshprobe",
                   "CALLER owner={} call={:08X} return={:08X} calls={} faces={} emittedBytes={} "
                   "contextual={}",
                   callsites[i].ownerFunction,
                   callsites[i].callInstruction,
                   callsites[i].returnAddress,
                   stats.calls,
                   stats.faces,
                   stats.emittedBytes,
                   stats.contextualCalls);
    }
  }
}

void buildFaces(Core *c) {
  const uint64_t faceCall = g_faceBuilderCensus.totals().calls + 1u;
  const uint32_t builderReturnAddress = c->r[31];
  const uint32_t faces = c->r[4];
  const uint32_t secondary = c->r[5];
  const uint32_t faceCount = c->r[6] & 0xFFFFu;
  constexpr uint32_t kPrimitiveCursor = 0x800B54B0u;
  const uint32_t cursorBefore = c->mem_r32(kPrimitiveCursor) & 0x00FFFFFFu;

  bool layoutMatches = false;
  uint32_t vertices = 0;
  uint32_t expectedSecondary = 0;
  uint32_t expectedFaces = 0;
  uint16_t vertexCount = 0;
  uint16_t secondaryCount = 0;
  uint16_t headerFaceCount = 0;
  if (g_active.mesh != 0u) {
    vertexCount = c->mem_r16(g_active.mesh + kSpiderMeshVertexCountOffset);
    secondaryCount = c->mem_r16(g_active.mesh + kSpiderMeshSecondaryCountOffset);
    headerFaceCount = c->mem_r16(g_active.mesh + kSpiderMeshFaceCountOffset);
    const MeshLayout layout =
        deriveMeshLayout(g_active.mesh, {vertexCount, secondaryCount, headerFaceCount});
    vertices = layout.vertices;
    expectedSecondary = layout.secondary;
    expectedFaces = layout.faces;
    layoutMatches = meshLayoutArgsMatch(layout, secondary, faces, faceCount);
    if (!layoutMatches) {
      ++g_layoutMismatches;
    }
  }

  const bool newContext = g_active.mesh != 0u && firstContext(g_active.owner, g_active.mesh);
  const bool sampleOrphan = g_active.mesh == 0u && g_orphanLines < 8u;
  const bool sampleTransformMismatch = g_active.kind == ActiveSubmission::Kind::Direct &&
                                       !g_active.transform.directPathMatches &&
                                       g_transformMismatchLines < 8u;
  if (sampleOrphan) {
    ++g_orphanLines;
  }
  if (sampleTransformMismatch) {
    ++g_transformMismatchLines;
  }
  // Print each object/mesh context once, a tiny bounded sample of calls from other builders, and
  // every mismatch. The periodic summary below carries the full denominator, so a quiet tail never
  // masquerades as a probe that stopped running.
  if (newContext || sampleOrphan || sampleTransformMismatch ||
      (g_active.mesh != 0u && !layoutMatches)) {
    logFaceSample(c,
                  faceCall,
                  builderReturnAddress,
                  faces,
                  secondary,
                  faceCount,
                  vertices,
                  expectedSecondary,
                  expectedFaces,
                  vertexCount,
                  secondaryCount,
                  headerFaceCount,
                  layoutMatches);
  }
  if (newContext && layoutMatches) {
    logDecodedSourceFace(c, faces, faceCount, vertices, vertexCount);
  }

  gen_func_8007C4D8(c);
  const uint32_t cursorAfter = c->mem_r32(kPrimitiveCursor) & 0x00FFFFFFu;
  const bool outputDeltaValid = cursorAfter >= cursorBefore;
  const uint32_t emittedBytes = outputDeltaValid ? cursorAfter - cursorBefore : 0u;
  const FaceBuilderRecord record = g_faceBuilderCensus.record(
      {builderReturnAddress, faceCount, emittedBytes, g_active.mesh != 0u, outputDeltaValid});
  if (record.firstCall) {
    lucent::info(
        "meshprobe",
        "FIRST_CALLER owner={} call={:08X} return={:08X} context={} faces={} emittedBytes={} "
        "outputDelta={}",
        record.callsite != nullptr ? record.callsite->ownerFunction : "UNKNOWN",
        record.callsite != nullptr ? record.callsite->callInstruction : 0u,
        builderReturnAddress,
        g_active.mesh != 0u ? submissionName(g_active.kind) : "other-builder",
        faceCount,
        emittedBytes,
        outputDeltaValid ? "valid" : "cursor-decreased");
  }
  if (newContext) {
    lucent::debug("meshprobe",
                  "faceResult faceCall={} owner={:08X} mesh={:08X} faces={} "
                  "primitiveCursor={:06X}->{:06X} emittedBytes={}",
                  faceCall,
                  g_active.owner,
                  g_active.mesh,
                  faceCount,
                  cursorBefore,
                  cursorAfter,
                  emittedBytes);
  }
  if (record.stats.calls == 1u || (g_faceBuilderCensus.totals().calls % 4096u) == 0u) {
    logProgress();
  }
}

} // namespace

void spiderman_install_mesh_probe(Game *) {
  static const lucent::Channel channel{"meshprobe"};
  if (!channel) {
    return;
  }
  const MeshLayout control = deriveMeshLayout(0x80100000u, {4u, 1u, 1u});
  const bool acceptsBaseline = meshLayoutArgsMatch(control, 0x8010003Cu, 0x80100044u, 1u);
  const bool rejectsPerturbation = !meshLayoutArgsMatch(control, 0x8010003Cu, 0x80100048u, 1u);
  const bool guardsEmptyFaces =
      faceWord(nullptr, 0u, 1u, 0u) == 0u && faceWord(nullptr, 0x80100044u, 0u, 0u) == 0u;
  const bool decodesFaceFormat = meshFaceFormatSelftest();
  const bool validatesTransform = meshDirectTransformSelftest();
  const bool validatesCallerCensus = faceBuilderCensusSelftest();
  if (!acceptsBaseline || !rejectsPerturbation || !guardsEmptyFaces || !decodesFaceFormat ||
      !validatesTransform || !validatesCallerCensus) {
    lucent::error("meshprobe",
                  "SELFTEST FAILED (baseline={}, perturbed={}, emptyFaceGuard={}, faceFormat={}, "
                  "directTransform={}, callerCensus={}) — wrappers NOT installed",
                  acceptsBaseline ? "accepted" : "rejected",
                  rejectsPerturbation ? "rejected" : "accepted",
                  guardsEmptyFaces ? "guarded" : "read",
                  decodesFaceFormat ? "decoded" : "wrong",
                  validatesTransform ? "validated" : "wrong",
                  validatesCallerCensus ? "validated" : "wrong");
    return;
  }
  lucent::info("meshprobe",
               "SELFTEST PASS: accepted the header-derived pointers and rejected a +4-byte "
               "face-stream perturbation; null and empty face streams were not read; executable-"
               "derived face decoding passed; the direct transform accepted exact source inputs "
               "and rejected relative, rotation, scale, and callsite perturbations; all twelve "
               "executable face-builder callsites are independently classified");
  engine_set_override_main(kRenderDisplayObject, renderDisplayObject, gen_func_80076480);
  engine_set_override_main(kSubmitAnimatedMesh, submitAnimatedMesh, gen_func_80077C08);
  engine_set_override_main(kSubmitMesh, submitMesh, gen_func_80077D64);
  engine_set_override_main(kBuildFaces, buildFaces, gen_func_8007C4D8);
  lucent::info(
      "meshprobe",
      "ARMED observe-only wrappers on 80076480, animated mesh builder 80077C08, direct mesh "
      "builder 80077D64, and face builder 8007C4D8; owners are captured from the statically "
      "proven caller registers. No faceCall line means these render chains never ran. Every "
      "wrapper super-calls the guest body.");
}
