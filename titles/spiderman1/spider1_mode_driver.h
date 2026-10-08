#pragma once

#include "spider1_alternate_mode.h"
#include "spider1_invalid_selector_input.h"
#include "spider1_menu_mode.h"
#include "spider1_mode_host.h"
#include "spider1_transition_wipe.h"

#include <cstdint>

class Core;

namespace spider::spider1 {

// Selector routes from SLUS_008.75's jump table at 0x80093C3C.
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

// A primary exit's selector word names the next route; two of the eleven selectors are the
// invalid-selector gate.
constexpr Spider1OuterRoute outerRouteFor(uint32_t selector) {
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

// Routes that leave the outer screen's clear word alone: coming out of the menu returns to a warm
// outer cycle.
constexpr bool modePreservesOuterClear(Spider1OuterRoute route) {
  return route == Spider1OuterRoute::Menu;
}

// Native owner of the outer selector, primary mode, 3D transition and level route. The menu,
// alternate mode and invalid-selector wait are separate owners it composes. Every step reaches
// exactly one presentation or unpresented fence.
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

  // One primary-exit dispatch, one method per route.
  void dispatchPrimaryExit(Core &core);
  void restartOuterCycleWithModeFive(Core &core);
  void enterMenuThroughTransition(Core &core);
  void enterLevelThroughTransition(Core &core);
  void advanceOuterCycleSelection(Core &core);
  void enterResourcePrimary(Core &core);
  void enterTransitionThenOuter(Core &core);
  void enterTransitionThenOuterWithFlag(Core &core);
  void enterPrimaryAfterReset(Core &core);

  void startTransition(Spider1OuterRoute continuation);

  void initializePrimary(Core &core);
  void stepPrimaryWarmup(Core &core);
  void stepPrimary(Core &core);
  void finishPrimary(Core &core);

  // The wipe itself belongs to `Spider1TransitionWipe`.
  void stepTransitionFirst(Core &core);
  void stepTransitionSecond(Core &core);
  Spider1TransitionWipe::Destination wipeDestination() const;

  void prepareLevelRoute(Core &core);
  void startAlternate();
  void startInvalidRoute();

  // Presents a field and returns true while the mode's asynchronous load is still running.
  bool awaitModeReady(Core &core);

  Spider1ModeHost &host_;
  State state_ = State::Dormant;
  Spider1OuterRoute transitionContinuation_ = Spider1OuterRoute::Invalid;
  uint32_t pendingOuterArgumentFirst_ = 0;
  uint32_t pendingOuterArgumentSecond_ = 0;
  Spider1MenuMode menu_;
  Spider1AlternateMode alternate_;
  Spider1InvalidSelectorInput invalidInput_;
  Spider1TransitionWipe wipe_;
};

} // namespace spider::spider1
