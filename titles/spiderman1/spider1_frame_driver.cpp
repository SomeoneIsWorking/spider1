#include "spider1_frame_driver.h"

#include "spider1_cd_initialization.h"
#include "spider1_field_clock.h"
#include "spider1_gpu_reset.h"
#include "spider1_guest_layout.h"

#include "cd_control.h"
#include "core.h"
#include "execution_control.h"
#include "execution_services.h"
#include "game.h"
#include "host_turn.h"
#include "native_execution.h"
#include "spider1_field_schedule.h"
#include "spider1_platform_facts.h"
#include "spider1_stream_driver.h"
#include "spider1_widescreen.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace spider {
namespace {

// Enter `entry` as if the guest had `jal`'d it from `returnPc`. FALSE MEANS THE HOST MUST STOP: a
// bounded exit is not this driver's to swallow, and the frame boundary is what produced it.
//
// This is deliberately not `spider1CallGuest` from `spider1_guest_call.h`. A mode driver treats a
// bounded-exit return as a completed call, because a mode step must reach one presentation fence
// before it hands control back. The frame driver propagates instead, because a display field
// boundary is its own cadence rather than a call anybody made.
bool callGuest(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  return dispatchGuestOrPropagate(core, entry);
}

// The ten callee-saved registers the finite boot prefix spills into the retail main frame's own
// stack window, as (register index, slot) pairs, HIGHEST REGISTER FIRST — which is the order the
// retail code stores them in and therefore the order they must be written here.
struct BootSavedRegister {
  uint32_t index;
  uint32_t slot;
};
constexpr std::array kBootSavedRegisters{
    BootSavedRegister{31, 68},
    BootSavedRegister{30, 64},
    BootSavedRegister{23, 60},
    BootSavedRegister{22, 56},
    BootSavedRegister{21, 52},
    BootSavedRegister{20, 48},
    BootSavedRegister{19, 44},
    BootSavedRegister{18, 40},
    BootSavedRegister{17, 36},
    BootSavedRegister{16, 32},
};

} // namespace

Spider1FrameDriver::Spider1FrameDriver(Game &game)
    : game_(game),
      modes_(std::make_unique<Spider1ModeDriver>(static_cast<Spider1ModeHost &>(*this))) {}

Spider1FrameDriver::~Spider1FrameDriver() = default;

Spider1FrameDriver &Spider1FrameDriver::from(Core &core) {
  if (!core.game || !core.game->frameDriver) {
    lucent::error("frame", "Spider-Man 1 frame callback ran without its title FrameDriver");
    std::abort();
  }
  auto *driver = dynamic_cast<Spider1FrameDriver *>(core.game->frameDriver.get());
  if (!driver) {
    lucent::error("frame", "Spider-Man 1 frame callback reached another title's FrameDriver");
    std::abort();
  }
  return *driver;
}

void Spider1FrameDriver::serviceBootstrapVsync(Core &core) {
  // ResetGraph's early VSync(0) runs before the finite title main has installed its boot fiber.
  // Deliver this field through the same clock, audio, callback, and return-value owners used by
  // the movie player. Nothing was drawn yet, so the presentation fence is unpresented.
  if (mainFrameInstalled_ || fiber_.active() || core.r[31] != spider1::resetGraphVsyncReturn ||
      core.r[4] != 0) {
    lucent::error("frame",
                  "Spider-Man 1 refused pre-main VSync: main={} fiber={} ra=0x{:08X} a0=0x{:08X}",
                  mainFrameInstalled_,
                  fiber_.active(),
                  core.r[31],
                  core.r[4]);
    std::abort();
  }
  const uint32_t returnValue = Spider1FieldClock::vsyncReturnValue(core);
  deliverField(core);
  completeMovieVsync(core, returnValue);
  game_.presentation.commitUnpresented(&core);
  core.pc = core.r[31];
  lucent::info("frame", "Spider-Man 1 completed pre-main ResetGraph field at 0x{:08X}", core.pc);
}

void Spider1FrameDriver::serviceBootstrapMovieVsync(Core &core) {
  const uint32_t returnPc = core.r[31];
  if (mainFrameInstalled_ || fiber_.active() || !spider1::isMovieFieldReturn(returnPc) ||
      core.r[4] != 0) {
    lucent::error("str",
                  "Spider-Man 1 refused retail STR field: main={} fiber={} ra=0x{:08X} a0=0x{:08X}",
                  mainFrameInstalled_,
                  fiber_.active(),
                  returnPc,
                  core.r[4]);
    std::abort();
  }

  // The direct Lightrec boot has already returned the whole guest CPU state at libetc VSync.
  // Deliver exactly that field, then continue at the call's authentic return PC. The retail movie
  // body, including its stack and decode loop, remains the owner of every instruction around it.
  const uint32_t returnValue = Spider1FieldClock::vsyncReturnValue(core);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = false;
  deliverField(core);
  if (core.executionControl().pending()) {
    lucent::error("str",
                  "Spider-Man 1 field callback exited before completing the retail STR field");
    std::abort();
  }
  completeMovieVsync(core, returnValue);
  commitMovieField(core);
  // The call site is at the END of guest work for this field. Begin the next logic frame only
  // after presenting this one, so its OT attributes remain intact through the current fence.
  ++game_.timing.logicFrame;
  core.rsub.otAttr.beginLogicFrame(game_.timing.logicFrame);
  game_.pad.serviceFrame();
  ++movieFieldCount_;
  core.pc = returnPc;
  lucent::info(
      "str", "Spider-Man 1 resumed retail STR field {} at 0x{:08X}", movieFieldCount_, core.pc);
}

void Spider1FrameDriver::serviceBootstrapStreamWait(Core &core) {
  // StGetNext has already executed its authentic body and returned "not ready". The host only
  // supplies the display field that passes while the asynchronous drive catches up. Unlike VSync,
  // this boundary does not update the VSync return value or its horizontal-counter baseline.
  if (mainFrameInstalled_ || fiber_.active() || core.pc != spider1::stGetNextAddress ||
      core.r[2] == 0) {
    lucent::error("str",
                  "Spider-Man 1 refused direct stream wait: main={} fiber={} pc=0x{:08X} v0={}",
                  mainFrameInstalled_,
                  fiber_.active(),
                  core.pc,
                  core.r[2]);
    std::abort();
  }
  const uint32_t continuation = core.r[31];
  fieldsSinceCommit_ = 0;
  frameCommitted_ = false;
  deliverField(core);
  if (core.executionControl().pending()) {
    lucent::error("str", "Spider-Man 1 field callback exited during direct stream wait");
    std::abort();
  }
  commitMovieField(core);
  ++game_.timing.logicFrame;
  core.rsub.otAttr.beginLogicFrame(game_.timing.logicFrame);
  game_.pad.serviceFrame();
  core.pc = continuation;
}

void Spider1FrameDriver::installBootstrapOverrides() {
  // The first post-ResetGraph hardware query is the stock GPU DMA timeout arm. Its VSync(-1)
  // reads the title field count without waiting, so the complete native leaf below owns it.
  installNativeOverride(
      game_.core, spider1::gpuDmaTimeoutStart, "Spider GPU DMA timeout", startGpuDmaTimeout);
  installNativeOverride(game_.core, spider1::cdInitAddress, "Spider CdInit", initializeCd);
  if (!game_.platform_hle.register_(spider1::vsyncCallbackRegistration, captureVsyncCallback)) {
    lucent::error("frame", "Spider-Man 1 could not bind measured VSyncCallback registration");
    std::abort();
  }
  // The directly called stock CdSync body has VSync(-1) timeout polls, while the host CD command
  // completes synchronously. The same complete/ready owner as the public wrapper serves it.
  if (!game_.platform_hle.register_(spider1::innerCdSync, cd_sync_stock_sync)) {
    lucent::error("cd", "Spider-Man 1 could not bind the measured inner CdSync service");
    std::abort();
  }
}

void Spider1FrameDriver::installOverrides() {
  // VSync itself is deliberately absent. The title's platform facts declare its address and
  // PlatformHle installs the framework's protected typed frame boundary. This driver owns the
  // recovered loop state and the two engine boundaries that would otherwise require a successful
  // VSync.
  // VSyncCallback is already bound before crt0 so direct retail boot can publish its field
  // callback. The inner CdSync body is already bound by the pre-main bootstrap phase.
  installNativeOverride(game_.core, spider1::guestFieldWait, "Spider field wait", waitGuestFields);
  installNativeOverride(game_.core, spider1::padRead, "Spider pad service", serviceBootTail);
  installNativeOverride(game_.core, spider1::moviePlayer, "Spider movie player", playMovie);
  installNativeOverride(
      game_.core, Spider1GpuReset::resetGraphEntry, "Spider ResetGraph", resetGraphWithoutVsync);
  // The GPU DMA timeout arm is already installed by the pre-main bootstrap phase.
  // The public CdInit body is already installed by the pre-main bootstrap phase.
  lucent::info(
      "frame",
      "Spider-Man 1 native frame ownership installed: VSync 0x{:08X} remains a frame boundary; "
      "outer dispatcher and all retail mode loops are host-driven",
      spider1::platformServices.vsyncAddress);
}

void Spider1FrameDriver::initializeCd(Core *core) {
  Spider1CdInitialization::install(*core);
}

void Spider1FrameDriver::serviceBootTail(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  const uint32_t returnPc = core->r[31];
  if (!callOriginalOrPropagate(*core, spider1::padRead)) {
    return;
  }

  // 0x8006C2FC..0x8006C35C is the authenticated post-logo wait in game init. It invokes this pad
  // service once per iteration until the callback-maintained game field count advances by 300 (or
  // input dismisses it). On PSX the display interrupt runs between those calls. A runtime turn
  // otherwise holds the boot fiber and prevents the host from delivering
  // even the first field it is waiting for. Preserve the complete pad-service body and make this
  // exact per-field call site yield to the one native cadence owner.
  if (driver.fiber_.phase() == Spider1FiberPhase::Boot && returnPc == spider1::bootTailPadReturn) {
    driver.yieldActiveField(*core, returnPc);
  }
}

void Spider1FrameDriver::registerVsyncCallback(uint32_t callback) {
  if (callback != vsyncCallback_) {
    lucent::info("frame",
                 "Spider-Man 1 registered field callback 0x{:08X} (was 0x{:08X})",
                 callback,
                 vsyncCallback_);
  }
  vsyncCallback_ = callback;
}

void Spider1FrameDriver::captureVsyncCallback(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  driver.registerVsyncCallback(core->r[4]);
  core->r[2] = 0;
}

void Spider1FrameDriver::deliverField(Core &core) {
  core.mem_w32(spider1::libetcVblankCountAddress,
               core.mem_r32(spider1::libetcVblankCountAddress) + 1u);
  ++fieldsSinceCommit_;
  game_.spu_audio.frame();
  if (vsyncCallback_) {
    const R3000 saved = static_cast<const R3000 &>(core);
    if (!dispatchGuestOrPropagate(core, vsyncCallback_)) {
      return;
    }
    static_cast<R3000 &>(core) = saved;
  }
  psx::cpu::notifyDisplayField(core);
}

void Spider1FrameDriver::claimFrameFence(std::string_view what) {
  if (frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 {} followed an earlier fence in the same host step", what);
    std::abort();
  }
  frameCommitted_ = true;
}

void Spider1FrameDriver::commitMovieField(Core &core) {
  claimFrameFence("an STR field");
  game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::bootHostTurn(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  if (driver.fiber_.phase() != Spider1FiberPhase::Boot) {
    lucent::error("boot", "Spider-Man 1 host field turn escaped the finite boot fiber");
    std::abort();
  }
  driver.yieldActiveField(*core, 0);
}

void Spider1FrameDriver::playMovie(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  if (!driver.fiber_.active() || !driver.fiber_.runningOn(*core)) {
    lucent::error("str",
                  "Spider-Man 1 STR player ran outside the title's finite host-stepped fiber");
    std::abort();
  }
  driver.currentMovieId_ = core->r[4];
  ++driver.movieCallCount_;
  lucent::info("str",
               "Spider-Man 1 native STR call {} begin: id={} owner-ra=0x{:08X}",
               driver.movieCallCount_,
               driver.currentMovieId_,
               core->r[31]);
  if (!psx::cpu::completeOrPropagate(*core, driver.movieExecution_.resume(*core))) {
    return;
  }
  lucent::info("str",
               "Spider-Man 1 native STR call {} complete: id={} fields={}",
               driver.movieCallCount_,
               driver.currentMovieId_,
               driver.movieFieldCount_);
}

void spider1_movie_field(Core *core, uint32_t returnPc) {
  Spider1FrameDriver &driver = Spider1FrameDriver::from(*core);
  if (!spider1::isMovieFieldReturn(returnPc) || core->r[31] != returnPc) {
    lucent::error("str",
                  "Spider-Man 1 STR body requested an unauthenticated field boundary: "
                  "argument=0x{:08X} ra=0x{:08X}",
                  returnPc,
                  core->r[31]);
    std::abort();
  }

  // Exact observable VSync(0) result/tail state, owned here without calling libetc VSync. The
  // Retail STR execution yields before the field and resumes only after the host has delivered it.
  const uint32_t returnValue = Spider1FieldClock::vsyncReturnValue(*core);
  ++driver.movieFieldCount_;
  lucent::info("str",
               "Spider-Man 1 native STR field {}: call={} id={} boundary=0x{:08X}",
               driver.movieFieldCount_,
               driver.movieCallCount_,
               driver.currentMovieId_,
               returnPc);
  driver.yieldActiveField(*core, returnPc);
  driver.completeMovieVsync(*core, returnValue);
}

void spider1_stream_wait_field(Core *core) {
  if (!core) {
    lucent::error("cd", "Spider-Man 1 stream wait reached the native field owner without a Core");
    std::abort();
  }
  Spider1FrameDriver &driver = Spider1FrameDriver::from(*core);
  if (driver.fiber_.active()) {
    driver.yieldActiveField(*core, spider1::stGetNextAddress);
    return;
  }
  if (driver.mainFrameInstalled_ || core->pc != spider1::stGetNextAddress) {
    lucent::error("str", "Spider-Man 1 direct stream wait has no boot owner");
    std::abort();
  }
  psx::cpu::requestExecutionExit(*core,
                                 {psx::cpu::ExecutionExitReason::CooperativeYield,
                                  spider1::stGetNextAddress,
                                  0,
                                  "Spider-Man 1 StGetNext waited through one display field"});
}

void Spider1FrameDriver::completeMovieVsync(Core &core, uint32_t returnValue) {
  Spider1FieldClock::recordDeliveredField(core);
  core.r[2] = returnValue;
}

void Spider1FrameDriver::yieldActiveField(Core &core, uint32_t returnPc) {
  fiber_.yieldField(core, returnPc);
}

void Spider1FrameDriver::waitFields(Core &core, uint32_t count) {
  if (!vsyncCallback_) {
    lucent::error("frame",
                  "Spider-Man 1 requested {} field(s) before VSyncCallback registration; the "
                  "native field owner cannot advance",
                  count);
    std::abort();
  }
  for (uint32_t field = 0; field < count; ++field) {
    deliverField(core);
  }
}

void Spider1FrameDriver::waitGuestFields(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  driver.waitFields(*core, core->r[4]);
  core->r[2] = 0;
}

void Spider1FrameDriver::startGpuDmaTimeout(Core *core) {
  // Exact observable body of SLUS_008.75 0x80083C60, except its VSync(-1) query at 0x80083C68.
  // The title field owner advances the same libetc counter at 0x800B397C, so querying it directly
  // preserves the GPU DMA timeout deadline without transferring cadence back to guest libetc.
  const uint32_t deadline =
      core->mem_r32(spider1::libetcVblankCountAddress) + spider1::gpuDmaTimeoutFieldBudget;
  core->r[2] = deadline;
  core->mem_w32(spider1::gpuDmaTimeoutDeadline, deadline);
  core->mem_w32(spider1::gpuDmaTimeoutPollCount, 0);
  psx::cpu::accountGuestInstructions(*core, spider1::gpuDmaTimeoutRetailInstructions);
}

void Spider1FrameDriver::commitSubmittedFrame(Core &core) {
  claimFrameFence("a second presentation");
  game_.presentation.commit(
      &core, static_cast<int>(fieldsSinceCommit_), game_.temporalPresentation.get());
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::commitRepeatedFieldFrame(Core &core) {
  claimFrameFence("a second field presentation");
  // The display scans the previously submitted image for this field. Pace and present it without
  // rotating the interpolation owner's logic-frame history through an empty captured queue.
  game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::commitUnpresentedFrame(Core &core) {
  claimFrameFence("a second frame fence");
  game_.presentation.commitUnpresented(&core);
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::resetGraphWithoutVsync(Core *core) {
  // Exact 0x80084778 body except its VSync(0) at return address 0x8008479C. ResetGraph owns GPU
  // reset sequencing, but it does not own cadence in the native product.
  (void)Spider1GpuReset::reset(*core, core->r[4]);
}

void Spider1FrameDriver::runBootPrefix(Core &core) {
  // Finite prefix of 0x8002C354. The persistent outer selector and every subordinate mode now live
  // in Spider1ModeDriver; no non-returning retail loop is dispatched from this boundary.
  if (mainFrameInstalled_) {
    lucent::error("boot", "Spider-Man 1 finite main frame was installed more than once");
    std::abort();
  }
  // The retail main frame allocates its own window and spills the callee-saved registers into it,
  // so the prefix reproduces that frame exactly before it enters anything.
  core.r[29] -= spider1::bootPrefixFrameBytes;
  for (const BootSavedRegister saved : kBootSavedRegisters) {
    core.mem_w32(core.r[29] + saved.slot, core.r[saved.index]);
  }
  mainFrameInstalled_ = true;
  if (!callGuest(core, spider1::libcEntry, 0x8002C384u)) {
    return;
  }
  core.r[20] = 0;
  core.r[21] = 0;
  beginBoot(core);
}

void Spider1FrameDriver::beginBoot(Core &core) {
  if (bootComplete_) {
    lucent::error("boot", "Spider-Man 1 finite boot fiber was started more than once");
    std::abort();
  }
  fiber_.beginBoot(core, bootHostTurn, spider1::ntscFieldRateMilliHz, [this, &core] {
    core.r[4] = 1;
    (void)callGuest(core, spider1::gameInit, 0x8002C394u);
  });
  fiber_.resume();
  if (fiber_.done()) {
    finishBoot(core);
  } else {
    lucent::info("boot",
                 "Spider-Man 1 finite boot yielded to the native frame owner before its first "
                 "display field");
  }
}

void Spider1FrameDriver::finishBoot(Core &core) {
  fiber_.finish(Spider1FiberPhase::Boot);
  // The C runtime's own register setup, restored so the mode driver starts from the same machine
  // the retail main would have left behind.
  core.r[30] = spider1::crt0GlobalPointer;
  core.r[22] = spider1::crt0StackBase;
  core.r[23] = spider1::kUnattributedCrt0Register23;
  modes_->start(core);
  bootComplete_ = true;
  lucent::info("boot",
               "Spider-Man 1 finite guest prefix complete; native driver owns outer dispatcher "
               "0x8002C354 and all mode loops");
}

void Spider1FrameDriver::beginModeStep(Core &core, uint32_t frame) {
  if (!bootComplete_) {
    lucent::error("frame", "Spider-Man 1 mode fiber started outside the finite gameplay phase");
    std::abort();
  }
  fiber_.beginModeStep(core, [this, &core, frame] {
    modes_->step(core, frame);
  });
  fiber_.resume();
}

void Spider1FrameDriver::stepFrame(Core &core, uint32_t frame) {
  // The elapsed-time host-turn timer exists only to get the non-returning retail boot prefix back
  // to native_boot's frame loop once. Leaving it armed after that handoff creates a second field
  // owner: under real audio work its deadline is already pending when the movie fiber resumes, so
  // the next guest-function entry yields again before decode can reach its authenticated STR
  // boundary. From this first native step onward, only the explicit title field waits below may
  // yield the fiber.
  fiber_.shutdownBootstrapHostTurn();

  game_.timing.logicFrame = frame;
  core.rsub.otAttr.beginLogicFrame(frame);
  game_.pad.serviceFrame();
  // Once per host frame, before any guest work and before this step's presentation fence: the title
  // owns the only publication of its own projection, and the framework's plan is frame-stable by
  // design, so the host canvas has to be re-latched for the aspect the player actually selected and
  // the display extent the guest has actually reached. It re-enters no guest code; see
  // `Spider1Widescreen::synchronizePresentation`.
  Spider1Widescreen::from(core).synchronizePresentation(core);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = false;
  bool deliveredField = false;

  if (fiber_.active()) {
    // A fiber carried in from the previous host step is blocked at a title field boundary.
    // Delivering that field and resuming in the SAME step is what lets guest work run before the
    // presentation pacing; deferring the resume starves the guest. `spider1PlanFiberResume` is the
    // whole rule.
    const Spider1FiberResumePlan resume =
        spider1PlanFiberResume(fiber_.fieldWaitOutstanding(), fiber_.fieldSatisfied());
    if (!resume.valid) {
      lucent::error("frame",
                    "Spider-Man 1 carried a finite fiber across host frames without a field "
                    "boundary");
      std::abort();
    }
    if (resume.deliverField) {
      deliverField(core);
      deliveredField = true;
    }
    fiber_.clearFieldWait();
    if (resume.resume) {
      fiber_.resume();
    }
  } else if (!bootComplete_) {
    lucent::error("boot",
                  "Spider-Man 1 lost its finite boot fiber before initialization completed");
    std::abort();
  } else {
    beginModeStep(core, frame);
  }

  if (fiber_.done()) {
    if (fiber_.phase() == Spider1FiberPhase::Boot) {
      finishBoot(core);
      beginModeStep(core, frame);
    }
    if (fiber_.done() && fiber_.phase() == Spider1FiberPhase::Mode) {
      fiber_.drop();
    }
  }

  // A fiber that reached a NEW field wait during this step owes the host one presentation fence,
  // and may consume a field only if this step has not already delivered one.
  const Spider1FiberYieldPlan yield = spider1PlanFiberYield(
      fiber_.fieldWaitOutstanding(), fiber_.fieldSatisfied(), deliveredField, frameCommitted_);
  if (!yield.valid) {
    lucent::error("frame",
                  "Spider-Man 1 reached an STR/boot field wait after its host fence was already "
                  "committed");
    std::abort();
  }
  if (yield.commit) {
    if (yield.deliverField) {
      deliverField(core);
    }
    if (yield.fieldSatisfied) {
      fiber_.setFieldSatisfied();
    }
    commitMovieField(core);
  }
  if (!frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 mode step {} returned without a frame fence", frame);
    std::abort();
  }
}

} // namespace spider
