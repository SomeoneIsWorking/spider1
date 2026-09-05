// frame_census.h — FrameCensus: what one submitFrame call is actually being asked to draw.
//
// WHY IT EXISTS. docs/codemap.md's "Render walk / display-object list" row says the frame's
// contents are "mapped at call-graph level only (RE-21) — no struct is decoded and no type code is
// named". A native producer cannot be written against a call graph: it needs to know what the
// picture the guest submits IS. This is that inventory, and nothing else.
//
// IT IS A DIAGNOSTIC, AND ONLY A DIAGNOSTIC (coord/PROTOCOL.md picture rule). It reads the OT and
// the two environments to ANSWER A QUESTION — "what is in this frame, and how is the GPU configured
// for it" — and its output goes to a log channel. No producer may call it, and nothing it returns
// enters the picture. Resolving a producer's geometry from these numbers would be exactly the
// inversion the protocol bans; the producer that follows this file resolves from the emitter's own
// inputs.
//
// HOW A NEGATIVE READS, designed before the positive. Every line carries its denominator:
//   * `nodes=N prims=0`   — the walk really traversed N OT entries and found no primitive. Distinct
//                           from the walk never running, because the line is emitted
//                           unconditionally per censused call, not `if (prims)`.
//   * `term=no`           — the chain did not reach a terminator inside kMaxNodes. The counts are
//                           then a LOWER BOUND and the line says so, rather than being silently
//                           truncated into a clean-looking total.
//   * `unknownOp`         — a GP0 byte the length table does not model. Non-zero means the rest of
//                           that node's words were skipped, so the totals under-count; it is
//                           printed on every line for exactly that reason.
// A census that found nothing therefore looks different from a census that never ran, which is the
// failure mode this project has been burned by.
#pragma once
#include <stdint.h>

class Core;

// One submitFrame call's display list, classified by GP0 command class.
struct FrameCensus {
  // ---- the walk itself ----
  uint32_t otHead = 0; // the address handed to DrawOTag
  int nodes = 0;       // OT entries traversed
  int nodesWithPrims = 0;
  int words = 0;           // primitive words seen
  bool terminated = false; // the chain reached 0xFFFFFF / 0 inside kMaxNodes
  int unknownOp = 0;       // GP0 bytes the length model does not cover (see header)

  // ---- primitives, by class ----
  int poly3 = 0, poly4 = 0;                       // 0x20..0x3F, split by vertex count
  int polyTex = 0, polyGouraud = 0, polySemi = 0; // sub-counts of the above
  int line = 0, rect = 0, sprite = 0;             // 0x40..0x5F, 0x60..0x7F (untextured / textured)
  int fill = 0, vramCopy = 0, upload = 0, envCmd = 0, nop = 0;
  // A GP0(80) whose source and destination rectangles are the SAME cannot change a pixel on any
  // hardware. It is counted apart from `vramCopy` because this engine's ordering table always
  // carries one — libgpu's own chain-terminator packet, a 2x1 copy of (0,0) onto (0,0) — and a
  // frame that contains only that is genuinely empty. Classifying it is a property of the
  // primitive, not a special case for one address.
  int vramCopySelf = 0;

  static constexpr int kMaxNodes = 0x2000;

  int prims() const {
    return poly3 + poly4 + line + rect + sprite + fill + vramCopy + vramCopySelf + upload;
  }

  // Primitives that can put different pixels in VRAM. The envelope-only claim in frame_envelope.h
  // rests on this being zero for the boot-init frame; historical runtime evidence checked it
  // rather than trusting the measurement that motivated it.
  int pixelWriters() const {
    return poly3 + poly4 + line + rect + sprite + fill + vramCopy + upload;
  }

  // Walk the ordering table at `otHead` and classify it. Read-only.
  void walk(Core *c, uint32_t otHead);
};

// The Sony DRAWENV / DISPENV the guest hands PutDrawEnv / PutDispEnv, as typed lenses over the
// guest block rather than a pile of offsets at the use site. Sizes are pinned by the copy libgpu
// performs: 0x5C bytes for DRAWENV, 0x14 for DISPENV (docs/re-frontier.md RE-12).
struct DrawEnvView {
  int clipX = 0, clipY = 0, clipW = 0, clipH = 0;
  int ofsX = 0, ofsY = 0;
  int twX = 0, twY = 0, twW = 0, twH = 0;
  uint16_t tpage = 0;
  uint8_t dtd = 0, dfe = 0, isbg = 0, r = 0, g = 0, b = 0;
  void read(Core *c, uint32_t base);
};

struct DispEnvView {
  int dispX = 0, dispY = 0, dispW = 0, dispH = 0; // the VRAM rectangle scanned out
  int screenX = 0, screenY = 0, screenW = 0, screenH = 0;
  uint8_t isinter = 0, isrgb24 = 0;
  void read(Core *c, uint32_t base);
};

// Emit one `fcensus` line for this call. Called unconditionally from the seam; the channel gate is
// inside, and the expensive OT walk is guarded on the channel because it is non-logging work.
void frame_census_report(Core *c,
                         uint32_t drawEnv,
                         uint32_t dispEnv,
                         uint32_t otHead,
                         unsigned long long call,
                         const char *scene);
