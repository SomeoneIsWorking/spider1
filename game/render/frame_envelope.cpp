#include "frame_envelope.h"
#include "core.h"
#include "game.h"
#include "gpu_env.h"
#include "gpu_native_internal.h" // gpu_gp0 / gpu_gp1 — the framework's GPU command entry points
#include <lucent/log.h>

namespace spider::render {

// Compile-time negative control: 0 makes the producer emit nothing. A constant because the config
// registry ignores unregistered PSXPORT_* knobs.
#define SPIDERMAN_FRAME_ENVELOPE_PRODUCER 1

void FrameEnvelope::produce(Core *c, uint32_t drawEnvAddr, uint32_t dispEnvAddr) {
#if !SPIDERMAN_FRAME_ENVELOPE_PRODUCER
  // Keep counting so a disabled build is visible in the log.
  ++mProduced;
  if (mProduced == 1 || mProduced % kReportEvery == 0) {
    lucent::info("envelope",
                 "SUPPRESSED build (SPIDERMAN_FRAME_ENVELOPE_PRODUCER=0) — the negative "
                 "control. calls={} clears=0; no GP0/GP1 word is emitted.",
                 mProduced);
  }
  (void)c;
  (void)drawEnvAddr;
  (void)dispEnvAddr;
  return;
#else

  const DispEnv disp(c, dispEnvAddr);
  const DrawEnv draw(c, drawEnvAddr);

  // PutDispEnv. GP1(05) is issued on every call and flips the scanned-out page (VRAM y=0/256).
  gpu_gp1(c, disp.displayStartWord());
  // GP1(08)/GP1(07) are issued only when the DISPENV geometry changed.
  if (!disp.sameGeometryAs(mLastDisp)) {
    gpu_gp1(c, disp.displayModeWord(c));
    gpu_gp1(c, disp.verticalRangeWord(c));
    mLastDisp = disp;
    lucent::debug("envelope",
                  "display geometry programmed: disp=({},{} {}x{}) screen=({},{} {}x{}) "
                  "inter={} rgb24={} -> GP1(08)={:08X} GP1(07)={:08X}",
                  disp.dispX(),
                  disp.dispY(),
                  disp.dispW(),
                  disp.dispH(),
                  disp.screenX(),
                  disp.screenY(),
                  disp.screenW(),
                  disp.screenH(),
                  disp.interlaced() ? 1 : 0,
                  disp.rgb24() ? 1 : 0,
                  disp.displayModeWord(c),
                  disp.verticalRangeWord(c));
  }

  // PutDrawEnv: six state words in FUN_80082770's order (DR_ENV +0x04 upward).
  gpu_gp0(c, draw.areaTopLeftWord(c));
  gpu_gp0(c, draw.areaBottomRightWord(c));
  gpu_gp0(c, draw.drawOffsetWord());
  gpu_gp0(c, draw.drawModeWord());
  gpu_gp0(c, draw.textureWindowWord());
  gpu_gp0(c, DrawEnv::maskWord());

  // Then the clear, the only part that puts colour on screen.
  uint32_t bg[3];
  const int nbg = draw.backgroundClearWords(c, bg);
  for (int i = 0; i < nbg; ++i) {
    gpu_gp0(c, bg[i]);
  }
  if (nbg) {
    ++mClears;
  }

  ++mProduced;
  if (mProduced == 1) {
    lucent::info("envelope",
                 "frame-envelope producer REACHED — call #1: draw={:08X} clip=({},{} "
                 "{}x{}) ofs=({},{}) isbg={} bg=({},{},{}) | disp={:08X} start=({},{}) "
                 "{}x{} | GP1(05)={:08X}",
                 draw.base(),
                 draw.clipX(),
                 draw.clipY(),
                 draw.clipW(),
                 draw.clipH(),
                 draw.ofsX(),
                 draw.ofsY(),
                 draw.clearsBackground() ? 1 : 0,
                 draw.bgR(),
                 draw.bgG(),
                 draw.bgB(),
                 disp.base(),
                 disp.dispX(),
                 disp.dispY(),
                 disp.dispW(),
                 disp.dispH(),
                 disp.displayStartWord());
  } else if (mProduced % kReportEvery == 0) {
    lucent::info("envelope",
                 "frame-envelope produced={} clears={} (page now ({},{}), draw page y={})",
                 mProduced,
                 mClears,
                 disp.dispX(),
                 disp.dispY(),
                 draw.clipY());
  }

  lucent::debug("envelope",
                "#{} draw={:08X} clip=({},{} {}x{}) ofs=({},{}) clearWords={} bg=({},{},{}) "
                "| disp={:08X} start=({},{}) | E3={:08X} E4={:08X} E5={:08X} E1={:08X} "
                "E2={:08X} fill0={:08X}",
                mProduced,
                draw.base(),
                draw.clipX(),
                draw.clipY(),
                draw.clipW(),
                draw.clipH(),
                draw.ofsX(),
                draw.ofsY(),
                nbg,
                draw.bgR(),
                draw.bgG(),
                draw.bgB(),
                disp.base(),
                disp.dispX(),
                disp.dispY(),
                draw.areaTopLeftWord(c),
                draw.areaBottomRightWord(c),
                draw.drawOffsetWord(),
                draw.drawModeWord(),
                draw.textureWindowWord(),
                nbg ? bg[0] : 0u);
#endif
}

// Equivalence self-check: recomputes the words of the guest's DR_ENV (DRAWENV+0x1C) and compares.
// The DR_ENV tag word is skipped; it carries libgpu's OT link.
void FrameEnvelope::verifyAgainstGuest(Core *c, uint32_t drawEnvAddr) {
  if (!lucent::channel_on("envcheck")) {
    return; // guards the guest reads, which are non-logging work
  }
  if (!drawEnvAddr) {
    return;
  }

  const DrawEnv draw(c, drawEnvAddr);
  const uint32_t kDrEnv = drawEnvAddr + 0x1Cu;

  uint32_t mine[9];
  mine[0] = draw.areaTopLeftWord(c);
  mine[1] = draw.areaBottomRightWord(c);
  mine[2] = draw.drawOffsetWord();
  mine[3] = draw.drawModeWord();
  mine[4] = draw.textureWindowWord();
  mine[5] = DrawEnv::maskWord();
  const int nbg = draw.backgroundClearWords(c, &mine[6]);
  const int n = 6 + nbg;

  // Guest words start at DR_ENV +0x04; the tag's high byte is the word count.
  const int guestWords = (int)(c->mem_r32(kDrEnv) >> 24);
  int bad = 0;
  for (int i = 0; i < n; ++i) {
    if (c->mem_r32(kDrEnv + 4u + (uint32_t)i * 4u) != mine[i]) {
      ++bad;
    }
  }
  if (guestWords != n) {
    ++bad;
  }

  ++mChecked;
  if (bad) {
    ++mMismatched;
    if (mMismatched <= kMaxMismatchLines) {
      // One accumulated line for the whole word grid.
      lucent::Line l;
      l.add("MISMATCH #{} at check {}: guest tag words={}, port words={}",
            mMismatched,
            mChecked,
            guestWords,
            n);
      for (int i = 0; i < n; ++i) {
        const uint32_t g = c->mem_r32(kDrEnv + 4u + (uint32_t)i * 4u);
        l.add("\n        [{}] guest={:08X} port={:08X}{}",
              i,
              g,
              mine[i],
              g == mine[i] ? "" : "   <-- differs");
      }
      l.flush(lucent::Level::Error, "envcheck");
    } else if (mMismatched == kMaxMismatchLines + 1) {
      lucent::error("envcheck",
                    "further envelope mismatches suppressed after {} — the running totals "
                    "ride on the periodic line",
                    kMaxMismatchLines);
    }
  }
  if (mChecked % kReportEvery == 0 || mChecked == 1) {
    lucent::info("envcheck",
                 "envelope vs guest PutDrawEnv: checked={} mismatch={} (words compared "
                 "this check = {}, of which background-clear = {})",
                 mChecked,
                 mMismatched,
                 n,
                 nbg);
  }
}

} // namespace spider::render
