#pragma once

#include "game_runtime.h"
#include "spider1_host_stepped_fiber.h"
#include "spider1_mode_driver.h"
#include "spider1_movie_execution.h"

#include <cstdint>
#include <memory>
#include <string_view>

class Core;
class Game;

namespace spider {

// Spider-Man 1's cadence/service owner. Spider1ModeDriver owns the retail outer selector and mode
// states; all guest addresses deliberately live with this title, never Enter Electro.
//
// WHAT THE HOST-STEPPED FIBER IS, AND WHY IT IS NOT IN THIS HEADER. The retail boot prefix and the
// mode loops do not return, so the port runs them on a fiber that yields at a title display-field
// boundary. That fiber's whole lifecycle — its two phases, the outstanding field wait, the yield,
// the resume, the single handoff from the boot host turn to the native field owner — belongs to
// `Spider1HostSteppedFiber`, and this class asks it for those facts rather than holding them
// itself. What stays here is what only this class knows: field delivery, the presentation fences,
// and the mode driver.
class Spider1FrameDriver final : public FrameDriver, private Spider1ModeHost {
public:
  explicit Spider1FrameDriver(Game &game);
  ~Spider1FrameDriver() override;

  void installOverrides();
  void installBootstrapOverrides();
  void serviceBootstrapVsync(Core &core);
  void serviceBootstrapMovieVsync(Core &core);
  void serviceBootstrapStreamWait(Core &core);
  void runBootPrefix(Core &core);
  void stepFrame(Core &core, uint32_t frame) override;
  static Spider1FrameDriver &from(Core &core);

private:
  void waitFields(Core &core, uint32_t count) override;
  void commitSubmittedFrame(Core &core) override;
  void commitRepeatedFieldFrame(Core &core) override;
  void commitUnpresentedFrame(Core &core) override;
  void deliverField(Core &core);
  void commitMovieField(Core &core);
  // Claim this host step's single presentation fence, or refuse. EVERY commit in this class goes
  // through here, because "exactly one fence per host step" is the invariant the whole cadence
  // rests on, and a second fence is always a bug rather than a detail.
  void claimFrameFence(std::string_view what);
  void registerVsyncCallback(uint32_t callback);
  void beginBoot(Core &core);
  void beginModeStep(Core &core, uint32_t frame);
  void finishBoot(Core &core);
  void yieldActiveField(Core &core, uint32_t returnPc);
  void completeMovieVsync(Core &core, uint32_t returnValue);

  static void bootHostTurn(Core *core);
  static void captureVsyncCallback(Core *core);
  static void initializeCd(Core *core);
  static void serviceBootTail(Core *core);
  static void waitGuestFields(Core *core);
  static void playMovie(Core *core);
  static void resetGraphWithoutVsync(Core *core);
  static void startGpuDmaTimeout(Core *core);

  friend void spider1_movie_field(Core *core, uint32_t returnPc);
  friend void spider1_stream_wait_field(Core *core);

  Game &game_;
  std::unique_ptr<Spider1ModeDriver> modes_;
  Spider1MovieExecution movieExecution_;
  Spider1HostSteppedFiber fiber_;
  uint32_t vsyncCallback_ = 0;
  uint32_t movieCallCount_ = 0;
  uint32_t movieFieldCount_ = 0;
  uint32_t currentMovieId_ = 0;
  uint32_t fieldsSinceCommit_ = 0;
  bool frameCommitted_ = false;
  bool mainFrameInstalled_ = false;
  bool bootComplete_ = false;
};

} // namespace spider
