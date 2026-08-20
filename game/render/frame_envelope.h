// frame_envelope.h — FrameEnvelope: THE FIRST NATIVE PRODUCER behind this port's render seam.
//
// WHAT IT DRAWS, and why it is first. The seam (`game/render/render_seam.cpp`) sits on the engine's
// own submitFrame, guest `FUN_80061308`:
//
//     ResetGraph(1); PutDispEnv(db + 0x5C); PutDrawEnv(db + 0x00); DrawOTag(db->ot + 0x3FFC);
//
// Three of those four are NOT the display list — they are the frame's ENVELOPE: which VRAM page the
// video hardware scans out (the page flip), which page and rectangle the GPU draws into, and the
// background clear that PutDrawEnv performs on the way. The native leg runs no guest body, so
// before this file existed it did none of them, and every native frame would have drawn into an
// unspecified page and been scanned out of a stale one.
//
// It is first because THE ABORT SAID SO. Run the native leg and it stops at submitFrame call #1,
// frame ~3, naming scene `'....'` — the boot-init submit that `FUN_80061140` makes at the end of
// graphics init. MEASURED with the RE-21 inventory instrument (`PSXPORT_DEBUG=fcensus`, see
// frame_census.h) on a reference boot:
//
//   call=1 scene='....' ot=800CE5F0 nodes=4098 withPrims=2 words=8 term=yes unknownOp=0 |
//     poly3=0 poly4=0 line=0 rect=0 sprite=0 fill=0 copy=1 upload=0 env=0 nop=4 |
//     DRAW clip=(0,256 512x240) ofs=(0,256) isbg=1 bg=(0,0,0) | DISP disp=(0,0 512x240)
//
// That is the WHOLE of scene `'....'`: an ordering table of 4098 link-only nodes carrying exactly
// one primitive — a GP0(80) VRAM→VRAM copy of a 2x1 block from (0,0) to (0,0), which is libgpu's
// own chain-terminator packet at 0x800B0EE8 and copies VRAM onto itself — plus four GP0(00) no-ops.
// So the engine's first frame contains NO GEOMETRY AT ALL, and its entire picture is the envelope.
// A producer for it is a producer for the envelope, and nothing else.
//
// THE PICTURE RULE (coord/PROTOCOL.md), checkably satisfied:
//   1. No `gen_func_*` body runs — the seam does not super-call on this leg, and nothing here calls
//      into the substrate.
//   2. Every input is the game's own submission INPUT: the DRAWENV and DISPENV blocks the engine
//      fills in and hands to libgpu, plus libgpu's own VRAM-extent and video-standard bytes.
//      Nothing is recovered from an OT link, a GP0 packet, or a GTE register.
//
// WHAT IT DELIBERATELY DOES NOT DO.
//   * `ResetGraph(1)`. `tools/ghidra_query.py func 0x8008173C` shows mode 1 taking neither the
//   reset
//     nor the re-init path: it falls through to the GPU-system jump-table entry at +0x34, libgpu's
//     "abort the in-flight DMA and reset the command queue". That is queue management for a
//     hardware DMA engine this port does not have — the framework's GPU is called synchronously —
//     so it has no picture, and the native leg is right not to reproduce it. This is a stated
//     omission with a reason, not a stub.
//   * `GP1(06)`, the analog horizontal display range. See gpu_env.h.
//   * The display list. That is the per-scene producers' job (docs/re-frontier.md RE-21), and until
//   a
//     scene has one the seam still aborts naming it.
#pragma once
#include "gpu_env.h"
#include <stdint.h>

class Core;

class FrameEnvelope {
public:
  // Produce this frame's envelope: the page flip, the drawing area/offset/mode, and the background
  // clear. Called from the seam's native leg for EVERY scene, before the per-scene dispatch,
  // because the envelope belongs to the frame rather than to the scene.
  //
  // `drawEnvAddr` / `dispEnvAddr` are the two blocks the current double-buffer context names — the
  // same two addresses the guest's own submitFrame passes to PutDrawEnv / PutDispEnv.
  void produce(Core *c, uint32_t drawEnvAddr, uint32_t dispEnvAddr);

  // ---- the reach evidence, emitted DURING the run --------------------------------------------
  // Same reasoning as the seam's own counters (see render_seam.cpp): the watchdog `_exit()`s, so an
  // atexit summary would never print. `produced()` is bumped unconditionally on the same line as
  // the work, so "0 produced" is a real measurement and not the absence of one.
  unsigned long long produced() const {
    return mProduced;
  }
  unsigned long long clears() const {
    return mClears;
  }

  // ---- the equivalence self-check (PSXPORT_DEBUG=envcheck) -----------------------------------
  // On the REFERENCE leg the guest's own PutDrawEnv leaves the DR_ENV packet it built in guest RAM
  // at DRAWENV+0x1C. `verifyAgainstGuest` recomputes the same words from the same DRAWENV and
  // compares them word for word, so the port's arithmetic is checked against libgpu's on every
  // frame of a real boot instead of being argued for. It is a DIAGNOSTIC: it draws nothing, and it
  // runs only on the leg where the guest packet exists.
  //
  // DESIGNED NEGATIVE: the summary line always carries `checked=` alongside `mismatch=`, so
  // `mismatch=0 checked=0` (never ran) cannot be mistaken for `mismatch=0 checked=3411` (verified).
  void verifyAgainstGuest(Core *c, uint32_t drawEnvAddr);
  unsigned long long checked() const {
    return mChecked;
  }
  unsigned long long mismatched() const {
    return mMismatched;
  }

private:
  // The last DISPENV this producer programmed the display geometry from. The port keeps its OWN
  // cache instead of reading libgpu's (0x800B0E94): on the native leg the guest's PutDispEnv never
  // runs, so libgpu's cache is frozen at whatever boot left there and comparing against it would
  // stop the port re-programming a mode that had genuinely changed. Default-constructed = "nothing
  // programmed yet", which differs from every real DISPENV (dispW 0), so the first frame always
  // programs.
  DispEnv mLastDisp;
  unsigned long long mProduced = 0, mClears = 0, mChecked = 0, mMismatched = 0;
  static constexpr unsigned long long kReportEvery = 512;
  static constexpr int kMaxMismatchLines = 8;
};
