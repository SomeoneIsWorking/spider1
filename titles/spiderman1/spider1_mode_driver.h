#pragma once

#include "spider1_alternate_mode.h"
#include "spider1_invalid_selector_input.h"
#include "spider1_menu_mode.h"
#include "spider1_mode_host.h"
#include "spider1_transition_wipe.h"

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

// The one implementation of the retail jump table: a primary exit's selector word names the route
// the outer cycle takes next. Nine of the eleven selectors are routes and the remaining two are the
// invalid-selector gate, which is why the result is an enum and not a "no route" sentinel.
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

// Which routes leave the outer screen's own clear word alone. Clearing it tears down the outer
// mode's retained state, and entering the menu deliberately does not: a title coming out of the
// menu returns to a warm outer cycle rather than a cold one.
//
// It takes the ROUTE and not the selector on purpose. The selectors that reach the menu are two,
// and this rule is about what the route does, so naming the route here keeps the answer in the jump
// table's hands instead of restating "2 and 9" in a second place.
constexpr bool spider1ModePreservesOuterClear(Spider1OuterRoute route) {
  return route == Spider1OuterRoute::Menu;
}

// Persistent native owner of Spider-Man 1's outer selector and its subordinate retail mode
// functions. Every step is finite and reaches exactly one host presentation or unpresented fence.
// Synchronous guest leaves execute through the per-Core runtime boundary.
//
// WHAT THIS CLASS IS, AND WHAT IT IS NOT. It owns the outer cycle, the primary mode, the 3D
// transition, and the level route. It does NOT own the title menu, the alternate mode, or the
// invalid-selector input wait: those are separate owners that hold their own state, and this class
// composes them the way it composes the outer cycle. What it keeps is the state machine that
// decides which mode is live, and the dispatch that turns a primary exit's selector into the next
// one.
class Spider1ModeDriver final {
public:
  explicit Spider1ModeDriver(Spider1ModeHost &host);

  void start(Core &core);
  void step(Core &core, uint32_t frame);

private:
  enum class State {
    Dormant,
    // The outer cycle is entered and its asynchronous load has not reported ready.
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

  // ---- the outer cycle -------------------------------------------------------------------------
  void enterOuterCycle(Core &core);
  void readOuterArgumentsAndPreparePrimary(Core &core);

  // One primary-exit dispatch, as a table of named routes. Every arm is a method below, so the
  // dispatch reads as a list of routes instead of as one long switch.
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

  // ---- the primary mode ------------------------------------------------------------------------
  void initializePrimary(Core &core);
  void stepPrimaryWarmup(Core &core);
  void stepPrimary(Core &core);
  void finishPrimary(Core &core);

  // ---- the 3D transition -----------------------------------------------------------------------
  // The wipe itself belongs to `Spider1TransitionWipe`. What stays here is the part that is the
  // MODE DRIVER's: which display half the wipe starts from, and what the title does once it lands.
  void stepTransitionFirst(Core &core);
  void stepTransitionSecond(Core &core);
  Spider1TransitionWipe::Destination wipeDestination() const;

  // ---- the level route -------------------------------------------------------------------------
  void prepareLevelRoute(Core &core);
  void startAlternate();
  void startInvalidRoute();

  // ---- the mode-entry gate ---------------------------------------------------------------------
  // The one place the "has this mode's asynchronous load reported ready?" question is asked. The
  // outer, menu, menu-exit, level, and invalid-selector entries all ask it identically, which is
  // why it is a method rather than five copies of a three-line guard. It presents a field and
  // returns true while the load is still running, which is also the caller's signal to end the host
  // step.
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

} // namespace spider
