// DrawEnv/DispEnv lenses over the engine's DRAWENV/DISPENV blocks and the ported libgpu leaves.
#pragma once
#include <stdint.h>

class Core;

namespace spider::render {

// libgpu's video standard, the byte GetVideoMode (0x8008BF64) returns.
bool gpu_env_is_pal(Core *c);

// The Sony DRAWENV, 0x5C bytes (the memcpy PutDrawEnv does into 0x800B0E38); offsets per
// FUN_80082770.
class DrawEnv {
public:
  DrawEnv() = default;
  DrawEnv(Core *c, uint32_t base) {
    read(c, base);
  }
  void read(Core *c, uint32_t base);

  uint32_t base() const {
    return mBase;
  }

  // Drawing clip rectangle in VRAM coordinates; also the background clear's rectangle.
  int clipX() const {
    return mClipX;
  }
  int clipY() const {
    return mClipY;
  }
  int clipW() const {
    return mClipW;
  }
  int clipH() const {
    return mClipH;
  }

  int ofsX() const {
    return mOfsX;
  } // added to every primitive's XY
  int ofsY() const {
    return mOfsY;
  }

  bool clearsBackground() const {
    return mIsBg != 0;
  }
  uint8_t bgR() const {
    return mR;
  }
  uint8_t bgG() const {
    return mG;
  }
  uint8_t bgB() const {
    return mB;
  }

  uint32_t areaTopLeftWord(Core *c) const;     // GP0(E3), FUN_80082A00
  uint32_t areaBottomRightWord(Core *c) const; // GP0(E4), FUN_80082A98
  uint32_t drawOffsetWord() const;             // GP0(E5), FUN_80082B30
  uint32_t drawModeWord() const;               // GP0(E1), FUN_800829E0
  uint32_t textureWindowWord() const;          // GP0(E2), FUN_80082B4C
  static constexpr uint32_t maskWord() {
    return 0xE6000000u;
  }

  // Background clear as FUN_80082770 encodes it; returns 0 without isbg, else 3 words. A clip with
  // 64-aligned X and width becomes a GP0(02) VRAM FILL, else a GP0(60) rectangle in draw space.
  int backgroundClearWords(Core *c, uint32_t *out) const;

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
  DispEnv(Core *c, uint32_t base) {
    read(c, base);
  }
  void read(Core *c, uint32_t base);

  uint32_t base() const {
    return mBase;
  }

  // VRAM rectangle scanned out; the engine alternates dispY between 0 and 256 each frame.
  int dispX() const {
    return mDispX;
  }
  int dispY() const {
    return mDispY;
  }
  int dispW() const {
    return mDispW;
  }
  int dispH() const {
    return mDispH;
  }

  int screenX() const {
    return mScreenX;
  } // screenH == 0 means libgpu's default height
  int screenY() const {
    return mScreenY;
  }
  int screenW() const {
    return mScreenW;
  }
  int screenH() const {
    return mScreenH;
  } // 0 means libgpu's default height

  bool interlaced() const {
    return mIsInter != 0;
  }
  bool rgb24() const {
    return mIsRgb24 != 0;
  }

  uint32_t displayStartWord() const; // GP1(05) — issued every frame by the guest
  uint32_t
  displayModeWord(Core *c) const; // GP1(08) — needs libgpu's video-standard + reverse bytes
  uint32_t
  verticalRangeWord(Core *c) const; // GP1(07) — the scanline constants are standard-dependent
  // GP1(06) is not built: the guest derives it from CRT timing tables at 0x800B0EFC and 0x800B0F24,
  // and the framework GPU discards GP1(06).

  bool sameGeometryAs(const DispEnv &o) const; // the fields the guest's own cache-compare uses

private:
  uint32_t mBase = 0;
  int mDispX = 0, mDispY = 0, mDispW = 0, mDispH = 0;
  int mScreenX = 0, mScreenY = 0, mScreenW = 0, mScreenH = 0;
  uint8_t mIsInter = 0, mIsRgb24 = 0;
};

} // namespace spider::render
