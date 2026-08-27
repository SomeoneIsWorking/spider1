#include "spider1_frame_driver.h"

#include "cd_control.h"
#include "core.h"
#include "game.h"
#include "guest_frame_fallback.h"
#include "recomp_iface.h"
#include "render_seam.h"
#include "spider1_field_schedule.h"
#include "spider1_stream_driver.h"

#include <cstdlib>
#include <lucent/log.h>

// The original per-field pad service. This remains compiled and is super-called by the title
// boundary below; the native frame owner changes only who advances the field clock.
extern void gen_func_8006B514(Core *core);

namespace spider {
namespace {

// SLUS_008.75 service and finite-boot facts. Enter Electro's addresses stay in its own title tree.
constexpr uint32_t kVsync = 0x80084BE0u;
constexpr uint32_t kVsyncCallback = 0x8008B8CCu;
constexpr uint32_t kResetGraph = 0x80084778u;
constexpr uint32_t kGuestFieldWait = 0x8005E748u;
constexpr uint32_t kPadService = 0x8006B514u;
constexpr uint32_t kInnerCdSync = 0x8008C944u;
constexpr uint32_t kGpuDmaTimeoutStart = 0x80083C60u;
constexpr uint32_t kLibetcVblankCount = 0x800B397Cu;
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

void call(Core &core, uint32_t entry, uint32_t returnPc) {
  core.r[31] = returnPc;
  rec_dispatch(&core, entry);
}

uint32_t horizontalCounter(Core &core) {
  const uint32_t pointer = core.mem_r32(kHorizontalCounterPointer);
  return pointer ? core.mem_r32(pointer) : 0u;
}

bool isMovieFieldReturn(uint32_t returnPc) {
  return returnPc == 0x8002AC8Cu || returnPc == 0x8002AE1Cu || returnPc == 0x8002AFECu;
}

void requireRecompOverride(const RecompRegistry *registry, uint32_t address, RecOverrideFn fn) {
  if (!registry || !registry->shard_set_override || !registry->rec_func_index ||
      registry->rec_func_index(address) < 0) {
    lucent::error("frame",
                  "Spider-Man 1 cannot install the required title override at 0x{:08X}; the "
                  "authenticated substrate does not contain that measured entry",
                  address);
    std::abort();
  }
  registry->shard_set_override(address, fn);
}

} // namespace

Spider1FrameDriver::Spider1FrameDriver(Game &game)
    : game_(game),
      modes_(std::make_unique<Spider1ModeDriver>(static_cast<Spider1ModeHost &>(*this))) {}

Spider1FrameDriver::~Spider1FrameDriver() {
  if (hostTurnRegistered_) {
    rec_host_turn_shutdown();
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

void Spider1FrameDriver::installOverrides() {
  // VSync itself is deliberately absent. GameConfig::hle.vsyncTrap declares 0x80084BE0 and
  // PlatformHle installs the framework's protected all-mode abort. Generated mode supers remain
  // compiled, while this driver owns their loop state and the two engine boundaries that would
  // otherwise require a successful VSync.
  game_.platform_hle.register_(kVsyncCallback, captureVsyncCallback);
  // FUN_80086F18 calls stock libcd's inner CdSync body directly after the final STR field. Its
  // observable success contract is the same complete/ready result as the public wrapper already
  // owned by Cd::overridesInit; running the body would enter two VSync(-1) timeout polls even
  // though every command in this product completes synchronously on the host.
  game_.platform_hle.register_(kInnerCdSync, cd_sync_stock_sync);
  const RecompRegistry *registry = psxport_recomp();
  requireRecompOverride(registry, kGuestFieldWait, waitGuestFields);
  requireRecompOverride(registry, kPadService, serviceBootTail);
  requireRecompOverride(registry, kMoviePlayer, playMovie);
  requireRecompOverride(registry, kResetGraph, resetGraphWithoutVsync);
  requireRecompOverride(registry, kGpuDmaTimeoutStart, startGpuDmaTimeout);
  if (!game_.core.cfg || !game_.core.cfg->cdInit) {
    lucent::error("frame", "Spider-Man 1 has no measured public CdInit boundary");
    std::abort();
  }
  requireRecompOverride(registry, game_.core.cfg->cdInit, initializeCd);
  lucent::info("frame",
               "Spider-Man 1 native frame ownership installed: VSync 0x{:08X} remains trapped; "
               "outer dispatcher and all retail mode loops are host-driven",
               kVsync);
}

void Spider1FrameDriver::initializeCd(Core *core) {
  // Exact public success contract of SLUS_008.75 CdInit: install its event callbacks and return
  // true. The host owns every CD operation synchronously, so starting the guest controller/IRQ
  // handshake would be both unobservable and a cadence violation (its timeout polls VSync(-1)).
  core->game->cd.hleInit();
  core->r[2] = 1;
}

void Spider1FrameDriver::serviceBootTail(Core *core) {
  Spider1FrameDriver &driver = from(*core);
  const uint32_t returnPc = core->r[31];
  gen_func_8006B514(core);

  // 0x8006C2FC..0x8006C35C is the authenticated post-logo wait in game init. It invokes this pad
  // service once per iteration until the callback-maintained game field count advances by 300 (or
  // input dismisses it). On PSX the display interrupt runs between those calls. Under a static
  // recomp, the whole loop otherwise holds the boot fiber and prevents the host from delivering
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
    rec_dispatch(&core, vsyncCallback_);
    static_cast<R3000 &>(core) = saved;
  }
  rec_host_turn_field_delivered(&core);
}

void Spider1FrameDriver::commitMovieField(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame",
                  "Spider-Man 1 STR field followed an earlier fence in the same host step");
    std::abort();
  }
  if (game_.diff_mode) {
    game_.presentation.commitUnpresented(&core);
  } else {
    game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  }
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
  spider1_native_movie_body(core);
  lucent::info("str",
               "Spider-Man 1 native STR call {} complete: id={} fields={}",
               driver.movieCallCount_,
               driver.currentMovieId_,
               driver.movieFieldCount_);
}

void spider1_movie_field(Core *core, uint32_t returnPc) {
  Spider1FrameDriver &driver = Spider1FrameDriver::from(*core);
  if (!isMovieFieldReturn(returnPc) || core->r[31] != returnPc) {
    lucent::error("str",
                  "Spider-Man 1 STR body requested an unauthenticated field boundary: "
                  "argument=0x{:08X} ra=0x{:08X}",
                  returnPc,
                  core->r[31]);
    std::abort();
  }

  // Exact observable VSync(0) result/tail state, owned here without calling libetc VSync. The
  // generated STR body yields before the field and resumes only after the host has delivered it.
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
  driver.yieldActiveField(*core, 0x80086B10u);
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
  rec_guest_instruction_ticks(core, 13u);
}

void Spider1FrameDriver::commitSubmittedFrame(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame", "Spider-Man 1 mode attempted a second presentation in one host step");
    std::abort();
  }
  const bool guestFallback = spiderman_take_guest_frame_fallback();
  if (game_.diff_mode) {
    game_.presentation.commitUnpresented(&core);
  } else if (guestFallback) {
    GuestFrameFallbackModeScope pureGuestPresentation(core.rsub.mode);
    game_.presentation.commit(
        &core, static_cast<int>(fieldsSinceCommit_), game_.temporalPresentation.get());
  } else {
    game_.presentation.commit(
        &core, static_cast<int>(fieldsSinceCommit_), game_.temporalPresentation.get());
  }
  fieldsSinceCommit_ = 0;
  frameCommitted_ = true;
}

void Spider1FrameDriver::commitRepeatedFieldFrame(Core &core) {
  if (frameCommitted_) {
    lucent::error("frame",
                  "Spider-Man 1 mode attempted a second field presentation in one host step");
    std::abort();
  }
  if (game_.diff_mode) {
    game_.presentation.commitUnpresented(&core);
  } else {
    // The display scans the previously submitted image for this field. Pace and present it without
    // rotating the interpolation owner's logic-frame history through an empty captured queue.
    game_.presentation.commit(&core, static_cast<int>(fieldsSinceCommit_), nullptr);
  }
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
  call(*core, kGpuResetBegin, 0x80084794u);
  call(*core, kGpuReadMode, 0x800847A4u);
  const uint32_t priorMode = core->r[2];
  call(*core, kGpuResetProbe, 0x800847ACu);
  if (core->r[2] == 0) {
    mode = 0;
  }
  core->r[4] = mode;
  call(*core, kGpuResetApply, 0x800847C0u);
  call(*core, kGpuResetStep0, 0x800847C8u);
  call(*core, kGpuResetStep1, 0x800847D0u);
  call(*core, kGpuResetStep2, 0x800847D8u);
  call(*core, kGpuResetStep3, 0x800847E0u);
  if (priorMode == 1) {
    call(*core, kGpuResetFinalize, 0x800847F4u);
  }
}

void Spider1FrameDriver::runBootPrefix(Core &core) {
  // Finite prefix of 0x8002C354. The persistent outer selector and every subordinate mode now live
  // in Spider1ModeDriver; no generated non-returning loop is dispatched from this boundary.
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
  call(core, kLibcEntry, 0x8002C384u);
  core.r[20] = 0;
  core.r[21] = 0;
  beginBoot(core);
}

void Spider1FrameDriver::beginBoot(Core &core) {
  if (activeCoro_ || bootComplete_) {
    lucent::error("boot", "Spider-Man 1 finite boot fiber was started more than once");
    std::abort();
  }
  rec_host_turn_register(&core, bootHostTurn, kNtscFieldRateMilliHz);
  hostTurnRegistered_ = true;
  activeCore_ = &core;
  activePhase_ = ActivePhase::Boot;
  activeCoro_ = std::make_unique<Coro>();
  activeCoro_->start([this, &core] {
    core.r[4] = 1;
    call(core, kGameInit, 0x8002C394u);
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
  rec_host_turn_shutdown();
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
  // the next generated-function entry yields again before decode can reach its authenticated STR
  // boundary. From this first native step onward, only the explicit title field waits below may
  // yield the fiber.
  if (hostTurnRegistered_) {
    rec_host_turn_shutdown();
    hostTurnRegistered_ = false;
    lucent::info("hostturn",
                 "Spider-Man 1 bootstrap turn complete; native frame driver now owns every field");
  }

  game_.timing.logicFrame = frame;
  game_.pad.serviceFrame();
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
