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

namespace spider::spider1 {
namespace {

// Enter `entry` as if the guest had `jal`'d it from `returnPc`; false means the host must stop.
// Unlike `enterGuestCall`, a bounded exit propagates because the field boundary is this driver's
// cadence.
bool callGuest(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  return dispatchGuestOrPropagate(core, entry);
}

// Callee-saved registers the boot prefix spills into the retail main frame, highest register first.
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
  // ResetGraph's early VSync(0) runs before the boot fiber exists; nothing is drawn yet, so the
  // fence is unpresented.
  if (mainFrameInstalled_ || fiber_.active() || core.r[31] != resetGraphVsyncReturn ||
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
  if (mainFrameInstalled_ || fiber_.active() || !isMovieFieldReturn(returnPc) || core.r[4] != 0) {
    lucent::error("str",
                  "Spider-Man 1 refused retail STR field: main={} fiber={} ra=0x{:08X} a0=0x{:08X}",
                  mainFrameInstalled_,
                  fiber_.active(),
                  returnPc,
                  core.r[4]);
    std::abort();
  }

  // The guest stopped at libetc VSync; deliver that field and resume at the call's return PC.
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
  // Start the next logic frame only after presenting this one, so its OT attributes survive the
  // fence.
  ++game_.timing.logicFrame;
  core.rsub.otAttr.beginLogicFrame(game_.timing.logicFrame);
  game_.pad.serviceFrame();
  ++movieFieldCount_;
  core.pc = returnPc;
  lucent::info(
      "str", "Spider-Man 1 resumed retail STR field {} at 0x{:08X}", movieFieldCount_, core.pc);
}

void Spider1FrameDriver::serviceBootstrapStreamWait(Core &core) {
  // StGetNext already returned "not ready"; supply the field that passes while the drive catches
  // up. Unlike VSync this updates neither the VSync return value nor the horizontal-counter
  // baseline.
  if (mainFrameInstalled_ || fiber_.active() || core.pc != stGetNextAddress || core.r[2] == 0) {
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
  // Stock GPU DMA timeout arm; its VSync(-1) just reads the title field count.
  installNativeOverride(
      game_.core, gpuDmaTimeoutStart, "Spider GPU DMA timeout", startGpuDmaTimeout);
  installNativeOverride(game_.core, cdInitAddress, "Spider CdInit", initializeCd);
  if (!game_.platform_hle.register_(vsyncCallbackRegistration, captureVsyncCallback)) {
    lucent::error("frame", "Spider-Man 1 could not bind measured VSyncCallback registration");
    std::abort();
  }
  // The stock CdSync body polls VSync(-1) while the host CD command completes synchronously.
  if (!game_.platform_hle.register_(innerCdSync, cd_sync_stock_sync)) {
    lucent::error("cd", "Spider-Man 1 could not bind the measured inner CdSync service");
    std::abort();
  }
}

void Spider1FrameDriver::installOverrides() {
  // VSync is absent here: PlatformHle installs the framework's frame boundary from the title's
  // platform facts.
  installNativeOverride(game_.core, guestFieldWait, "Spider field wait", waitGuestFields);
  installNativeOverride(game_.core, padRead, "Spider pad service", serviceBootTail);
  installNativeOverride(game_.core, moviePlayer, "Spider movie player", playMovie);
  installNativeOverride(
      game_.core, Spider1GpuReset::resetGraphEntry, "Spider ResetGraph", resetGraphWithoutVsync);
  lucent::info(
      "frame",
      "Spider-Man 1 native frame ownership installed: VSync 0x{:08X} remains a frame boundary; "
      "outer dispatcher and all retail mode loops are host-driven",
      platformServices.vsyncAddress);
}

void Spider1FrameDriver::initializeCd(Core *core) {
  Spider1CdInitialization::install(*core);
}

void Spider1FrameDriver::serviceBootTail(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  const uint32_t returnPc = core->r[31];
  if (!callOriginalOrPropagate(*core, padRead)) {
    return;
  }

  // 0x8006C2FC..0x8006C35C is the post-logo wait in game init; it calls this pad service once per
  // field, and yielding there keeps a runtime turn from starving the host of its first field.
  if (driver.fiber_.phase() == Spider1FiberPhase::Boot && returnPc == bootTailPadReturn) {
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
  core.mem_w32(libetcVblankCountAddress, core.mem_r32(libetcVblankCountAddress) + 1u);
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

void Spider1FrameDriver::streamWaitField(Core *core) {
  if (!core) {
    lucent::error("cd", "Spider-Man 1 stream wait reached the native field owner without a Core");
    std::abort();
  }
  Spider1FrameDriver &driver = Spider1FrameDriver::from(*core);
  if (driver.fiber_.active()) {
    driver.yieldActiveField(*core, stGetNextAddress);
    return;
  }
  if (driver.mainFrameInstalled_ || core->pc != stGetNextAddress) {
    lucent::error("str", "Spider-Man 1 direct stream wait has no boot owner");
    std::abort();
  }
  psx::cpu::requestExecutionExit(*core,
                                 {psx::cpu::ExecutionExitReason::CooperativeYield,
                                  stGetNextAddress,
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
  // SLUS_008.75 0x80083C60 minus its VSync(-1) query at 0x80083C68; the title owns the counter at
  // 0x800B397C.
  const uint32_t deadline = core->mem_r32(libetcVblankCountAddress) + gpuDmaTimeoutFieldBudget;
  core->r[2] = deadline;
  core->mem_w32(gpuDmaTimeoutDeadline, deadline);
  core->mem_w32(gpuDmaTimeoutPollCount, 0);
  psx::cpu::accountGuestInstructions(*core, gpuDmaTimeoutRetailInstructions);
}

void Spider1FrameDriver::commitSubmittedFrame(Core &core) {
  claimFrameFence("a second presentation");
  game_.presentation.commit(
      &core, static_cast<int>(fieldsSinceCommit_), game_.temporalPresentation.get());
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::commitRepeatedFieldFrame(Core &core) {
  claimFrameFence("a second field presentation");
  // The display rescans the last submitted image; present it without touching the interpolation
  // history.
  game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::commitUnpresentedFrame(Core &core) {
  claimFrameFence("a second frame fence");
  game_.presentation.commitUnpresented(&core);
  fieldsSinceCommit_ = 0;
}

void Spider1FrameDriver::resetGraphWithoutVsync(Core *core) {
  // 0x80084778 minus its VSync(0) at return address 0x8008479C.
  (void)Spider1GpuReset::reset(*core, core->r[4]);
}

void Spider1FrameDriver::runBootPrefix(Core &core) {
  // Finite prefix of 0x8002C354; the outer selector and modes live in Spider1ModeDriver.
  if (mainFrameInstalled_) {
    lucent::error("boot", "Spider-Man 1 finite main frame was installed more than once");
    std::abort();
  }
  // The retail main frame spills callee-saved registers into its own window; reproduce it first.
  core.r[29] -= bootPrefixFrameBytes;
  for (const BootSavedRegister saved : kBootSavedRegisters) {
    core.mem_w32(core.r[29] + saved.slot, core.r[saved.index]);
  }
  mainFrameInstalled_ = true;
  if (!callGuest(core, libcEntry, 0x8002C384u)) {
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
  fiber_.beginBoot(core, bootHostTurn, ntscFieldRateMilliHz, [this, &core] {
    core.r[4] = 1;
    (void)callGuest(core, gameInit, 0x8002C394u);
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
  // crt0 register setup the retail main would have left for the mode driver.
  core.r[30] = crt0GlobalPointer;
  core.r[22] = crt0StackBase;
  core.r[23] = kUnattributedCrt0Register23;
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
  // The boot host turn returns the non-returning prefix to the frame loop once; left armed it would
  // be a second field owner, so after this only the title's field waits may yield the fiber.
  fiber_.shutdownBootstrapHostTurn();

  game_.timing.logicFrame = frame;
  core.rsub.otAttr.beginLogicFrame(frame);
  game_.pad.serviceFrame();
  // Re-latch the host canvas for the selected aspect and reached display extent; runs no guest
  // code.
  Spider1Widescreen::from(core).synchronizePresentation(core);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = false;
  bool deliveredField = false;

  if (fiber_.active()) {
    // A fiber carried over is blocked at a field boundary; deliver and resume in this same step so
    // guest work runs before presentation pacing.
    const Spider1FiberResumePlan resume =
        planFiberResume(fiber_.fieldWaitOutstanding(), fiber_.fieldSatisfied());
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

  // A fiber at a new field wait owes one fence and may consume a field only if none was delivered
  // this step.
  const Spider1FiberYieldPlan yield = planFiberYield(
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

} // namespace spider::spider1
