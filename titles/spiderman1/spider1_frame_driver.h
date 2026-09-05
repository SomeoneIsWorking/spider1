#pragma once

#include "coro.h"
#include "game_runtime.h"
#include "spider1_mode_driver.h"
#include "spider1_movie_execution.h"

#include <cstdint>
#include <memory>

class Core;
class Game;

namespace spider {

// Spider-Man 1's cadence/service owner. Spider1ModeDriver owns the retail outer selector and mode
// states; all guest addresses deliberately live with this title, never Enter Electro.
class Spider1FrameDriver final : public FrameDriver, private Spider1ModeHost {
public:
  explicit Spider1FrameDriver(Game &game);
  ~Spider1FrameDriver() override;

  void installOverrides();
  void runBootPrefix(Core &core);
  void stepFrame(Core &core, uint32_t frame) override;

private:
  enum class ActivePhase { None, Boot, Mode };

  void waitFields(Core &core, uint32_t count) override;
  void commitSubmittedFrame(Core &core) override;
  void commitRepeatedFieldFrame(Core &core) override;
  void commitUnpresentedFrame(Core &core) override;
  void deliverField(Core &core);
  void commitMovieField(Core &core);
  void registerVsyncCallback(uint32_t callback);
  void beginBoot(Core &core);
  void beginModeStep(Core &core, uint32_t frame);
  void resumeActive();
  void finishBoot(Core &core);
  void yieldActiveField(Core &core, uint32_t returnPc);
  void completeMovieVsync(Core &core, uint32_t returnValue);

  static Spider1FrameDriver &from(Core &core);
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
  std::unique_ptr<Coro> activeCoro_;
  Spider1MovieExecution movieExecution_;
  Core *activeCore_ = nullptr;
  ActivePhase activePhase_ = ActivePhase::None;
  uint32_t vsyncCallback_ = 0;
  uint32_t movieCallCount_ = 0;
  uint32_t movieFieldCount_ = 0;
  uint32_t currentMovieId_ = 0;
  uint32_t fieldsSinceCommit_ = 0;
  bool frameCommitted_ = false;
  bool mainFrameInstalled_ = false;
  bool hostTurnRegistered_ = false;
  bool bootComplete_ = false;
  bool fieldWaiting_ = false;
  bool fieldSatisfied_ = false;
};

} // namespace spider
