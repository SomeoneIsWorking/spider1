// frame_census.cpp — the display-list inventory instrument. See frame_census.h for what it is for
// and, more importantly, for what it is NOT allowed to be used for.
#include "frame_census.h"
#include "core.h"
#include <lucent/log.h>

namespace {

// GP0 primitive word counts, from the command byte. This is the same model the framework's DMA2
// walk relies on; it is duplicated here rather than reached into because that walk is framework
// internal and no framework slot is open this run.
//
// Returns the total words INCLUDING the command word, or 0 for "not modelled" — which the caller
// counts as `unknownOp` and reports, instead of guessing a length and silently mis-parsing the
// rest.
int gp0Len(uint8_t op, Core *c, uint32_t wordAddr, int wordsLeft) {
  if (op >= 0x20 && op <= 0x3F) { // polygon
    const int nv = (op & 0x08) ? 4 : 3;
    const int tex = (op & 0x04) ? 1 : 0;
    const int gour = (op & 0x10) ? 1 : 0;
    return nv * (1 + tex + gour) + (gour ? 0 : 1);
  }
  if (op >= 0x40 && op <= 0x5F) { // line
    const int gour = (op & 0x10) ? 1 : 0;
    if (op & 0x08) { // POLYLINE: variable, ends at the 0x55555555 sentinel
      int n = 1;
      while (n < wordsLeft && n < 256) {
        if (c->mem_r32(wordAddr + (uint32_t)n * 4u) == 0x55555555u) {
          return n + 1;
        }
        ++n;
      }
      return 0; // no sentinel in range: report it rather than invent one
    }
    return 1 + 2 * (1 + gour) - (gour ? 1 : 0); // flat: cmd+v0+v1 = 3; gouraud: cmd+v0+c1+v1 = 4
  }
  if (op >= 0x60 && op <= 0x7F) { // rectangle / sprite
    const int tex = (op & 0x04) ? 1 : 0;
    const int size = (op >> 3) & 3; // 0 = variable, so a W/H word follows
    return 1 + 1 + tex + (size == 0 ? 1 : 0);
  }
  if (op == 0x02) {
    return 3; // fill rectangle
  }
  if (op >= 0x80 && op <= 0x9F) {
    return 4; // VRAM->VRAM copy
  }
  if (op >= 0xA0 && op <= 0xDF) {
    return 3; // CPU<->VRAM transfer header (data words follow the OT)
  }
  if (op == 0x00 || op == 0x01 || op == 0x03 || (op >= 0xE0 && op <= 0xE6) || op == 0x1F) {
    return 1; // NOP / cache flush / env / IRQ
  }
  return 0; // not modelled
}

} // namespace

void FrameCensus::walk(Core *c, uint32_t head) {
  otHead = head;
  if (!head) {
    return; // nodes=0 prims=0 term=false — a real, readable answer
  }
  // PSXPORT_DEBUG=fcensusv adds a per-PRIMITIVE dump. Capped BY NOVELTY, not by an arbitrary
  // first-N: every primitive is printed while the frame is small, and once past kVerboseCap only
  // the FIRST primitive of each GP0 opcode is, so a 4400-node frame still shows one example of
  // every class it contains instead of 40 copies of whatever happens to come first.
  const bool verbose = lucent::channel_on("fcensusv");
  bool opSeen[256] = {false};
  static constexpr int kVerboseCap = 48;
  uint32_t addr = head & 0x1FFFFCu;
  for (nodes = 0; nodes < kMaxNodes; ++nodes) {
    const uint32_t hdr = c->mem_r32(addr);
    const int n = (int)(hdr >> 24);
    if (n > 0) {
      ++nodesWithPrims;
    }
    words += n;
    int i = 0;
    while (i < n) {
      const uint32_t wAddr = addr + 4u + (uint32_t)i * 4u;
      const uint32_t w = c->mem_r32(wAddr);
      const uint8_t op = (uint8_t)(w >> 24);
      const int len = gp0Len(op, c, wAddr, n - i);
      if (verbose && (prims() < kVerboseCap || !opSeen[op])) {
        opSeen[op] = true;
        lucent::debug("fcensusv",
                      "  node@{:08X} +{} op={:02X} len={} w=[{:08X} {:08X} {:08X} {:08X} "
                      "{:08X} {:08X} {:08X} {:08X}]",
                      0x80000000u | addr,
                      i,
                      op,
                      len,
                      w,
                      i + 1 < n ? c->mem_r32(wAddr + 4) : 0u,
                      i + 2 < n ? c->mem_r32(wAddr + 8) : 0u,
                      i + 3 < n ? c->mem_r32(wAddr + 12) : 0u,
                      i + 4 < n ? c->mem_r32(wAddr + 16) : 0u,
                      i + 5 < n ? c->mem_r32(wAddr + 20) : 0u,
                      i + 6 < n ? c->mem_r32(wAddr + 24) : 0u,
                      i + 7 < n ? c->mem_r32(wAddr + 28) : 0u);
      }
      if (len <= 0) {
        ++unknownOp;
        break;
      } // stop THIS node; the counter says the totals are short

      if (op >= 0x20 && op <= 0x3F) {
        if (op & 0x08) {
          ++poly4;
        } else {
          ++poly3;
        }
        if (op & 0x04) {
          ++polyTex;
        }
        if (op & 0x10) {
          ++polyGouraud;
        }
        if (op & 0x02) {
          ++polySemi;
        }
      } else if (op >= 0x40 && op <= 0x5F) {
        ++line;
      } else if (op >= 0x60 && op <= 0x7F) {
        if (op & 0x04) {
          ++sprite;
        } else {
          ++rect;
        }
      } else if (op == 0x02) {
        ++fill;
      } else if (op >= 0x80 && op <= 0x9F) {
        // words: [cmd][src yx][dst yx][h w]. src == dst means the copy writes back what it read.
        const uint32_t src = (i + 1 < n) ? c->mem_r32(wAddr + 4) : 0u;
        const uint32_t dst = (i + 2 < n) ? c->mem_r32(wAddr + 8) : 0u;
        if (src == dst) {
          ++vramCopySelf;
        } else {
          ++vramCopy;
        }
      } else if (op >= 0xA0 && op <= 0xDF) {
        ++upload;
      } else if (op >= 0xE0 && op <= 0xE6) {
        ++envCmd;
      } else {
        ++nop;
      }
      i += len;
    }
    const uint32_t next = hdr & 0xFFFFFFu;
    if (next == 0xFFFFFFu || next == 0) {
      terminated = true;
      ++nodes;
      break;
    }
    addr = next & 0x1FFFFCu;
  }
}

void DrawEnvView::read(Core *c, uint32_t base) {
  clipX = c->mem_r16(base + 0x00);
  clipY = c->mem_r16(base + 0x02);
  clipW = c->mem_r16(base + 0x04);
  clipH = c->mem_r16(base + 0x06);
  ofsX = (int16_t)c->mem_r16(base + 0x08);
  ofsY = (int16_t)c->mem_r16(base + 0x0A);
  twX = c->mem_r16(base + 0x0C);
  twY = c->mem_r16(base + 0x0E);
  twW = c->mem_r16(base + 0x10);
  twH = c->mem_r16(base + 0x12);
  tpage = c->mem_r16(base + 0x14);
  dtd = c->mem_r8(base + 0x16);
  dfe = c->mem_r8(base + 0x17);
  isbg = c->mem_r8(base + 0x18);
  r = c->mem_r8(base + 0x19);
  g = c->mem_r8(base + 0x1A);
  b = c->mem_r8(base + 0x1B);
}

void DispEnvView::read(Core *c, uint32_t base) {
  dispX = c->mem_r16(base + 0x00);
  dispY = c->mem_r16(base + 0x02);
  dispW = c->mem_r16(base + 0x04);
  dispH = c->mem_r16(base + 0x06);
  screenX = c->mem_r16(base + 0x08);
  screenY = c->mem_r16(base + 0x0A);
  screenW = c->mem_r16(base + 0x0C);
  screenH = c->mem_r16(base + 0x0E);
  isinter = c->mem_r8(base + 0x10);
  isrgb24 = c->mem_r8(base + 0x11);
}

void frame_census_report(Core *c,
                         uint32_t drawEnv,
                         uint32_t dispEnv,
                         uint32_t otHead,
                         unsigned long long call,
                         const char *scene) {
  // GUARD: the OT walk is up to 8192 guest reads of NON-logging work, so it is tied to the channel.
  // The log line itself is not wrapped — it lives inside the block the guard exists for.
  if (!lucent::channel_on("fcensus")) {
    return;
  }

  DrawEnvView de;
  de.read(c, drawEnv);
  DispEnvView di;
  di.read(c, dispEnv);
  FrameCensus f;
  f.walk(c, otHead);

  lucent::debug(
      "fcensus",
      "call={} scene='{}' ot={:08X} nodes={} withPrims={} words={} term={} unknownOp={} | "
      "poly3={} poly4={} (tex={} gouraud={} semi={}) line={} rect={} sprite={} fill={} copy={} "
      "selfcopy={} pixelWriters={} "
      "upload={} env={} nop={} | DRAW clip=({},{} {}x{}) ofs=({},{}) tw=({},{} {}x{}) tpage={:04X} "
      "dtd={} dfe={} isbg={} bg=({},{},{}) | DISP disp=({},{} {}x{}) screen=({},{} {}x{}) "
      "inter={} rgb24={}",
      call,
      scene,
      f.otHead,
      f.nodes,
      f.nodesWithPrims,
      f.words,
      f.terminated ? "yes" : "NO",
      f.unknownOp,
      f.poly3,
      f.poly4,
      f.polyTex,
      f.polyGouraud,
      f.polySemi,
      f.line,
      f.rect,
      f.sprite,
      f.fill,
      f.vramCopy,
      f.vramCopySelf,
      f.pixelWriters(),
      f.upload,
      f.envCmd,
      f.nop,
      de.clipX,
      de.clipY,
      de.clipW,
      de.clipH,
      de.ofsX,
      de.ofsY,
      de.twX,
      de.twY,
      de.twW,
      de.twH,
      de.tpage,
      de.dtd,
      de.dfe,
      de.isbg,
      de.r,
      de.g,
      de.b,
      di.dispX,
      di.dispY,
      di.dispW,
      di.dispH,
      di.screenX,
      di.screenY,
      di.screenW,
      di.screenH,
      di.isinter,
      di.isrgb24);
}
