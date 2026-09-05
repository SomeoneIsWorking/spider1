#pragma once

#include <cstdint>

class Core;

namespace spider {

// Exact selector routes from SLUS_008.75's jump table at 0x80093C3C. These are title facts, not
// Neversoft-lineage policy; Enter Electro must derive its own table and mode driver.
enum class Spider1OuterRoute {
  Invalid,
  RestartWithModeFive,
  Menu,
  Level,
  CycleSelection,
  ResourcePrimary,
  TransitionThenOuter,
  ResetThenPrimary,
  TransitionThenOuterWithFlag,
};

constexpr Spider1OuterRoute spider1OuterRoute(uint32_t selector) {
  switch (selector) {
  case 1:
    return Spider1OuterRoute::RestartWithModeFive;
  case 2:
  case 9:
    return Spider1OuterRoute::Menu;
  case 3:
    return Spider1OuterRoute::Level;
  case 4:
  case 5:
    return Spider1OuterRoute::CycleSelection;
  case 6:
    return Spider1OuterRoute::ResourcePrimary;
  case 7:
    return Spider1OuterRoute::TransitionThenOuter;
  case 8:
    return Spider1OuterRoute::ResetThenPrimary;
  case 10:
    return Spider1OuterRoute::TransitionThenOuterWithFlag;
  default:
    return Spider1OuterRoute::Invalid;
  }
}

class Spider1ModeHost {
public:
  virtual ~Spider1ModeHost() = default;

  virtual void waitFields(Core &core, uint32_t count) = 0;
  virtual void commitSubmittedFrame(Core &core) = 0;
  virtual void commitRepeatedFieldFrame(Core &core) = 0;
  virtual void commitUnpresentedFrame(Core &core) = 0;
};

// Persistent native owner of Spider-Man 1's outer selector and its three subordinate retail mode
// functions. Every step is finite and reaches exactly one host presentation or unpresented fence.
// Synchronous guest leaves execute through the per-Core runtime boundary.
class Spider1ModeDriver final {
public:
  explicit Spider1ModeDriver(Spider1ModeHost &host);

  void start(Core &core);
  void step(Core &core, uint32_t frame);

private:
  enum class State {
    Dormant,
    AwaitOuterReady,
    PrimaryWarmup,
    PrimaryFrame,
    TransitionFirstFrame,
    TransitionSecondFrame,
    AwaitMenuReady,
    MenuFrame,
    AwaitMenuExit,
    AwaitLevelReady,
    AlternateFrame,
    InvalidAwaitReady,
    InvalidInput,
  };

  void enterOuterCycle(Core &core);
  void readOuterArgumentsAndPreparePrimary(Core &core);
  void initializePrimary(Core &core);
  void stepPrimaryWarmup(Core &core);
  void stepPrimary(Core &core);
  void finishPrimary(Core &core);
  void dispatchPrimaryExit(Core &core);

  void startTransition(Spider1OuterRoute continuation);
  void stepTransitionFirst(Core &core);
  void stepTransitionSecond(Core &core);
  void finishTransition(Core &core);

  void initializeMenu(Core &core);
  void stepMenu(Core &core);
  void finishMenu(Core &core);

  void prepareLevelRoute(Core &core);
  void initializeAlternate(Core &core);
  void stepAlternate(Core &core);
  bool finishAlternate(Core &core);

  void startInvalidRoute(Core &core);
  void initializeInvalidInput(Core &core);
  bool pollInvalidInput(Core &core);

  Spider1ModeHost &host_;
  State state_ = State::Dormant;
  Spider1OuterRoute transitionContinuation_ = Spider1OuterRoute::Invalid;
  uint16_t transitionY_ = 0;
  uint32_t outerFlag20_ = 0;
  uint32_t outerFlag21_ = 0;
  uint32_t menuObject_ = 0;
  uint32_t menuFrame_ = 0;
  bool menuResult_ = false;
  bool alternateFlag_ = true;
  bool alternateNeedsInit_ = false;
  uint32_t invalidStartField_ = 0;
  bool invalidSawPad224_ = false;
  bool invalidSawPad48_ = false;
};

} // namespace spider
