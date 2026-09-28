#include "spider1_frame_driver.h"

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

#include <cstdlib>
#include <lucent/log.h>

namespace spider {
namespace {

// SLUS_008.75 service and finite-boot facts. Enter Electro's addresses stay in its own title tree.
constexpr uint32_t kVsyncCallback = 0x8008B8CCu;
constexpr uint32_t kResetGraph = 0x80084778u;
constexpr uint32_t kGuestFieldWait = 0x8005E748u;
constexpr uint32_t kPadService = 0x8006B514u;
constexpr uint32_t kInnerCdSync = 0x8008C944u;
constexpr uint32_t kGpuDmaTimeoutStart = 0x80083C60u;
constexpr uint32_t kLibetcVblankCount = spider1::libetcVblankCountAddress;
constexpr uint32_t kGpuDmaTimeoutDeadline = 0x800B0F64u;
constexpr uint32_t kGpuDmaTimeoutPollCount = 0x800B0F68u;
constexpr uint32_t kGpuStatusPointer = 0x800B0FA0u;
constexpr uint32_t kHorizontalCounterPointer = 0x800B0FA4u;
constexpr uint32_t kHorizontalCounterBaseline = 0x800B0FA8u;
constexpr uint32_t kLastVsyncField = 0x800B0FACu;
constexpr unsigned kNtscFieldRateMilliHz = 59940u;

constexpr uint32_t kLibcEntry = 0x80087444u;
constexpr uint32_t kGameInit = 0x8006BF9Cu;
constexpr uint32_t kMoviePlayer = 0x8002AA0Cu;
constexpr uint32_t kBootTailPadReturn = 0x8006C304u;

constexpr uint32_t kGpuResetBegin = 0x8008C000u;
constexpr uint32_t kGpuReadMode = 0x80084E00u;
constexpr uint32_t kGpuResetProbe = 0x8008C010u;
constexpr uint32_t kGpuResetApply = 0x8008C020u;
constexpr uint32_t kGpuResetStep0 = 0x8008C210u;
constexpr uint32_t kGpuResetStep1 = 0x8008C10Cu;
constexpr uint32_t kGpuResetStep2 = 0x8008C1A0u;
constexpr uint32_t kGpuResetStep3 = 0x8008C030u;
constexpr uint32_t kGpuResetFinalize = 0x800848F0u;

bool call(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  return dispatchGuestOrPropagate(core, entry);
}

uint32_t horizontalCounter(Core &core) {
  const uint32_t pointer = core.mem_r32(kHorizontalCounterPointer);
  return pointer ? core.mem_r32(pointer) : 0u;
}

} // namespace

Spider1FrameDriver::Spider1FrameDriver(Game &game)
    : game_(game),
      modes_(std::make_unique<Spider1ModeDriver>(static_cast<Spider1ModeHost &>(*this))) {}

Spider1FrameDriver::~Spider1FrameDriver() {
  if (hostTurnRegistered_) {
    psx::cpu::shutdownHostTurn();
    hostTurnRegistered_ = false;
  }
  if (activeCoro_ && !activeCoro_->done()) {
    activeCoro_->cancel();
  }
  activeCoro_.reset();
}

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
  if (mainFrameInstalled_ || activeCoro_ || core.r[31] != spider1::resetGraphVsyncReturn ||
      core.r[4] != 0) {
    lucent::error("frame",
                  "Spider-Man 1 refused pre-main VSync: main={} fiber={} ra=0x{:08X} a0=0x{:08X}",
                  mainFrameInstalled_,
                  static_cast<bool>(activeCoro_),
                  core.r[31],
                  core.r[4]);
    std::abort();
  }
  const uint32_t returnValue =
      (horizontalCounter(core) - core.mem_r32(kHorizontalCounterBaseline)) & 0xFFFFu;
  deliverField(core);
  completeMovieVsync(core, returnValue);
  game_.presentation.commitUnpresented(&core);
  core.pc = core.r[31];
  lucent::info("frame", "Spider-Man 1 completed pre-main ResetGraph field at 0x{:08X}", core.pc);
}

void Spider1FrameDriver::serviceBootstrapMovieVsync(Core &core) {
  const uint32_t returnPc = core.r[31];
  if (mainFrameInstalled_ || activeCoro_ || !spider1::isMovieFieldReturn(returnPc) ||
      core.r[4] != 0) {
    lucent::error("str",
                  "Spider-Man 1 refused retail STR field: main={} fiber={} ra=0x{:08X} a0=0x{:08X}",
                  mainFrameInstalled_,
                  static_cast<bool>(activeCoro_),
                  returnPc,
                  core.r[4]);
    std::abort();
  }

  // The direct Lightrec boot has already returned the whole guest CPU state at libetc VSync.
  // Deliver exactly that field, then continue at the call's authentic return PC. The retail movie
  // body, including its stack and decode loop, remains the owner of every instruction around it.
  const uint32_t returnValue =
      (horizontalCounter(core) - core.mem_r32(kHorizontalCounterBaseline)) & 0xFFFFu;
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
  if (mainFrameInstalled_ || activeCoro_ || core.pc != spider1::stGetNextAddress ||
      core.r[2] == 0) {
    lucent::error("str",
                  "Spider-Man 1 refused direct stream wait: main={} fiber={} pc=0x{:08X} v0={}",
                  mainFrameInstalled_,
                  static_cast<bool>(activeCoro_),
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
      game_.core, kGpuDmaTimeoutStart, "Spider GPU DMA timeout", startGpuDmaTimeout);
  installNativeOverride(game_.core, spider1::cdInitAddress, "Spider CdInit", initializeCd);
  if (!game_.platform_hle.register_(kVsyncCallback, captureVsyncCallback)) {
    lucent::error("frame", "Spider-Man 1 could not bind measured VSyncCallback registration");
    std::abort();
  }
  // The directly called stock CdSync body has VSync(-1) timeout polls, while the host CD command
  // completes synchronously. The same complete/ready owner as the public wrapper serves it.
  if (!game_.platform_hle.register_(kInnerCdSync, cd_sync_stock_sync)) {
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
  installNativeOverride(game_.core, kGuestFieldWait, "Spider field wait", waitGuestFields);
  installNativeOverride(game_.core, kPadService, "Spider pad service", serviceBootTail);
  installNativeOverride(game_.core, kMoviePlayer, "Spider movie player", playMovie);
  installNativeOverride(game_.core, kResetGraph, "Spider ResetGraph", resetGraphWithoutVsync);
  // The GPU DMA timeout arm is already installed by the pre-main bootstrap phase.
  // The public CdInit body is already installed by the pre-main bootstrap phase.
  lucent::info(
      "frame",
      "Spider-Man 1 native frame ownership installed: VSync 0x{:08X} remains a frame boundary; "
      "outer dispatcher and all retail mode loops are host-driven",
      spider1::platformServices.vsyncAddress);
}

void Spider1FrameDriver::initializeCd(Core *core) {
  // Exact public success contract of SLUS_008.75 CdInit: install its event callbacks and return
  // true. The host owns every CD operation synchronously, so starting the guest controller reset
  // handshake would be both unobservable and a cadence violation (its IntrWait polls I_STAT bit 0).
  core->mem_w32(spider1::cdSyncCallbackSlot, spider1::cdSyncCallback);
  core->mem_w32(spider1::cdReadyCallbackSlot, spider1::cdReadyCallback);
  core->mem_w32(spider1::cdEventCallbackSlot, spider1::cdEventCallback);
  core->mem_w32(spider1::cdEventUnusedSlot, 0);
  core->r[2] = 1;
  armCdInterrupt(*core);
}

// The mask bit the retail CdInit body reaches, and NOT the reason the CD channel is serviced.
//
// MEASURED 2026-09-27, `scratch/dem1/probe_gate.log`: the controller queued a sector and latched
// IRQ2, and the guest's registered CD interrupt element was never invoked, because
// `Hle::irqPoll` delivers `i_stat & i_mask` and 0x004 & 0x009 is 0. The title added the arm
// (0x009 -> 0x00D), and the gate opened.
//
// MEASURED 2026-09-28, `docs/issues/0026`: **opening the gate was not the fix, and the reasoning
// this comment used to give was wrong on two counts. Both corrections are from the executable's own
// bytes, and the second is the one that matters:**
//
// 1. 0x8008B86C is NOT a B-vector thunk. It is an indirect call through this title's own tables:
//        0x8008B86C  lui $v0,0x800B ; lw $v0,0x390C($v0)   ; $v0 = [0x800B390C] = 0x800B38EC
//        0x8008B87C  lw $v0,8($v0)                          ; $v0 = [0x800B38EC+8] = 0x8008BBD0
//        0x8008B884  jalr $v0                               ; the GUEST's own 0x8008BBD0(2, name)
//    0x800B390C is the title's HARDWARE-ADDRESS table (I_STAT, I_MASK, DPCR), not a BIOS vector
//    table, and 0x8008BBD0 is the guest's own interrupt-source registration into a table at
//    0x800B2888 that is ALL ZEROS in the image and filled at run time. So "on a PSX B-vector entry
//    2 is the interrupt-enable call" is a step that does not exist. The arm below still happens,
//    because it writes the device directly and the device agrees: the conclusion survived, the
//    reason did not.
//
// 2. The registered element is NOT a CD service. 0x800C1528 is `{0, handler 0x80087660, verifier
//    0x800875F8, 0}`, and 0x80087660 issues GP1(0x88) -- a VBlank display-area start. One bounded
//    run, 4,199 presented frames, 88,891,395 executed instructions: the framework handed that
//    element `I_STAT&I_MASK=0x001` 4,044 times and `=0x005` (VBlank + CD-ROM) 15 times, and it
//    correctly declined the CD bit every time. The guest then acked with `I_STAT=0x0FE` 4,059 times
//    from `ra=0x80086D90`, armed DMA channel 3 exactly once (DICR 0x00920000 -> 0x009A0000, armed
//    mask 0x1A), and produced ZERO `DMA3 complete` over the whole run, with `PSXPORT_DEBUG=cdcr`
//    logging 0 reads of the controller's data FIFO at 0x1F801802.
//
// THE ACTUAL CAUSE is not a mask bit. The guest's CD-ROM service is the BIOS hardware event class
// 0xF0000003 (HwCD). Retail CdInit at 0x8008A16C writes 0x8008A238 / 0x8008A260 / 0x8008A288 into
// the BIOS callback slots at 0x800B3B14 / 0x800B3B18 / 0x800B1C7C, and each of those three
// functions is `a0 = 0xF0000003 ; jal 0x8008F9D0` (the B0:0x07 DeliverEvent stub) with a1 = 0x20 or
// 0x40. On hardware the chain is: the controller raises IRQ2 -> the BIOS's OWN CD-ROM interrupt
// handler reads the response -> it calls the function at 0x800B3B18 -> DeliverEvent(0xF0000003,
// ...) -> the libcd handler starts DMA channel 3. `Hle::deliverEvent` has arms for SwCARD
// (0xF4000001), HwCARD (0xF0000011) and HwSPU (0xF0000009) and NONE for HwCD, and the BIOS CD-ROM
// interrupt handler is ROM code this port does not have.
//
// THIS ARM STAYS, and it is still the right device write: it is what makes IRQ2 deliverable at all,
// and the guest does ask for its data (the run's own `cd` channel shows the 71-sector STR read from
// LBA 397). But it is NECESSARY, NOT SUFFICIENT. Do not read this function as the CD-ROM service.
// The framework change that owns the rest is in `docs/issues/0026`, and it is not the title's to
// write: a spider1 override for a BIOS hardware event class would be a second implementation of
// the framework's event owner and would be a tap.
//
// The write goes through the device (0x1F801074), not to the host's mask field, so the framework
// owns the value and re-arms its own delivery gate. The CURRENT mask is read back from the same
// device and OR-ed, because the guest owns the rest of it: a title that wrote a literal here would
// clear whatever VBlank/DMA enables the guest had already asked for.

void Spider1FrameDriver::armCdInterrupt(Core &core) {
  const uint32_t before = core.mem_r32(spider1::interruptMaskRegister);
  const uint32_t after = before | spider1::interruptMaskCdBit;
  if (after == before) {
    lucent::info("cd",
                 "Spider-Man 1 CdInit found the CD interrupt already armed in I_MASK 0x{:03X}",
                 before);
    return;
  }
  core.mem_w32(spider1::interruptMaskRegister, after);
  lucent::info(
      "cd",
      "Spider-Man 1 CdInit armed the CD interrupt: I_MASK 0x{:03X} -> 0x{:03X}. The retail "
      "0x8008D54C reach of the B-vector enable thunk (a0=2) is the effect this override "
      "replaces, and without it Hle::irqPoll computes i_stat & i_mask = 0, so the "
      "guest's own registered CD element never runs",
      before,
      after);
}

void Spider1FrameDriver::serviceBootTail(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  const uint32_t returnPc = core->r[31];
  if (!callOriginalOrPropagate(*core, kPadService)) {
    return;
  }

  // 0x8006C2FC..0x8006C35C is the authenticated post-logo wait in game init. It invokes this pad
  // service once per iteration until the callback-maintained game field count advances by 300 (or
  // input dismisses it). On PSX the display interrupt runs between those calls. A runtime turn
  // otherwise holds the boot fiber and prevents the host from delivering
  // even the first field it is waiting for. Preserve the complete pad-service body and make this
  // exact per-field call site yield to the one native cadence owner.
  if (driver.activePhase_ == ActivePhase::Boot && returnPc == kBootTailPadReturn) {
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
  core.mem_w32(kLibetcVblankCount, core.mem_r32(kLibetcVblankCount) + 1u);
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

void Spider1FrameDriver::commitMovieField(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame",
                  "Spider-Man 1 STR field followed an earlier fence in the same host step");
    std::abort();
  }
  game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = true;
}

void Spider1FrameDriver::bootHostTurn(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  if (driver.activePhase_ != ActivePhase::Boot) {
    lucent::error("boot", "Spider-Man 1 host field turn escaped the finite boot fiber");
    std::abort();
  }
  driver.yieldActiveField(*core, 0);
}

void Spider1FrameDriver::playMovie(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  if (!driver.activeCoro_ || driver.activeCore_ != core) {
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
  const uint32_t returnValue =
      (horizontalCounter(*core) - core->mem_r32(kHorizontalCounterBaseline)) & 0xFFFFu;
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
  if (driver.activeCoro_) {
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
  core.mem_w32(kLastVsyncField, core.mem_r32(kLibetcVblankCount));
  core.mem_w32(kHorizontalCounterBaseline, horizontalCounter(core));
  (void)core.mem_r32(kGpuStatusPointer);
  core.r[2] = returnValue;
}

void Spider1FrameDriver::yieldActiveField(Core &core, uint32_t returnPc) {
  if (!activeCoro_ || activeCore_ != &core || activePhase_ == ActivePhase::None || fieldWaiting_ ||
      fieldSatisfied_) {
    lucent::error("frame",
                  "Spider-Man 1 field yield has no resumable finite owner: phase={} waiting={} "
                  "satisfied={} ra=0x{:08X}",
                  static_cast<unsigned>(activePhase_),
                  fieldWaiting_,
                  fieldSatisfied_,
                  returnPc);
    std::abort();
  }
  fieldWaiting_ = true;
  activeCoro_->yield();
}

void Spider1FrameDriver::resumeActive() {
  if (!activeCoro_ || activeCoro_->done()) {
    lucent::error("frame", "Spider-Man 1 attempted to resume an absent or completed finite fiber");
    std::abort();
  }
  activeCoro_->resume();
  if (!activeCoro_->done() && !fieldWaiting_) {
    lucent::error("frame", "Spider-Man 1 finite fiber returned without a field boundary");
    std::abort();
  }
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
  const uint32_t deadline = core->mem_r32(kLibetcVblankCount) + 240u;
  core->r[2] = deadline;
  core->mem_w32(kGpuDmaTimeoutDeadline, deadline);
  core->mem_w32(kGpuDmaTimeoutPollCount, 0);
  psx::cpu::accountGuestInstructions(*core, 13u);
}

void Spider1FrameDriver::commitSubmittedFrame(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 mode attempted a second presentation in one host step");
    std::abort();
  }
  game_.presentation.commit(
      &core, static_cast<int>(fieldsSinceCommit_), game_.temporalPresentation.get());
  fieldsSinceCommit_ = 0;
  frameCommitted_ = true;
}

void Spider1FrameDriver::commitRepeatedFieldFrame(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame",
                  "Spider-Man 1 mode attempted a second field presentation in one host step");
    std::abort();
  }
  // The display scans the previously submitted image for this field. Pace and present it without
  // rotating the interpolation owner's logic-frame history through an empty captured queue.
  game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = true;
}

void Spider1FrameDriver::commitUnpresentedFrame(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 mode attempted a second frame fence in one host step");
    std::abort();
  }
  game_.presentation.commitUnpresented(&core);
  fieldsSinceCommit_ = 0;
  frameCommitted_ = true;
}

void Spider1FrameDriver::resetGraphWithoutVsync(Core *core) {
  // Exact 0x80084778 body except its VSync(0) at return address 0x8008479C. ResetGraph owns GPU
  // reset sequencing, but it does not own cadence in the native product.
  uint32_t mode = core->r[4];
  if (!call(*core, kGpuResetBegin, 0x80084794u) || !call(*core, kGpuReadMode, 0x800847A4u)) {
    return;
  }
  const uint32_t priorMode = core->r[2];
  if (!call(*core, kGpuResetProbe, 0x800847ACu)) {
    return;
  }
  if (core->r[2] == 0) {
    mode = 0;
  }
  core->r[4] = mode;
  if (!call(*core, kGpuResetApply, 0x800847C0u) || !call(*core, kGpuResetStep0, 0x800847C8u) ||
      !call(*core, kGpuResetStep1, 0x800847D0u) || !call(*core, kGpuResetStep2, 0x800847D8u) ||
      !call(*core, kGpuResetStep3, 0x800847E0u)) {
    return;
  }
  if (priorMode == 1) {
    (void)call(*core, kGpuResetFinalize, 0x800847F4u);
  }
}

void Spider1FrameDriver::runBootPrefix(Core &core) {
  // Finite prefix of 0x8002C354. The persistent outer selector and every subordinate mode now live
  // in Spider1ModeDriver; no non-returning retail loop is dispatched from this boundary.
  if (mainFrameInstalled_) {
    lucent::error("boot", "Spider-Man 1 finite main frame was installed more than once");
    std::abort();
  }
  core.r[29] -= 72u;
  core.mem_w32(core.r[29] + 68u, core.r[31]);
  core.mem_w32(core.r[29] + 64u, core.r[30]);
  core.mem_w32(core.r[29] + 60u, core.r[23]);
  core.mem_w32(core.r[29] + 56u, core.r[22]);
  core.mem_w32(core.r[29] + 52u, core.r[21]);
  core.mem_w32(core.r[29] + 48u, core.r[20]);
  core.mem_w32(core.r[29] + 44u, core.r[19]);
  core.mem_w32(core.r[29] + 40u, core.r[18]);
  core.mem_w32(core.r[29] + 36u, core.r[17]);
  core.mem_w32(core.r[29] + 32u, core.r[16]);
  mainFrameInstalled_ = true;
  if (!call(core, kLibcEntry, 0x8002C384u)) {
    return;
  }
  core.r[20] = 0;
  core.r[21] = 0;
  beginBoot(core);
}

void Spider1FrameDriver::beginBoot(Core &core) {
  if (activeCoro_ || bootComplete_) {
    lucent::error("boot", "Spider-Man 1 finite boot fiber was started more than once");
    std::abort();
  }
  psx::cpu::registerHostTurn(core, bootHostTurn, kNtscFieldRateMilliHz);
  hostTurnRegistered_ = true;
  activeCore_ = &core;
  activePhase_ = ActivePhase::Boot;
  activeCoro_ = std::make_unique<Coro>();
  activeCoro_->start([this, &core] {
    core.r[4] = 1;
    (void)call(core, kGameInit, 0x8002C394u);
  });
  resumeActive();
  if (activeCoro_->done()) {
    finishBoot(core);
  } else {
    lucent::info("boot",
                 "Spider-Man 1 finite boot yielded to the native frame owner before its first "
                 "display field");
  }
}

void Spider1FrameDriver::finishBoot(Core &core) {
  if (!activeCoro_ || !activeCoro_->done() || activePhase_ != ActivePhase::Boot || fieldWaiting_ ||
      fieldSatisfied_) {
    lucent::error("boot", "Spider-Man 1 attempted to finish an incomplete boot fiber");
    std::abort();
  }
  psx::cpu::shutdownHostTurn();
  hostTurnRegistered_ = false;
  activeCoro_.reset();
  activeCore_ = nullptr;
  activePhase_ = ActivePhase::None;
  core.r[30] = 0x800B0000u;
  core.r[22] = 0x800A0000u;
  core.r[23] = 0x80090000u;
  modes_->start(core);
  bootComplete_ = true;
  lucent::info("boot",
               "Spider-Man 1 finite guest prefix complete; native driver owns outer dispatcher "
               "0x8002C354 and all mode loops");
}

void Spider1FrameDriver::beginModeStep(Core &core, uint32_t frame) {
  if (activeCoro_ || !bootComplete_) {
    lucent::error("frame", "Spider-Man 1 mode fiber started outside the finite gameplay phase");
    std::abort();
  }
  activeCore_ = &core;
  activePhase_ = ActivePhase::Mode;
  activeCoro_ = std::make_unique<Coro>();
  activeCoro_->start([this, &core, frame] {
    modes_->step(core, frame);
  });
  resumeActive();
}

void Spider1FrameDriver::stepFrame(Core &core, uint32_t frame) {
  // The elapsed-time host-turn timer exists only to get the non-returning retail boot prefix back
  // to native_boot's frame loop once. Leaving it armed after that handoff creates a second field
  // owner: under real audio work its deadline is already pending when the movie fiber resumes, so
  // the next guest-function entry yields again before decode can reach its authenticated STR
  // boundary. From this first native step onward, only the explicit title field waits below may
  // yield the fiber.
  if (hostTurnRegistered_) {
    psx::cpu::shutdownHostTurn();
    hostTurnRegistered_ = false;
    lucent::info("hostturn",
                 "Spider-Man 1 bootstrap turn complete; native frame driver now owns every field");
  }

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

  if (activeCoro_) {
    const Spider1FiberResumePlan resume = spider1PlanFiberResume(fieldWaiting_, fieldSatisfied_);
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
    fieldWaiting_ = false;
    fieldSatisfied_ = false;
    if (resume.resume) {
      resumeActive();
    }
  } else if (!bootComplete_) {
    lucent::error("boot",
                  "Spider-Man 1 lost its finite boot fiber before initialization completed");
    std::abort();
  } else {
    beginModeStep(core, frame);
  }

  if (activeCoro_ && activeCoro_->done()) {
    if (activePhase_ == ActivePhase::Boot) {
      finishBoot(core);
      beginModeStep(core, frame);
    }
    if (activeCoro_ && activeCoro_->done() && activePhase_ == ActivePhase::Mode) {
      activeCoro_.reset();
      activeCore_ = nullptr;
      activePhase_ = ActivePhase::None;
    }
  }

  const Spider1FiberYieldPlan yield =
      spider1PlanFiberYield(fieldWaiting_, fieldSatisfied_, deliveredField, frameCommitted_);
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
    fieldSatisfied_ = yield.fieldSatisfied;
    commitMovieField(core);
  }
  if (!frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 mode step {} returned without a frame fence", frame);
    std::abort();
  }
}

} // namespace spider
