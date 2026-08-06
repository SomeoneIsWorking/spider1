// frame_envelope.cpp — the native producer for the frame envelope. See frame_envelope.h for what it
// draws, why it is the FIRST producer, and what it deliberately leaves out.
#include "frame_envelope.h"
#include "gpu_env.h"
#include "core.h"
#include "game.h"
#include "gpu_native_internal.h"   // gpu_gp0 / gpu_gp1 — the framework's GPU command entry points
#include <lucent/log.h>

// ── THE NEGATIVE CONTROL, and it is deliberately a COMPILE-TIME constant ─────────────────────────
// Flip this to 0, rebuild, and the producer emits nothing while every other line of the program —
// the seam, the counters, the abort, the scene census — is byte-identical. That is the "same binary
// bar one line" the gate requires, and it is a constant rather than an env knob for two reasons:
// this port's config registry warns and IGNORES any unregistered `PSXPORT_*` name (measured on
// PSXPORT_SCENEDUMP: "UNKNOWN knob ... it did NOTHING in this run"), and registering one would be a
// framework edit. A disabled build is not an escape hatch — it makes the picture WORSE, which is the
// whole point of a control.
#define SPIDERMAN_FRAME_ENVELOPE_PRODUCER 1

void FrameEnvelope::produce(Core* c, uint32_t drawEnvAddr, uint32_t dispEnvAddr) {
#if !SPIDERMAN_FRAME_ENVELOPE_PRODUCER
  // The counter still runs, so a disabled build says so IN BAND instead of looking like a producer
  // that was never reached: `[envelope] SUPPRESSED build — produced=N clears=0`.
  ++mProduced;
  if (mProduced == 1 || mProduced % kReportEvery == 0)
    lucent::info("envelope", "SUPPRESSED build (SPIDERMAN_FRAME_ENVELOPE_PRODUCER=0) — the negative "
                             "control. calls={} clears=0; no GP0/GP1 word is emitted.", mProduced);
  (void)c; (void)drawEnvAddr; (void)dispEnvAddr;
  return;
#else

  const DispEnv disp(c, dispEnvAddr);
  const DrawEnv draw(c, drawEnvAddr);

  // ---- PutDispEnv: the PAGE FLIP, and the display geometry ------------------------------------
  // GP1(05) is issued unconditionally by the guest on every call and is what alternates the scanned-
  // out page between VRAM y=0 and y=256 — the single most picture-critical word in the envelope.
  gpu_gp1(c, disp.displayStartWord());
  // GP1(08)/GP1(07) are issued by the guest only when the DISPENV differs from libgpu's cached copy.
  // The port keeps its OWN cache of the last block it programmed rather than reading libgpu's, for
  // the same reason it recomputes the words instead of copying them: on this leg libgpu's cache is
  // never updated (its PutDispEnv never runs), so reading it would compare against a stale value and
  // silently stop re-programming a mode that had genuinely changed.
  if (!disp.sameGeometryAs(mLastDisp)) {
    gpu_gp1(c, disp.displayModeWord(c));
    gpu_gp1(c, disp.verticalRangeWord(c));
    mLastDisp = disp;
    lucent::debug("envelope", "display geometry programmed: disp=({},{} {}x{}) screen=({},{} {}x{}) "
                              "inter={} rgb24={} -> GP1(08)={:08X} GP1(07)={:08X}",
                  disp.dispX(), disp.dispY(), disp.dispW(), disp.dispH(), disp.screenX(),
                  disp.screenY(), disp.screenW(), disp.screenH(), disp.interlaced() ? 1 : 0,
                  disp.rgb24() ? 1 : 0, disp.displayModeWord(c), disp.verticalRangeWord(c));
  }

  // ---- PutDrawEnv: the drawing area, and the background clear ---------------------------------
  // The six state words first, in the guest's own order (FUN_80082770 writes them at DR_ENV +0x04
  // upward, and DrawOTag walks them in that order).
  gpu_gp0(c, draw.areaTopLeftWord(c));
  gpu_gp0(c, draw.areaBottomRightWord(c));
  gpu_gp0(c, draw.drawOffsetWord());
  gpu_gp0(c, draw.drawModeWord());
  gpu_gp0(c, draw.textureWindowWord());
  gpu_gp0(c, DrawEnv::maskWord());

  // ...then the clear, which is the only part of the envelope that puts colour on the screen.
  uint32_t bg[3];
  const int nbg = draw.backgroundClearWords(c, bg);
  for (int i = 0; i < nbg; ++i) gpu_gp0(c, bg[i]);
  if (nbg) ++mClears;

  ++mProduced;
  if (mProduced == 1)
    lucent::info("envelope", "frame-envelope producer REACHED — call #1: draw={:08X} clip=({},{} "
                             "{}x{}) ofs=({},{}) isbg={} bg=({},{},{}) | disp={:08X} start=({},{}) "
                             "{}x{} | GP1(05)={:08X}",
                 draw.base(), draw.clipX(), draw.clipY(), draw.clipW(), draw.clipH(), draw.ofsX(),
                 draw.ofsY(), draw.clearsBackground() ? 1 : 0, draw.bgR(), draw.bgG(), draw.bgB(),
                 disp.base(), disp.dispX(), disp.dispY(), disp.dispW(), disp.dispH(),
                 disp.displayStartWord());
  else if (mProduced % kReportEvery == 0)
    lucent::info("envelope", "frame-envelope produced={} clears={} (page now ({},{}), draw page y={})",
                 mProduced, mClears, disp.dispX(), disp.dispY(), draw.clipY());

  lucent::debug("envelope", "#{} draw={:08X} clip=({},{} {}x{}) ofs=({},{}) clearWords={} bg=({},{},{}) "
                            "| disp={:08X} start=({},{}) | E3={:08X} E4={:08X} E5={:08X} E1={:08X} "
                            "E2={:08X} fill0={:08X}",
                mProduced, draw.base(), draw.clipX(), draw.clipY(), draw.clipW(), draw.clipH(),
                draw.ofsX(), draw.ofsY(), nbg, draw.bgR(), draw.bgG(), draw.bgB(), disp.base(),
                disp.dispX(), disp.dispY(), draw.areaTopLeftWord(c), draw.areaBottomRightWord(c),
                draw.drawOffsetWord(), draw.drawModeWord(), draw.textureWindowWord(),
                nbg ? bg[0] : 0u);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The equivalence self-check. On the reference leg the guest's PutDrawEnv has already built its own
// DR_ENV at DRAWENV+0x1C; this recomputes the same words from the same DRAWENV and compares. Nothing
// about the picture depends on it — it exists so the arithmetic above is CHECKED against libgpu's on
// every frame of a real boot rather than argued for on paper.
//
// The comparison covers the six state words and the (up to) three clear words. It does NOT compare
// the DR_ENV's tag word: that carries libgpu's OT link, which is packet plumbing, not picture, and
// the port emits no packet.
void FrameEnvelope::verifyAgainstGuest(Core* c, uint32_t drawEnvAddr) {
  if (!lucent::channel_on("envcheck")) return;   // guards the guest reads, which are non-logging work
  if (!drawEnvAddr) return;

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

  // The guest's words sit at DR_ENV +0x04 upward (the tag is at +0x00), and its tag's high byte is
  // the word count — read it so a length disagreement is caught as loudly as a value disagreement.
  const int guestWords = (int)(c->mem_r32(kDrEnv) >> 24);
  int bad = 0;
  for (int i = 0; i < n; ++i)
    if (c->mem_r32(kDrEnv + 4u + (uint32_t)i * 4u) != mine[i]) ++bad;
  if (guestWords != n) ++bad;

  ++mChecked;
  if (bad) {
    ++mMismatched;
    if (mMismatched <= kMaxMismatchLines) {
      // One accumulated line rather than N calls: the whole word grid is one finding.
      lucent::Line l;
      l.add("MISMATCH #{} at check {}: guest tag words={}, port words={}", mMismatched, mChecked,
            guestWords, n);
      for (int i = 0; i < n; ++i) {
        const uint32_t g = c->mem_r32(kDrEnv + 4u + (uint32_t)i * 4u);
        l.add("\n        [{}] guest={:08X} port={:08X}{}", i, g, mine[i],
              g == mine[i] ? "" : "   <-- differs");
      }
      l.flush(lucent::Level::Error, "envcheck");
    } else if (mMismatched == kMaxMismatchLines + 1) {
      lucent::error("envcheck", "further envelope mismatches suppressed after {} — the running totals "
                                "ride on the periodic line", kMaxMismatchLines);
    }
  }
  if (mChecked % kReportEvery == 0 || mChecked == 1)
    lucent::info("envcheck", "envelope vs guest PutDrawEnv: checked={} mismatch={} (words compared "
                             "this check = {}, of which background-clear = {})",
                 mChecked, mMismatched, n, nbg);
}
