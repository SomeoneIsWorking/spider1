// gpu_env.cpp — the ported libgpu field→word leaves. See gpu_env.h for the RE provenance of every
// offset and bit position; this file is the transcription-free restatement of those five functions
// in terms of named fields.
#include "gpu_env.h"
#include "core.h"

namespace {

// libgpu's VRAM extent, set by ResetGraph from its own mode table and used by FUN_80082A00 /
// FUN_80082A98 to clamp the drawing area. READ FROM GUEST RAM rather than hardcoded to 1024x512:
// they are libgpu's own state, the clamp is only correct against the values libgpu is actually
// using, and a hardcoded pair would silently disagree the moment a mode changes them.
//
//   0x800B0E2C  u16  VRAM width  (DAT_800b0e2c, loaded from &DAT_800b0ea8 + mode*4)
//   0x800B0E2E  u16  VRAM height (DAT_800b0e2e, loaded from &DAT_800b0eb4 + mode*4)
constexpr uint32_t kLibgpuVramW = 0x800B0E2Cu;
constexpr uint32_t kLibgpuVramH = 0x800B0E2Eu;

// FUN_80082A00 / FUN_80082A98 share this clamp: negative → 0, above the extent → extent-1, and the
// comparison is done on the SIGN-EXTENDED s16 the guest loads, not on the raw field.
int clampToExtent(int v, int extent) {
  const int16_t s = (int16_t)v;
  if (s < 0) {
    return 0;
  }
  if (s > extent - 1) {
    return extent - 1;
  }
  return s;
}

// The guest's own NTSC/PAL selector. `tools/ghidra_query.py func 0x8008BF64` decompiles to exactly
// `return DAT_800B393C;` — libgpu's GetVideoMode is a single load of that byte, and PutDispEnv
// calls it to pick both the GP1(08) PAL bit and the GP1(07) scanline constants. This port is the
// USA release (SLUS_008.75) and its boot programs GP1(08)=0x08000002 with no PAL bit — but the
// value is READ rather than assumed, so a PAL image would come out right instead of silently wrong.
constexpr uint32_t kLibgpuVideoMode = 0x800B393Cu;

// libgpu's "reverse video" flag, ORed into GP1(08) as bit 7 by PutDispEnv (`DAT_800b0e2b`). Read
// for the same reason: it is a real field of the submission, and defaulting it would be a guess.
constexpr uint32_t kLibgpuReverse = 0x800B0E2Bu;

} // namespace

void DrawEnv::read(Core *c, uint32_t base) {
  mBase = base;
  if (!base) {
    return; // a null context is a real state at boot; every field stays 0
  }
  mClipX = c->mem_r16(base + 0x00);
  mClipY = c->mem_r16(base + 0x02);
  mClipW = c->mem_r16(base + 0x04);
  mClipH = c->mem_r16(base + 0x06);
  mOfsX = (int16_t)c->mem_r16(base + 0x08);
  mOfsY = (int16_t)c->mem_r16(base + 0x0A);
  mTwX = c->mem_r8(base + 0x0C);
  mTwY = c->mem_r8(base + 0x0E);
  mTwW = (int16_t)c->mem_r16(base + 0x10);
  mTwH = (int16_t)c->mem_r16(base + 0x12);
  mTpage = c->mem_r16(base + 0x14);
  mDtd = c->mem_r8(base + 0x16);
  mDfe = c->mem_r8(base + 0x17);
  mIsBg = c->mem_r8(base + 0x18);
  mR = c->mem_r8(base + 0x19);
  mG = c->mem_r8(base + 0x1A);
  mB = c->mem_r8(base + 0x1B);
}

uint32_t DrawEnv::areaTopLeftWord(Core *c) const {
  const int w = c->mem_r16(kLibgpuVramW), h = c->mem_r16(kLibgpuVramH);
  return 0xE3000000u | ((uint32_t)clampToExtent(mClipY, h) & 0x3FFu) << 10 |
         ((uint32_t)clampToExtent(mClipX, w) & 0x3FFu);
}

uint32_t DrawEnv::areaBottomRightWord(Core *c) const {
  const int w = c->mem_r16(kLibgpuVramW), h = c->mem_r16(kLibgpuVramH);
  // The guest passes (x+w-1, y+h-1) through a u16 add and an explicit sign-extension back to s16
  // (`((clip.x + clip.w - 1) << 16) >> 16`), so the wrap is part of the contract, not an accident.
  const int brX = (int16_t)(uint16_t)(mClipX + mClipW - 1);
  const int brY = (int16_t)(uint16_t)(mClipY + mClipH - 1);
  return 0xE4000000u | ((uint32_t)clampToExtent(brY, h) & 0x3FFu) << 10 |
         ((uint32_t)clampToExtent(brX, w) & 0x3FFu);
}

uint32_t DrawEnv::drawOffsetWord() const {
  return 0xE5000000u | ((uint32_t)mOfsY & 0x7FFu) << 11 | ((uint32_t)mOfsX & 0x7FFu);
}

uint32_t DrawEnv::drawModeWord() const {
  // FUN_800829E0(dfe, dtd, tpage): bit 9 = dither, bit 10 = draw-to-display-area, and the tpage's
  // own bits survive through the 0x9FF mask (which is what drops any bit 9/10 the tpage word
  // carried).
  uint32_t w = 0xE1000000u | (mDtd ? 0x200u : 0u);
  uint32_t tp = (uint32_t)mTpage & 0x9FFu;
  if (mDfe) {
    tp |= 0x400u;
  }
  return w | tp;
}

uint32_t DrawEnv::textureWindowWord() const {
  // FUN_80082B4C over the DRAWENV's tw RECT: mask from the negated size, offset from the position,
  // both in units of 8 texels.
  return 0xE2000000u | ((uint32_t)(mTwY >> 3) & 0x1Fu) << 15 |
         ((uint32_t)(mTwX >> 3) & 0x1Fu) << 10 | ((uint32_t)(((-mTwH) & 0xFF) >> 3) & 0x1Fu) << 5 |
         ((uint32_t)(((-mTwW) & 0xFF) >> 3) & 0x1Fu);
}

int DrawEnv::backgroundClearWords(Core *c, uint32_t *out) const {
  if (!mIsBg) {
    return 0;
  }
  const int vw = c->mem_r16(kLibgpuVramW), vh = c->mem_r16(kLibgpuVramH);
  const uint32_t clampedW = (uint32_t)clampToExtent(mClipW, vw);
  const uint32_t clampedH = (uint32_t)clampToExtent(mClipH, vh);
  const uint32_t colour = ((uint32_t)mB << 16) | ((uint32_t)mG << 8) | (uint32_t)mR;
  const uint32_t size = (clampedH << 16) | clampedW;

  // The guest's own branch: a clip whose X and width are both 64-aligned can use the VRAM FILL,
  // which is faster on hardware and ignores clip/offset; anything else has to be a real rectangle
  // drawn in drawing space, which is why its position is (clip - ofs) rather than clip.
  if ((mClipX & 0x3F) == 0 && (clampedW & 0x3Fu) == 0) {
    out[0] = 0x02000000u | colour;
    out[1] = ((uint32_t)mClipY << 16) | ((uint32_t)mClipX & 0xFFFFu);
    out[2] = size;
  } else {
    out[0] = 0x60000000u | colour;
    out[1] = ((uint32_t)(uint16_t)(mClipY - mOfsY) << 16) | (uint32_t)(uint16_t)(mClipX - mOfsX);
    out[2] = size;
  }
  return 3;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────

void DispEnv::read(Core *c, uint32_t base) {
  mBase = base;
  if (!base) {
    return;
  }
  mDispX = c->mem_r16(base + 0x00);
  mDispY = c->mem_r16(base + 0x02);
  mDispW = c->mem_r16(base + 0x04);
  mDispH = c->mem_r16(base + 0x06);
  mScreenX = (int16_t)c->mem_r16(base + 0x08);
  mScreenY = (int16_t)c->mem_r16(base + 0x0A);
  mScreenW = (int16_t)c->mem_r16(base + 0x0C);
  mScreenH = (int16_t)c->mem_r16(base + 0x0E);
  mIsInter = c->mem_r8(base + 0x10);
  mIsRgb24 = c->mem_r8(base + 0x11);
}

uint32_t DispEnv::displayStartWord() const {
  return 0x05000000u | ((uint32_t)mDispY & 0x3FFu) << 10 | ((uint32_t)mDispX & 0x3FFu);
}

uint32_t DispEnv::displayModeWord(Core *c) const {
  const bool pal = gpu_env_is_pal(c);
  uint32_t w = pal ? 0x08000008u : 0x08000000u;
  if (mIsRgb24) {
    w |= 0x10u; // bit 4 — 24-bit display colour depth
  }
  if (mIsInter) {
    w |= 0x20u; // bit 5 — interlace
  }
  if (c->mem_r8(kLibgpuReverse)) {
    w |= 0x80u; // bit 7 — libgpu's reverse-video flag
  }
  // The horizontal-resolution ladder is the guest's, thresholds included: it selects a resolution
  // from the DISPENV's own width rather than from anything the GPU reported.
  const int w16 = (int16_t)mDispW;
  if (w16 > 0x118) {
    if (w16 < 0x161) {
      w |= 1; // 320
    } else if (w16 < 0x191) {
      w |= 0x40; // 368
    } else if (w16 < 0x231) {
      w |= 2; // 512
    } else {
      w |= 3; // 640
    }
  } // ...and 256 is the ladder's implicit bottom, encoded as 0
  // 480-line vertical resolution: the threshold itself depends on the video standard.
  const int h16 = (int16_t)mDispH;
  const bool fitsOneField = pal ? (h16 < 0x121) : (h16 < 0x101);
  if (!fitsOneField) {
    w |= 0x24u; // bits 2 + 5 — VRes 480 with interlace
  }
  return w;
}

uint32_t DispEnv::verticalRangeWord(Core *c) const {
  const bool pal = gpu_env_is_pal(c);
  // FUN_80082000's Y arithmetic. Unlike the X range it needs NO libgpu timing table: the first
  // scanline is a fixed standard-dependent constant plus the screen offset, and the last is that
  // plus the screen height — or plus libgpu's 240-line default when the DISPENV leaves height 0,
  // which is what this game ships (measured: screen=(0,0 0x0) on every frame of a reference boot).
  const int y0 = mScreenY + (pal ? 0x13 : 0x10);
  const int y1 = y0 + (mScreenH == 0 ? 0xF0 : mScreenH);
  // The guest's clamps, which differ per standard because the two rasters have different active
  // windows. Reproduced rather than dropped: a degenerate range is a real state this game passes
  // through during screen changes.
  const int lo = pal ? 0x13 : 0x10, hi = pal ? 0x12F : 0x101, hiEnd = pal ? 0x131 : 0x102;
  int cy0 = y0 < lo ? lo : (y0 < hi + 1 ? y0 : hi);
  int cy1 = cy0 + 2;
  if (y1 >= cy1) {
    cy1 = (y1 < hiEnd + 1) ? y1 : hiEnd;
  }
  return 0x07000000u | ((uint32_t)cy1 & 0x3FFu) << 10 | ((uint32_t)cy0 & 0x3FFu);
}

bool DispEnv::sameGeometryAs(const DispEnv &o) const {
  return mDispX == o.mDispX && mDispY == o.mDispY && mDispW == o.mDispW && mDispH == o.mDispH &&
         mScreenX == o.mScreenX && mScreenY == o.mScreenY && mScreenW == o.mScreenW &&
         mScreenH == o.mScreenH && mIsInter == o.mIsInter && mIsRgb24 == o.mIsRgb24;
}

// The video standard, read from libgpu's own byte rather than assumed. See kLibgpuVideoMode.
bool gpu_env_is_pal(Core *c) {
  return c->mem_r8(kLibgpuVideoMode) != 0;
}
