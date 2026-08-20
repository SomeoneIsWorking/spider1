// mesh_probe.cpp — live structural evidence for Spider-Man's first native display-list producer.
//
// Static RE establishes the chain and the layout arithmetic:
//
//   FUN_80076480(displayObject)
//     -> FUN_80077D64(meshHeader, objectRelativeTranslation)
//       -> FUN_8007C4D8(faceStream, secondaryRecords, faceCount)
//
// FUN_80077D64 derives all three FUN_8007C4D8 arguments from the mesh header. This observe-only
// probe checks that derivation against a running game and names the display object and transform
// that own each mesh. Those are producer inputs; unlike the ordering table and projected GTE
// output, a future native renderer is allowed to consume them.
#include "mesh_probe.h"

#include "core.h"
#include "game.h"
#include "override_registry.h"

#include <lucent/log.h>

#include <cstdint>

extern void gen_func_80076480(Core *);
extern void gen_func_80077D64(Core *);
extern void gen_func_8007C4D8(Core *);
int gpu_frame_no(Core *c);

namespace {

constexpr uint32_t kRenderDisplayObject = 0x80076480u;
constexpr uint32_t kSubmitMesh = 0x80077D64u;
constexpr uint32_t kBuildFaces = 0x8007C4D8u;

// FUN_80076480 conditionally treats the object's byte-offset 0x38 field as a transform pointer.
// Some paths do not use it, so the probe reports the raw field instead of laundering it into a
// pointer unconditionally. Its position is three signed 20.12 values at byte offsets
// 0x08/0x10/0x18; the function subtracts the camera position before passing a three-word
// translation to FUN_80077D64.
constexpr uint32_t kObjectField38 = 0x38u;

// FUN_80077D64's mesh layout, from its instruction-exact pointer arithmetic:
//   +0x02 u16 source-vertex count
//   +0x04 u16 secondary-record count
//   +0x06 u16 face count
//   +0x1C source vertices, 8 bytes each
//   then secondary records, 8 bytes each
//   then the variable-length face stream handed to FUN_8007C4D8
constexpr uint32_t kMeshVertexCount = 0x02u;
constexpr uint32_t kMeshSecondaryCount = 0x04u;
constexpr uint32_t kMeshFaceCount = 0x06u;
constexpr uint32_t kMeshVertices = 0x1Cu;
constexpr uint32_t kRecordBytes = 8u;

struct ActiveSubmission {
  uint32_t object = 0;
  uint32_t objectField38 = 0;
  uint32_t mesh = 0;
  uint32_t relativeTranslation = 0;
  int32_t relativeX = 0;
  int32_t relativeY = 0;
  int32_t relativeZ = 0;
};

struct SeenContext {
  uint32_t object = 0;
  uint32_t mesh = 0;
};

struct MeshLayout {
  uint32_t vertices = 0;
  uint32_t secondary = 0;
  uint32_t faces = 0;
  uint16_t faceCount = 0;
};

ActiveSubmission g_active;
uint64_t g_objectCalls = 0;
uint64_t g_meshCalls = 0;
uint64_t g_faceCalls = 0;
uint64_t g_contextualFaceCalls = 0;
uint64_t g_orphanFaceCalls = 0;
uint64_t g_layoutMismatches = 0;
SeenContext g_seen[64];
uint32_t g_seenCount = 0;
uint32_t g_orphanLines = 0;

MeshLayout
deriveLayout(uint32_t mesh, uint16_t vertexCount, uint16_t secondaryCount, uint16_t faceCount) {
  MeshLayout layout;
  layout.vertices = mesh + kMeshVertices;
  layout.secondary = layout.vertices + static_cast<uint32_t>(vertexCount) * kRecordBytes;
  layout.faces = layout.secondary + static_cast<uint32_t>(secondaryCount) * kRecordBytes;
  layout.faceCount = faceCount;
  return layout;
}

bool argsMatch(const MeshLayout &layout, uint32_t secondary, uint32_t faces, uint32_t faceCount) {
  return secondary == layout.secondary && faces == layout.faces && faceCount == layout.faceCount;
}

bool firstContext(uint32_t object, uint32_t mesh) {
  for (uint32_t i = 0; i < g_seenCount; ++i) {
    if (g_seen[i].object == object && g_seen[i].mesh == mesh) {
      return false;
    }
  }
  if (g_seenCount == 64u) {
    return false;
  }
  g_seen[g_seenCount++] = {object, mesh};
  return true;
}

void renderDisplayObject(Core *c) {
  const ActiveSubmission previous = g_active;
  g_active.object = c->r[4];
  g_active.objectField38 = g_active.object ? c->mem_r32(g_active.object + kObjectField38) : 0u;
  ++g_objectCalls;
  gen_func_80076480(c);
  g_active = previous;
}

void submitMesh(Core *c) {
  const uint32_t previousMesh = g_active.mesh;
  const uint32_t previousTranslation = g_active.relativeTranslation;
  const int32_t previousX = g_active.relativeX;
  const int32_t previousY = g_active.relativeY;
  const int32_t previousZ = g_active.relativeZ;
  g_active.mesh = c->r[4];
  g_active.relativeTranslation = c->r[5];
  if (g_active.relativeTranslation != 0u) {
    g_active.relativeX = static_cast<int32_t>(c->mem_r32(g_active.relativeTranslation));
    g_active.relativeY = static_cast<int32_t>(c->mem_r32(g_active.relativeTranslation + 4u));
    g_active.relativeZ = static_cast<int32_t>(c->mem_r32(g_active.relativeTranslation + 8u));
  } else {
    g_active.relativeX = 0;
    g_active.relativeY = 0;
    g_active.relativeZ = 0;
  }
  ++g_meshCalls;
  gen_func_80077D64(c);
  g_active.mesh = previousMesh;
  g_active.relativeTranslation = previousTranslation;
  g_active.relativeX = previousX;
  g_active.relativeY = previousY;
  g_active.relativeZ = previousZ;
}

void logFaceSample(Core *c,
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
      "faceCall={} frame={} object={:08X} objectFlags={:04X} field38={:08X} mesh={:08X} "
      "relTrans={:08X} rel=({},{},{}) "
      "headerCounts(v={}, secondary={}, faces={}) derived(vertices={:08X}, secondary={:08X}, "
      "faces={:08X}) args(secondary={:08X}, faces={:08X}, count={}) layout={} firstFace=[{:08X} "
      "{:08X}]",
      g_faceCalls,
      gpu_frame_no(c),
      g_active.object,
      g_active.object ? c->mem_r16(g_active.object) : 0u,
      g_active.objectField38,
      g_active.mesh,
      g_active.relativeTranslation,
      g_active.relativeX,
      g_active.relativeY,
      g_active.relativeZ,
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
      faceCount ? c->mem_r32(faces) : 0u,
      faceCount ? c->mem_r32(faces + 4u) : 0u);
}

void logProgress() {
  lucent::info("meshprobe",
               "PROGRESS faceCalls={} contextual={} orphan={} layoutMismatches={} "
               "objectCalls={} meshCalls={} uniqueContexts={}",
               g_faceCalls,
               g_contextualFaceCalls,
               g_orphanFaceCalls,
               g_layoutMismatches,
               g_objectCalls,
               g_meshCalls,
               g_seenCount);
}

void buildFaces(Core *c) {
  ++g_faceCalls;
  const uint32_t faces = c->r[4];
  const uint32_t secondary = c->r[5];
  const uint32_t faceCount = c->r[6] & 0xFFFFu;

  bool layoutMatches = false;
  uint32_t vertices = 0;
  uint32_t expectedSecondary = 0;
  uint32_t expectedFaces = 0;
  uint16_t vertexCount = 0;
  uint16_t secondaryCount = 0;
  uint16_t headerFaceCount = 0;
  if (g_active.mesh != 0u) {
    ++g_contextualFaceCalls;
    vertexCount = c->mem_r16(g_active.mesh + kMeshVertexCount);
    secondaryCount = c->mem_r16(g_active.mesh + kMeshSecondaryCount);
    headerFaceCount = c->mem_r16(g_active.mesh + kMeshFaceCount);
    const MeshLayout layout =
        deriveLayout(g_active.mesh, vertexCount, secondaryCount, headerFaceCount);
    vertices = layout.vertices;
    expectedSecondary = layout.secondary;
    expectedFaces = layout.faces;
    layoutMatches = argsMatch(layout, secondary, faces, faceCount);
    if (!layoutMatches) {
      ++g_layoutMismatches;
    }
  } else {
    ++g_orphanFaceCalls;
  }

  const bool newContext = g_active.mesh != 0u && firstContext(g_active.object, g_active.mesh);
  const bool sampleOrphan = g_active.mesh == 0u && g_orphanLines < 8u;
  if (sampleOrphan) {
    ++g_orphanLines;
  }
  // Print each object/mesh context once, a tiny bounded sample of calls from other builders, and
  // every mismatch. The periodic summary below carries the full denominator, so a quiet tail never
  // masquerades as a probe that stopped running.
  if (newContext || sampleOrphan || (g_active.mesh != 0u && !layoutMatches)) {
    logFaceSample(c,
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
  if (g_faceCalls == 1u || (g_faceCalls % 4096u) == 0u) {
    logProgress();
  }

  gen_func_8007C4D8(c);
}

} // namespace

void spiderman_install_mesh_probe(Game *) {
  static const lucent::Channel channel{"meshprobe"};
  if (!channel) {
    return;
  }
  const MeshLayout control = deriveLayout(0x80100000u, 4u, 1u, 1u);
  const bool acceptsBaseline = argsMatch(control, 0x8010003Cu, 0x80100044u, 1u);
  const bool rejectsPerturbation = !argsMatch(control, 0x8010003Cu, 0x80100048u, 1u);
  if (!acceptsBaseline || !rejectsPerturbation) {
    lucent::error("meshprobe",
                  "SELFTEST FAILED (baseline={}, perturbed={}) — wrappers NOT installed",
                  acceptsBaseline ? "accepted" : "rejected",
                  rejectsPerturbation ? "rejected" : "accepted");
    return;
  }
  lucent::info("meshprobe",
               "SELFTEST PASS: accepted the header-derived pointers and rejected a +4-byte "
               "face-stream perturbation");
  engine_set_override_main(kRenderDisplayObject, renderDisplayObject, gen_func_80076480);
  engine_set_override_main(kSubmitMesh, submitMesh, gen_func_80077D64);
  engine_set_override_main(kBuildFaces, buildFaces, gen_func_8007C4D8);
  lucent::info("meshprobe",
               "ARMED observe-only wrappers on 80076480 -> 80077D64 -> 8007C4D8; no faceCall line "
               "means this render chain never ran. Every wrapper super-calls the guest body.");
}
