// render_seam.cpp — the two-leg render seam on the engine's own submitFrame (guest FUN_80061308).
//
// LEG 1, `psx_render` — THE REFERENCE. Super-call the recompiled body, so the guest's own
//   ResetGraph/PutDispEnv/PutDrawEnv/DrawOTag runs and the framework's DMA2 linked-list walk draws
//   the picture exactly as it does today. Byte-unchanged: this leg adds reads and log lines and
//   nothing else.
//
// LEG 2, `pc_render` — THE NATIVE RENDERER. Do NOT run the guest body: produce the frame ENVELOPE
//   natively (game/render/frame_envelope.cpp — the page flip, the drawing area/offset/mode and the
//   background clear), then classify the scene from the game's own state and dispatch to its native
//   DISPLAY-LIST producer. There are ZERO display-list producers today, so every scene except the
//   boot-init scene '....' — whose entire picture IS the envelope, measured — reaches
//   abortUnimplemented() and the run dies naming the scene. THAT IS THE CORRECT RESULT for this
//   step (docs/re-frontier.md RE-20/RE-18): no plausible-looking fallback, no OT transcription —
//   the crash list IS the rebuild backlog.
//
// WHICH LEG IS THE DEFAULT, and why this port's default is the opposite of Tomba!2's.
//   Tomba!2 defaults to pc_render because it has producers for the scenes its boot passes through.
//   This port has one producer and it covers exactly one scene, so defaulting to pc_render would
//   abort every run at submitFrame call #2 — including every other agent's boot gate. The default
//   is therefore the REFERENCE leg, stated as the DEFAULT LAYER of the framework's render-path CVar
//   (`PSXPORT_RENDER_PATH`, psxport docs/plans/render-path-tristate.md), so `native` / `gte` /
//   `psx` from the settings file, the environment or a REPL `renderpath` all outrank it by
//   construction — not by this call happening to run before the flag is read. This is not a
//   fallback and not a stopgap: it is the honest statement that this port's shipping picture still
//   comes from the guest's OT walk. The condition for flipping it is not "a producer exists" but
//   "every scene a boot passes through has one" — today that is '....' and not 'dem1'.
//
// THE PICTURE RULE (coord/PROTOCOL.md). The native leg runs no `gen_func_*` body — structurally, by
// not super-calling. Everything this file reads (the level name, the double-buffer context) is read
// for DIAGNOSTICS and for scene identity, never to reconstruct a transform out of GTE output. When
// a producer lands it must resolve from what SUBMITS to the GTE (the setters recorded in
// `c->rsub.projParams`, RE-17), never from the OT, GP0 packets or `gte_read_ctrl()`.
#include "render_seam.h"
#include "config_vars.h" // psx::config::cv_render_path — this port's DEFAULT render path
#include "core.h"
#include "frame_census.h"
#include "frame_envelope.h"
#include "game.h"
#include "game_iface.h"
#include "recomp_iface.h"
#include "render_substrate.h"
#include "scene_id.h"
#include <cstdio>
#include <cstdlib>
#include <lucent/log.h>

// Framework internal, declared at file scope exactly as fntrace.cpp does: inside an anonymous
// namespace it would take internal linkage and fail to resolve.
int gpu_frame_no(Core *c);
void gpu_vram_save(Core *, uint16_t *); // gpu_native.cpp — a copy of the whole 1024x512 CPU VRAM

namespace {

// WHAT THE NATIVE LEG HAD DRAWN WHEN IT GAVE UP. The abort below is the designed outcome for a
// scene with no producer, but "it aborted" says nothing about whether the producers that DID run
// put the right pixels anywhere — and this leg dies by abort(), so no later capture can answer it.
//
// So dump the WHOLE of VRAM, not the display region: the display region depends on the page flip,
// which is itself one of the things a producer here programs, and a capture whose extent moves with
// the thing under test cannot be compared between two builds. 1024x512 is fixed by the hardware.
//
// CAVEAT, stated because it bounds every conclusion drawn from this file: under the VK backend the
// RASTERISED picture lives in the GPU image and is not written back to CPU VRAM (framework issue
// 0006). What this dump therefore shows is everything that reaches CPU VRAM — GP0(02) fills, VRAM
// uploads and copies, FMV output — and NOT projected geometry. That is exactly the right instrument
// for the frame envelope, whose whole visible effect is a fill, and the wrong one for a future
// geometry producer, which will need a present-stage capture instead.
void dumpVramAtAbort(Core *c) {
  static const char *kPath = "scratch/raw/native_abort_vram.ppm";
  static uint16_t vram[1024 * 512];
  gpu_vram_save(c, vram);
  FILE *f = fopen(kPath, "wb");
  if (!f) {
    lucent::error("rseam",
                  "could not open {} — NO abort-time VRAM capture was written, so "
                  "any comparison against one is comparing a stale file",
                  kPath);
    return;
  }
  fprintf(f, "P6\n1024 512\n255\n");
  for (int i = 0; i < 1024 * 512; ++i) {
    const uint16_t p = vram[i];
    const unsigned char rgb[3] = {(unsigned char)((p & 31) << 3),
                                  (unsigned char)(((p >> 5) & 31) << 3),
                                  (unsigned char)(((p >> 10) & 31) << 3)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  long nonzero = 0;
  for (int i = 0; i < 1024 * 512; ++i) {
    if (vram[i]) {
      ++nonzero;
    }
  }
  lucent::error("rseam",
                "abort-time VRAM written to {} — {}/{} halfwords non-zero ({:.2f}%). CPU VRAM "
                "only: rasterised geometry lives in the VK image and is NOT in this capture.",
                kPath,
                nonzero,
                1024 * 512,
                100.0 * (double)nonzero / (1024.0 * 512.0));
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// The guest addresses this file names, each with the instruction it was read from.
//
//   python3 tools/redump_ram.py
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x80061308 0x80061354
//
//   80061308  addiu $sp, $sp, -0x18
//   80061310  jal   0x8008173c          ; ResetGraph
//   80061314  addiu $a0, $zero, 1       ;   (delay slot) mode 1
//   80061318  lw    $a0, 0xcb4($gp)     ; db = *(gp + 0xCB4)   gp = 0x800B47F4 -> 0x800B54A8
//   8006131C  jal   0x80082000          ; PutDispEnv
//   80061320  addiu $a0, $a0, 0x5c      ;   (delay slot) db + 0x5C = the DISPENV
//   80061324  lw    $a0, 0xcb4($gp)
//   80061328  jal   0x80081f40          ; PutDrawEnv(db + 0x00)
//   80061330  lw    $v1, 0xcb4($gp)
//   80061338  lw    $a0, 0x70($v1)      ; db->ot
//   8006133C  jal   0x80081ed0          ; DrawOTag
//   80061340  addiu $a0, $a0, 0x3ffc    ;   (delay slot) the LAST OT entry = the chain head
// ─────────────────────────────────────────────────────────────────────────────────────────────────
constexpr uint32_t kSubmitFrame = 0x80061308u; // the engine's submitFrame — this port's render seam
constexpr uint32_t kCurrentDb =
    0x800B54A8u; // gp+0xCB4: pointer to the ACTIVE double-buffer context

// The Sony standard double-buffer context ("DB"). RE-verified for this game (docs/re-frontier.md
// RE-12): the pair lives at 0x8009A6E4 / 0x8009A75C, stride 0x78, and each field's size is
// confirmed by the size of the libgpu copy that consumes it.
class DrawBuffer {
public:
  DrawBuffer(Core *c, uint32_t base) : mCore(c), mBase(base) {}

  // A null base is a REAL state, not an error: before FUN_80061140 runs graphics init no context
  // has been selected. The accessors below return 0 instead of dereferencing it, and the
  // diagnostics print that 0 — so "no DB yet" is visible in the log rather than being a crash or a
  // lie.
  uint32_t base() const {
    return mBase;
  }
  uint32_t drawEnv() const {
    return mBase + kDrawEnv;
  } // PutDrawEnv memcpys 0x5C bytes from here
  uint32_t dispEnv() const {
    return mBase + kDispEnv;
  } // PutDispEnv memcpys 0x14 bytes from here
  uint32_t otBase() const {
    return mBase ? mCore->mem_r32(mBase + kOtPtr) : 0u;
  }
  uint32_t poolBase() const {
    return mBase ? mCore->mem_r32(mBase + kPoolPtr) : 0u;
  }

  // What the guest hands DrawOTag. The OT is cleared with ClearOTagR(ot, 0x1000) in beginFrame, so
  // it holds 0x1000 entries and the LAST one is the head of the back-to-front chain.
  uint32_t otHead() const {
    const uint32_t ot = otBase();
    return ot ? ot + (kOtEntries - 1) * 4u : 0u;
  }

private:
  static constexpr uint32_t kDrawEnv = 0x00u;
  static constexpr uint32_t kDispEnv = 0x5Cu;
  static constexpr uint32_t kOtPtr = 0x70u;
  static constexpr uint32_t kPoolPtr = 0x74u;
  static constexpr uint32_t kOtEntries = 0x1000u;
  Core *mCore;
  uint32_t mBase;
};

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// class RenderSeam — the override body, its own reach counter, and the scene census.
//
// SANCTIONED SINGLETON. The recomp override table stores a bare `void(*)(Core*)` and carries no
// context pointer, so the trampoline needs a file-scope instance to reach. Same exception the
// framework documents for its own atexit/signal hooks (overlay_router.cpp, sbs.cpp).
// ─────────────────────────────────────────────────────────────────────────────────────────────────
class RenderSeam {
public:
  void install();
  void submitFrame(Core *c);

private:
  void seamPass(Core *c, bool psxLeg); // everything that must not write guest RAM
  void censusTick(Core *c, const SceneName &scene);
  void renderScene(Core *c, const SceneName &scene);
  [[noreturn]] void abortUnimplemented(Core *c, const SceneName &scene, const char *why);
  void superCall(Core *c);

  // Counted UNCONDITIONALLY, on the same line as the work, so the number is honest whether or not a
  // channel is on — a counter bumped inside a log call only counts while someone is watching.
  unsigned long long mCalls = 0;
  unsigned long mSceneChanges = 0;
  bool mHaveScene = false;
  SceneName mLastScene; // default = "nothing read yet"; replaced on the first call

  // HOW A NEGATIVE IS REPORTED, designed before the positive. This port is killed by SIGTERM and
  // the watchdog's own handler `_exit(130)`s, so an atexit summary would never print (watchdog.cpp
  // on_interrupt). The reach evidence is therefore emitted DURING the run: an unconditional line on
  // the first call, and another every kReportEvery calls carrying the running count. So:
  //   * install line present, no "REACHED" line  -> the override never ran. That is a real,
  //   readable
  //     negative, and the install line says so in as many words.
  //   * "REACHED" plus periodic counts           -> it ran, with its own denominator on every line.
  // A run killed at any moment still leaves the count in the log.
  static constexpr unsigned long long kReportEvery = 512;

  // THE FIRST NATIVE PRODUCER. The frame envelope belongs to the FRAME, not to a scene — every
  // scene's picture sits inside the page it flips to and the clip/clear it programs — so it runs
  // before the per-scene dispatch, for every scene. See frame_envelope.h.
  FrameEnvelope mEnvelope;

  // The scene census (docs/re-frontier.md RE-23's open gap): report every CHANGE of the game's own
  // level-name lens, capped so a pathological wobble cannot flood the log — but the cap is
  // announced and the running total rides on every periodic line, so the denominator never goes
  // missing.
  static constexpr unsigned long kMaxSceneLines = 64;
};

RenderSeam g_seam;

void trampoline(Core *c) {
  g_seam.submitFrame(c);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────

void RenderSeam::superCall(Core *c) {
  // The generated dispatcher is `pc = addr; if (override) { override(c); return; } gen(c);`, so an
  // override cannot super-call without naming the generated symbol. Step aside, re-dispatch (which
  // now finds no override and runs the real body), then put ourselves back. Same mechanism the
  // framework's fntrace uses, and the reason its 1761 measured hits are themselves proof that an
  // override at this address executes.
  //
  // LIMIT, stated: while the body runs we are not installed, so a recursive entry would go unhooked
  // and uncounted. FUN_80061308 calls only libgpu leaves and does not recurse.
  const RecompRegistry *R = psxport_recomp();
  R->shard_set_override(kSubmitFrame, nullptr);
  R->main_dispatch(c, kSubmitFrame);
  R->shard_set_override(kSubmitFrame, trampoline);
}

void RenderSeam::censusTick(Core *c, const SceneName &scene) {
  if (mHaveScene && mLastScene.sameAs(scene)) {
    return;
  }
  const bool first = !mHaveScene;
  mHaveScene = true;
  mLastScene = scene;
  ++mSceneChanges;
  if (mSceneChanges <= kMaxSceneLines) {
    lucent::info("scene",
                 "{} scene identity: name='{}' raw={:02X},{:02X},{:02X},{:02X} code=0x{:04X} "
                 "printable={} (call #{}, frame {})",
                 first ? "FIRST" : "CHANGED",
                 scene.text(),
                 scene.rawByte(0),
                 scene.rawByte(1),
                 scene.rawByte(2),
                 scene.rawByte(3),
                 scene.code(),
                 scene.printable() ? 1 : 0,
                 mCalls,
                 gpu_frame_no(c));
  } else if (mSceneChanges == kMaxSceneLines + 1) {
    lucent::info("scene",
                 "further scene-identity changes suppressed after {} lines — the running "
                 "total rides on the periodic [rseam] line",
                 kMaxSceneLines);
  }
}

void RenderSeam::submitFrame(Core *c) {
  ++mCalls;
  const bool psxLeg = c->rsub.mode.psxRender();

  // READ-ONLY SCOPE, enforced rather than asserted. Everything this seam does OUTSIDE the guest
  // body — the scene census, the diagnostics, and the whole native leg — must not write guest main
  // RAM or the scratchpad. The framework's DisplayPassGuard arms a check at the top of every guest
  // store (mem.cpp display_pass_write_guard) that aborts with a guest backtrace on the first
  // violation, so this is a live invariant with a real failure mode, not a comment. The super-call
  // is deliberately OUTSIDE it: the guest's own body legitimately writes guest RAM.
  {
    DisplayPassGuard readOnly(c->rsub.mode);
    seamPass(c, psxLeg);
  }

  if (psxLeg) {
    superCall(c);
    // THE EQUIVALENCE CHECK RUNS AFTER THE SUPER-CALL, and the ordering is the whole point: the
    // guest builds this frame's DR_ENV inside PutDrawEnv, so before the super-call the packet in
    // guest RAM is the one from two frames ago (the DB alternates) — or, on the very first call,
    // uninitialised. Checking there compared the port's CURRENT words against the guest's STALE
    // ones and reported mismatches that were an artefact of the ordering, not of the arithmetic.
    // Diagnostic only (PSXPORT_DEBUG=envcheck); reads guest RAM, writes nothing.
    const DrawBuffer post(c, c->mem_r32(kCurrentDb));
    mEnvelope.verifyAgainstGuest(c, post.drawEnv());
  }
}

void RenderSeam::seamPass(Core *c, bool psxLeg) {
  // The level name the mode switch keeps at 0x800A568C, and the engine's own encoding of it. This
  // is scene IDENTITY, not picture data.
  const SceneName scene(c);
  censusTick(c, scene);

  if (mCalls == 1) {
    lucent::info("rseam",
                 "submitFrame override REACHED — call #1 at frame {}, ra={:08X}, leg={}, "
                 "scene='{}' code=0x{:04X}",
                 gpu_frame_no(c),
                 c->r[31],
                 psxLeg ? "psx_render (reference)" : "pc_render (native)",
                 scene.text(),
                 scene.code());
  } else if (mCalls % kReportEvery == 0) {
    lucent::info("rseam",
                 "submitFrame calls={} frame={} leg={} scene='{}' code=0x{:04X} sceneChanges={}",
                 mCalls,
                 gpu_frame_no(c),
                 psxLeg ? "psx" : "pc",
                 scene.text(),
                 scene.code(),
                 mSceneChanges);
  }

  const DrawBuffer db(c, c->mem_r32(kCurrentDb));
  lucent::debug(
      "rseam",
      "call #{} frame={} db={:08X} ot={:08X} otHead={:08X} pool={:08X} scene='{}' code=0x{:04X}",
      mCalls,
      gpu_frame_no(c),
      db.base(),
      db.otBase(),
      db.otHead(),
      db.poolBase(),
      scene.text(),
      scene.code());

  // The RE-21 display-list inventory (PSXPORT_DEBUG=fcensus). A DIAGNOSTIC — it answers "what is
  // this frame made of", and no producer may resolve anything from it. See frame_census.h.
  frame_census_report(c, db.drawEnv(), db.dispEnv(), db.otHead(), mCalls, scene.text());

  // ---- LEG 1: the reference renderer -----------------------------------------------------------
  // Nothing to PRODUCE here — submitFrame() super-calls the recompiled body once this read-only
  // scope has closed, so the guest's own ResetGraph/PutDispEnv/PutDrawEnv/DrawOTag runs unchanged.
  // The one thing that does run is the envelope producer's EQUIVALENCE CHECK, which needs the
  // guest's own DR_ENV packet to compare against and therefore only exists on this leg. It is a
  // diagnostic (PSXPORT_DEBUG=envcheck), off by default, and emits nothing.
  if (psxLeg) {
    return;
  }

  // ---- LEG 2: the one native renderer ----------------------------------------------------------
  // No super-call, so no `gen_func_*` body runs in the picture path — that is the structural half
  // of the picture rule, checkable by reading this call path.
  //
  // THE FRAME ENVELOPE FIRST, for every scene: the page flip, the drawing area/offset/mode, and the
  // background clear. It is the whole of scene '....' (measured — see frame_envelope.h) and it is
  // layer 0 of every other scene, so it cannot live behind the per-scene dispatch.
  mEnvelope.produce(c, db.drawEnv(), db.dispEnv());

  renderScene(c, scene);

  // The framework drains the render queue at the end of its DMA2 linked-list walk (gpu_native.cpp),
  // which this leg does not run. The envelope emits through gpu_gp0/gpu_gp1, which act immediately
  // and queue nothing, so there is nothing to flush YET — the first producer that uses
  // RenderQueue::emitOrQueue must add `c->game->rq.flush(c)` here, and this comment is the note
  // that it is missing rather than forgotten.
}

void RenderSeam::renderScene(Core *c, const SceneName &scene) {
  // THE ONE NATIVE-RENDERER DISPATCH. A producer is selected here from the scene IDENTITY — never
  // from the OT, the GP0 stream or the GTE. Every scene without one falls through to the abort, and
  // the abort names it, so the crash sequence IS the backlog (docs/re-frontier.md RE-21).
  //
  // SCENE '....' — the engine's boot-init submit, and the FIRST scene the abort named. Its picture
  // is the frame envelope and NOTHING ELSE: mEnvelope.produce() has already drawn it by the time we
  // get here, so the frame is complete and this returns.
  //
  // THE CLAIM IS CHECKED, NOT ASSUMED. "This frame carries no geometry" came from a measurement
  // (frame_envelope.h quotes it), and a measurement is about the run it was taken on. So re-walk
  // the ordering table and abort if it ever carries a primitive that can change a pixel — then a
  // build that silently dropped part of the boot picture would CRASH here instead of looking
  // finished. The walk is a diagnostic use of the OT (it decides whether to abort); nothing about
  // the picture is resolved from it.
  if (scene.unset()) {
    const DrawBuffer db(c, c->mem_r32(kCurrentDb));
    FrameCensus f;
    f.walk(c, db.otHead());
    if (f.pixelWriters() == 0) {
      return; // envelope-only, as measured — frame complete
    }
    lucent::error("rseam",
                  "the boot-init frame carries {} pixel-writing primitives (poly3={} poly4={} "
                  "line={} rect={} sprite={} fill={} copy={} upload={}; self-copies {} are "
                  "excluded because they write back what they read). The envelope-only "
                  "producer does not cover that.",
                  f.pixelWriters(),
                  f.poly3,
                  f.poly4,
                  f.line,
                  f.rect,
                  f.sprite,
                  f.fill,
                  f.vramCopy,
                  f.upload,
                  f.vramCopySelf);
    abortUnimplemented(
        c, scene, "scene '....' is no longer envelope-only — see the [rseam] line above");
  }

  abortUnimplemented(
      c,
      scene,
      "no native producer exists for this scene. The frame ENVELOPE (page flip, "
      "drawing area/offset/mode, background clear) is produced natively for every "
      "scene; what is missing here is the DISPLAY LIST — RE-21, not one display-object "
      "type is decoded yet");
}

void RenderSeam::abortUnimplemented(Core *c, const SceneName &scene, const char *why) {
  const DrawBuffer db(c, c->mem_r32(kCurrentDb));
  const ProjParams &pp = c->rsub.projParams;

  lucent::error(
      "FATAL",
      "\nunimplemented native rendering: {}\n"
      "        scene: name='{}' raw={:02X},{:02X},{:02X},{:02X} code=0x{:04X} printable={}\n"
      "               (the game's own level-name lens at 0x{:08X}, encoded by FUN_8005A734 into "
      "the\n"
      "                value FUN_80062CE0 switches on — docs/re-frontier.md RE-23)\n"
      "        frame {} / submitFrame call #{} / scene changes so far {}\n"
      "        db={:08X} drawenv={:08X} dispenv={:08X} ot={:08X} otHead={:08X} pool={:08X}\n"
      "        projection: geomValid={} OFX={} OFY={} H={}\n"
      "        pc_render has no native producer for this scene. Build one (RE-21), or run the\n"
      "        reference renderer with PSXPORT_RENDER_PSX=1 to get past here. There is no OT-walk\n"
      "        fallback and no env escape hatch on this leg — that is the point of it.\n",
      why,
      scene.text(),
      scene.rawByte(0),
      scene.rawByte(1),
      scene.rawByte(2),
      scene.rawByte(3),
      scene.code(),
      scene.printable() ? 1 : 0,
      SceneName::kAddr,
      gpu_frame_no(c),
      mCalls,
      mSceneChanges,
      db.base(),
      db.drawEnv(),
      db.dispEnv(),
      db.otBase(),
      db.otHead(),
      db.poolBase(),
      pp.geomValid() ? 1 : 0,
      pp.geomOfx(),
      pp.geomOfy(),
      pp.geomH());
  lucent::error("rseam",
                "native producers reached this run: frame envelope produced={} clears={} "
                "(0 produced means the envelope never ran — a different failure from a "
                "producer that ran and drew nothing)",
                mEnvelope.produced(),
                mEnvelope.clears());
  dumpVramAtAbort(c);
  fflush(stderr);
  abort();
}

void RenderSeam::install() {
  const RecompRegistry *R = psxport_recomp();
  if (!R || !R->shard_set_override || !R->main_dispatch || !R->rec_func_index) {
    lucent::error("rseam",
                  "the recomp registry is not installed — the render seam CANNOT be wired, "
                  "and this run's picture is entirely the guest's. Call "
                  "spiderman_install_render_seam() after spiderman_install_recomp().");
    return;
  }
  // Refuse rather than install into nothing. A MAIN-module function ENTRY is exactly what the
  // override table can reach; an overlay address or a mid-function address is not, and installing
  // at one would silently never fire — which reads identically to "the code never ran".
  if (R->rec_func_index(kSubmitFrame) < 0) {
    lucent::error("rseam",
                  "0x{:08X} is not a MAIN-module function entry, so the override table cannot "
                  "reach it. NOT installed — nothing below this line is measuring the seam.",
                  kSubmitFrame);
    return;
  }
  R->shard_set_override(kSubmitFrame, trampoline);
  lucent::info("rseam",
               "render seam installed at 0x{:08X} (the engine's submitFrame — the ONE "
               "game-side DrawOTag caller, RE-19/C029). If no '[rseam] submitFrame override "
               "REACHED' line follows, it never ran.",
               kSubmitFrame);
}

} // namespace

void spiderman_install_render_seam(Game *g) {
  // THE DEFAULT LEG, expressed as the render path's DEFAULT LAYER rather than by poking the live
  // mode.
  //
  // It used to write `mode.setPsxRender(true)` here and rely on running BEFORE native_boot_run read
  // PSXPORT_RENDER_PSX, so that an explicit env value could still win. That ordering dependency is
  // gone: the framework resolves the path through the CVar ladder (Default < Value < Override <
  // Runtime), so this states the port's DEFAULT and every layer above it — the settings file,
  // PSXPORT_RENDER_PATH, a REPL `renderpath` — still outranks it by construction instead of by call
  // order.
  //
  // WHY THIS PORT DEFAULTS TO THE REFERENCE LEG while Tomba!2 defaults to native: it has no
  // DISPLAY-LIST producer for any scene, so a native default would abort every run — including
  // every boot gate — on its first engine frame.
  psx::config::cv_render_path.set(psx::config::Layer::Default, "gte");
  lucent::info("rseam",
               "default render leg = psx_render (reference). This port has ONE native "
               "producer — the frame envelope (page flip, drawing area/offset/mode, "
               "background clear), which is the whole of the boot-init scene '....' and "
               "layer 0 of every other scene. It has no DISPLAY-LIST producer for any scene, "
               "so PSXPORT_RENDER_PSX=0 renders scene '....' natively and then aborts at the "
               "next scene naming it. That abort is the correct result, and it is why the "
               "default has NOT flipped: a pc_render default would abort every boot gate at "
               "submitFrame call #2.");
  g_seam.install();
}
