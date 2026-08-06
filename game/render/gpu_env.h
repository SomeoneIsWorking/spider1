// gpu_env.h — DrawEnv / DispEnv: typed lenses over the two Sony environment blocks this engine hands
// the GPU every frame, plus the PORTED libgpu leaves that turn them into GPU command words.
//
// WHY THESE ARE GAME STATE AND NOT "WHAT THE GPU PRODUCED" (coord/PROTOCOL.md picture rule). A
// DRAWENV/DISPENV is a struct the ENGINE fills in and SUBMITS. Reading it is reading the submitter's
// inputs, which is exactly what the rule requires; the thing it forbids is going the other way —
// recovering state by inverting a GP0 packet, an OT link or a GTE register. Nothing here reads any of
// those. The word builders below are the port of libgpu's own field→word functions, so the port
// computes the same words from the same inputs rather than copying the words the guest computed.
//
// ─────────────────────────────────────────────────────────────────────────────────────────────────
// RE PROVENANCE. Every offset and every bit position below comes from the game's own libgpu, read out
// of the image with Ghidra headless:
//
//   python3 tools/redump_ram.py
//   python3 tools/ghidra_query.py func 0x80081F40 0x80082770 0x80082000 \
//                                     0x80082A00 0x80082A98 0x80082B30 0x800829E0 0x80082B4C
//
//   0x80081F40  PutDrawEnv(env)  -> FUN_80082770(env+0x1C, env); links the packet; DMAs it; then
//                                   memcpy(0x800B0E38, env, 0x5C) — libgpu's DRAWENV cache.
//   0x80082770  the DR_ENV BUILDER. Writes, at env+0x1C: [E3 area TL][E4 area BR][E5 offset]
//                                   [E1 draw mode][E2 texture window][E6 mask], and — only when
//                                   env->isbg — three more words that are the BACKGROUND CLEAR.
//   0x80082000  PutDispEnv(env)  -> GP1(05) display start UNCONDITIONALLY, then (only when the block
//                                   differs from libgpu's DISPENV cache at 0x800B0E94) GP1(08) mode
//                                   and GP1(06)/(07) analog ranges.
//   0x80082A00 / 0x80082A98 / 0x80082B30 / 0x800829E0 / 0x80082B4C
//                                   the five field→word leaves, ported one-for-one below.
//
// CONFIRMATION THAT THIS READING IS RIGHT, not just plausible: the GP1(08) built by dispModeWord()
// from this game's DISPENV (w=512, isinter=0, isrgb24=0, NTSC) is 0x08000002, and the framework's own
// GPU model independently logs `[gpu] display depth -> 15-bit (GP1(08)=08000002, 512x240)` on a
// reference-leg boot. Two paths, same word.
#pragma once
#include <stdint.h>

class Core;

// libgpu's video standard, read from the byte its own GetVideoMode (0x8008BF64) returns. Exposed
// because the GP1(07) scanline constants and the GP1(08) PAL bit both depend on it.
bool gpu_env_is_pal(Core* c);

// The Sony DRAWENV, 0x5C bytes — the size pinned by the memcpy PutDrawEnv performs into libgpu's
// cache at 0x800B0E38. Field offsets are the ones FUN_80082770 indexes.
class DrawEnv {
 public:
  DrawEnv() = default;
  DrawEnv(Core* c, uint32_t base) { read(c, base); }
  void read(Core* c, uint32_t base);

  uint32_t base() const { return mBase; }

  // The drawing clip rectangle, in VRAM coordinates. This is also the rectangle the background clear
  // covers, which is why the clear needs no separate geometry of its own.
  int clipX() const { return mClipX; }
  int clipY() const { return mClipY; }
  int clipW() const { return mClipW; }
  int clipH() const { return mClipH; }

  int ofsX() const { return mOfsX; }   // added to every primitive's XY by the GPU
  int ofsY() const { return mOfsY; }

  bool     clearsBackground() const { return mIsBg != 0; }   // `isbg`
  uint8_t  bgR() const { return mR; }
  uint8_t  bgG() const { return mG; }
  uint8_t  bgB() const { return mB; }

  // ---- the ported word builders (see the RE block in the header comment) ----
  uint32_t areaTopLeftWord(Core* c) const;      // GP0(E3), FUN_80082A00
  uint32_t areaBottomRightWord(Core* c) const;  // GP0(E4), FUN_80082A98
  uint32_t drawOffsetWord() const;              // GP0(E5), FUN_80082B30
  uint32_t drawModeWord() const;                // GP0(E1), FUN_800829E0
  uint32_t textureWindowWord() const;           // GP0(E2), FUN_80082B4C
  static constexpr uint32_t maskWord() { return 0xE6000000u; }

  // The background clear, exactly as FUN_80082770 encodes it. Returns the number of words written to
  // `out` (0 when isbg is clear, 3 otherwise). Two encodings, and the guest chooses between them:
  // a 64-aligned clip becomes a GP0(02) VRAM FILL (which ignores the clip and the draw offset), and
  // anything else becomes a GP0(60) flat rectangle in drawing space. Both are reproduced because the
  // choice is the guest's, not ours.
  int backgroundClearWords(Core* c, uint32_t* out) const;

 private:
  uint32_t mBase = 0;
  int mClipX = 0, mClipY = 0, mClipW = 0, mClipH = 0;
  int mOfsX = 0, mOfsY = 0;
  int mTwX = 0, mTwY = 0, mTwW = 0, mTwH = 0;
  uint16_t mTpage = 0;
  uint8_t mDtd = 0, mDfe = 0, mIsBg = 0, mR = 0, mG = 0, mB = 0;
};

// The Sony DISPENV, 0x14 bytes — the size pinned by PutDispEnv's memcpy into 0x800B0E94.
class DispEnv {
 public:
  DispEnv() = default;
  DispEnv(Core* c, uint32_t base) { read(c, base); }
  void read(Core* c, uint32_t base);

  uint32_t base() const { return mBase; }

  // The VRAM rectangle the video hardware scans out. THIS IS THE PAGE FLIP: this engine alternates
  // dispY between 0 and 256 every frame, against a DRAWENV whose clip is the other page.
  int dispX() const { return mDispX; }
  int dispY() const { return mDispY; }
  int dispW() const { return mDispW; }
  int dispH() const { return mDispH; }

  int screenX() const { return mScreenX; }   // the analog screen position/size; screenH == 0 means
  int screenY() const { return mScreenY; }   // "libgpu's default height", which is what this game ships
  int screenW() const { return mScreenW; }
  int screenH() const { return mScreenH; }

  bool interlaced() const { return mIsInter != 0; }
  bool rgb24() const { return mIsRgb24 != 0; }

  // ---- the ported word builders ----
  uint32_t displayStartWord() const;         // GP1(05) — issued EVERY frame by the guest; the page flip
  uint32_t displayModeWord(Core* c) const;   // GP1(08) — needs libgpu's video-standard + reverse bytes
  uint32_t verticalRangeWord(Core* c) const; // GP1(07) — the scanline constants are standard-dependent
  // GP1(06), the analog HORIZONTAL range, is deliberately NOT built here. The guest derives it from
  // libgpu's CRT timing tables at 0x800B0EFC / 0x800B0F24, which are not RE'd — and this framework's
  // GPU model DISCARDS GP1(06) outright (`case 0x06: break;` in gpu_native.cpp, with a measurement
  // showing the span it carries only ever restates the resolution GP1(08) already gave). So the
  // omission is stated, bounded, and has no picture consequence on this backend. If a consumer ever
  // needs it, the tables are the RE step, not a guessed constant.

  bool sameGeometryAs(const DispEnv& o) const;   // the fields the guest's own cache-compare uses

 private:
  uint32_t mBase = 0;
  int mDispX = 0, mDispY = 0, mDispW = 0, mDispH = 0;
  int mScreenX = 0, mScreenY = 0, mScreenW = 0, mScreenH = 0;
  uint8_t mIsInter = 0, mIsRgb24 = 0;
};
