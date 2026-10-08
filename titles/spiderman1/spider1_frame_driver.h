#pragma once

#include "game_runtime.h"
#include "spider1_bootstrap_turn.h"
#include "spider1_host_stepped_fiber.h"
#include "spider1_mode_driver.h"
#include "spider1_movie_execution.h"

#include <cstdint>
#include <memory>
#include <string_view>

class Core;
class Game;

namespace spider::spider1 {

// Spider-Man 1 cadence/service owner. Spider1ModeDriver owns the retail outer selector and modes;
// the boot prefix and mode loops run on `Spider1HostSteppedFiber`, which yields at field
// boundaries.
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
  void beginGuest(Core &core);
  void stepFrame(Core &core, uint32_t frame) override;
  static Spider1FrameDriver &from(Core &core);

  // The CD boundary's request for a display field when StGetNext answers "not ready"; the fiber's
  // field boundary while a fiber runs, else a cooperative exit the bootstrap path resumes.
  static void streamWaitField(Core *core);

private:
  void waitFields(Core &core, uint32_t count) override;
  void commitSubmittedFrame(Core &core) override;
  void commitRepeatedFieldFrame(Core &core) override;
  void commitUnpresentedFrame(Core &core) override;
  void deliverField(Core &core);
  void commitMovieField(Core &core);
  // Claim this host step's single presentation fence, or refuse; every commit goes through here.
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

  Game &game_;
  Spider1BootstrapTurn turn_;
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

} // namespace spider::spider1
