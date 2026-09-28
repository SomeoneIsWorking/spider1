// spider1_mode_rules_test.cpp — the Spider-Man 1 mode DECISIONS, over their whole input space.
//
// WHY THIS IS A SEPARATE, Core-FREE TEST. Every rule here is a `constexpr` function of what the
// mode observed, so its whole input space is enumerable here — no guest image, no GPU, no window,
// no product. That is the point: the rules are the parts a readability refactor can most easily get
// subtly wrong, and they are the parts a single run of the game would exercise only one branch of.
//
// EACH RULE IS PINNED BOTH WAYS. Where a rule replaced open-coded arithmetic, the case the old code
// got RIGHT is asserted alongside the case it got wrong, so a test that only pins the new shape
// would fail to notice a behaviour change in the other direction.
//
// No disc, no GPU, no window, no network, no sleeping: this is a hermetic unit test of pure
// functions. See `testutil.h`'s rules for why a suite that reports "skipped" reads as green.

#include "spider1_mode_decisions.h"
#include "spider1_mode_driver.h"
#include "spider1_transition_wipe.h"

#include "testutil.h"

#include <cstdint>

using namespace spider;

namespace {

// ---- the frame's own field wait --------------------------------------------------------------
void test_frame_waits_only_when_no_field_passed(void) {
  // THE CONTRACT THE OLD COMPARISON GOT RIGHT: a frame whose work advanced the counter by none
  // waits, and one that advanced it by one does not. These two are the only cases a real frame
  // produces.
  CHECK(spider1ModeFrameNeedsFieldWait(100u, 100u));
  CHECK(!spider1ModeFrameNeedsFieldWait(100u, 101u));
  // The denominator: every advance from no-field to three-field waits exactly once, and every
  // non-zero advance waits NOT AT ALL. There is no advance for which this rule waits.
  for (uint32_t advance = 0; advance <= 3u; ++advance) {
    const uint32_t waits = spider1ModeFrameNeedsFieldWait(7u, 7u + advance) ? 1u : 0u;
    CHECK_EQ(waits, advance == 0u ? 1u : 0u);
  }
}

// ---- draw sync ---------------------------------------------------------------------------------
void test_draw_sync_is_complete_only_on_a_zero_result(void) {
  CHECK(spider1ModeDrawSyncComplete(0u));
  CHECK(!spider1ModeDrawSyncComplete(1u));
  CHECK(!spider1ModeDrawSyncComplete(0xFFFFFFFFu));
  // Exactly one of the two possible words completes; the loop's "keep servicing" answer is the
  // negation, so there is no third answer hiding in the rule.
  for (uint32_t result = 0; result < 2u; ++result) {
    CHECK_EQ(spider1ModeDrawSyncComplete(result) ? 1u : 0u, result == 0u ? 1u : 0u);
  }
}

// ---- the invalid-selector input gate -----------------------------------------------------------
void test_invalid_input_needs_a_release_and_not_just_a_press(void) {
  // A key that was NEVER latched as pressed can never release the gate, however long it reads
  // non-zero. This is the asymmetry that makes the gate a press-and-release, and it is the whole
  // of the behaviour, so it is asserted from all four latch/value pairs.
  CHECK(!spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 0u, 1800u));
  CHECK(!spider1ModeInvalidInputSatisfied(false, 1u, false, 1u, 0u, 1800u));
  CHECK(!spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 1799u, 1800u));
  // A latched key releases on the first LATER poll that reads non-zero — for EITHER key, and
  // whichever one it is.
  CHECK(spider1ModeInvalidInputSatisfied(true, 1u, false, 0u, 0u, 1800u));
  CHECK(spider1ModeInvalidInputSatisfied(false, 0u, true, 1u, 0u, 1800u));
  CHECK(spider1ModeInvalidInputSatisfied(true, 1u, true, 1u, 0u, 1800u));
  // A latched key that STILL reads zero is a held button, not a release: it does not open the gate.
  CHECK(!spider1ModeInvalidInputSatisfied(true, 0u, false, 0u, 0u, 1800u));
  // The timeout is a field count compared with `>=`, so it fires ON the boundary field and not one
  // before it, and it fires with no press at all.
  CHECK(!spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 1799u, 1800u));
  CHECK(spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 1800u, 1800u));
  CHECK(spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 1801u, 1800u));
  // A run long past its timeout still times out rather than wrapping: the elapsed count is a
  // subtraction and only ever grows.
  CHECK(spider1ModeInvalidInputSatisfied(false, 0u, false, 0u, 0xFFFFFFFFu, 1800u));
  // The denominator: over the full 2x2 latch space crossed with both pad values, the gate opens
  // either through a release (a latched key reading non-zero) or through the timeout, and those are
  // the ONLY ways it opens. Count them, so a rule that answered "always true" could not pass.
  unsigned opened = 0;
  unsigned expected = 0;
  for (unsigned latch = 0; latch < 4u; ++latch) {
    for (unsigned value = 0; value < 2u; ++value) {
      const bool sawSecond = (latch & 1u) != 0u;
      const bool sawFirst = (latch & 2u) != 0u;
      const uint8_t secondKey = static_cast<uint8_t>(value);
      const uint8_t firstKey = static_cast<uint8_t>(value);
      const bool openedHere =
          spider1ModeInvalidInputSatisfied(sawSecond, secondKey, sawFirst, firstKey, 0u, 1800u);
      const bool shouldOpen = (sawSecond && secondKey != 0u) || (sawFirst && firstKey != 0u);
      opened += openedHere ? 1u : 0u;
      expected += shouldOpen ? 1u : 0u;
      CHECK_EQ(openedHere ? 1u : 0u, shouldOpen ? 1u : 0u);
    }
  }
  CHECK_EQ(opened, expected);
  // Over the full 2x2 latch space crossed with both pad values, exactly THREE combinations open the
  // gate: each latch that has seen a press, at the non-zero value. The remaining five do not, so a
  // rule that answered "always true" or "true once a latch is set" both fail the count.
  CHECK_EQ(opened, 3u);
}

// ---- the alternate mode's exit ---------------------------------------------------------------
void test_alternate_exit_distinguishes_a_press_from_the_flag(void) {
  // THE CONTRACT, all four combinations. `byPad` is the part a bare bool would have thrown away,
  // and it is load-bearing: only a press is CONSUMED before the mode acts on it.
  const Spider1AlternateExit running = spider1ModeAlternateExit(true, false);
  CHECK(!running.exits);
  CHECK(!running.byPad);
  const Spider1AlternateExit byPress = spider1ModeAlternateExit(true, true);
  CHECK(byPress.exits);
  CHECK(byPress.byPad);
  const Spider1AlternateExit byFlag = spider1ModeAlternateExit(false, false);
  CHECK(byFlag.exits);
  CHECK(!byFlag.byPad);
  // A press that arrives while the flag has ALREADY ended the mode ends nothing further: the mode
  // is on its way out, so the press must not be consumed and must not make noise.
  const Spider1AlternateExit both = spider1ModeAlternateExit(false, true);
  CHECK(both.exits);
  CHECK(!both.byPad);
  // The denominator: exactly one of the four combinations consumes a press, and exactly three of
  // the four end the mode.
  unsigned consuming = 0;
  unsigned exiting = 0;
  for (unsigned flag = 0; flag < 2u; ++flag) {
    for (unsigned press = 0; press < 2u; ++press) {
      const Spider1AlternateExit outcome = spider1ModeAlternateExit(flag != 0u, press != 0u);
      consuming += outcome.byPad ? 1u : 0u;
      exiting += outcome.exits ? 1u : 0u;
    }
  }
  CHECK_EQ(consuming, 1u);
  CHECK_EQ(exiting, 3u);
}

void test_alternate_repeats_needs_both_the_byte_and_the_bytes_read(void) {
  // Either condition alone is a bug, and both directions are asserted: the byte alone would loop
  // forever on a file the drive can no longer supply, and the size alone would repeat an object
  // that has released itself.
  CHECK(!spider1ModeAlternateRepeats(true, 0u));
  CHECK(!spider1ModeAlternateRepeats(false, 0x1000u));
  CHECK(!spider1ModeAlternateRepeats(false, 0u));
  CHECK(spider1ModeAlternateRepeats(true, 1u));
  CHECK(spider1ModeAlternateRepeats(true, 0xFFFFFFFFu));
  unsigned repeats = 0;
  for (unsigned asked = 0; asked < 2u; ++asked) {
    for (unsigned size = 0; size < 2u; ++size) {
      repeats += spider1ModeAlternateRepeats(asked != 0u, size) ? 1u : 0u;
    }
  }
  CHECK_EQ(repeats, 1u);
}

// ---- the saturating visit counter --------------------------------------------------------------
void test_level_counter_saturates_and_never_wraps(void) {
  CHECK_EQ(spider1ModeSaturatingIncrement(0u), 1u);
  CHECK_EQ(spider1ModeSaturatingIncrement(254u), 255u);
  CHECK_EQ(spider1ModeSaturatingIncrement(255u), 255u);
  // A wrapped counter would read as a level nobody has visited, which is exactly the answer this
  // rule exists to refuse. Nothing above the saturation point may ever decrease, and the saturated
  // value is the SAME whatever the counter was — a counter that decremented from 255 would be a
  // level that un-visits itself.
  for (uint32_t counter = 0; counter < 256u; ++counter) {
    const uint8_t next = spider1ModeSaturatingIncrement(static_cast<uint8_t>(counter));
    CHECK(next >= counter);
    CHECK(next <= 255u);
  }
  // Exactly one of the 256 reachable counters is the saturated value, and it maps to itself.
  // Counted, so a rule that stopped incrementing early — or one that wrapped — would fail the count
  // rather than passing a single spot check.
  unsigned saturated = 0;
  for (uint32_t counter = 0; counter < 256u; ++counter) {
    saturated += spider1ModeSaturatingIncrement(static_cast<uint8_t>(counter)) == 0xFFu ? 1u : 0u;
  }
  CHECK_EQ(saturated, 2u);
}

// ---- which routes keep the outer clear ---------------------------------------------------------
void test_only_the_menu_route_preserves_the_outer_clear(void) {
  // The rule takes the ROUTE, so the jump table above stays the one place that knows which
  // selectors reach the menu. Asserted against every route: exactly one preserves it.
  const Spider1OuterRoute routes[]{
      Spider1OuterRoute::Invalid,
      Spider1OuterRoute::RestartWithModeFive,
      Spider1OuterRoute::Menu,
      Spider1OuterRoute::Level,
      Spider1OuterRoute::CycleSelection,
      Spider1OuterRoute::ResourcePrimary,
      Spider1OuterRoute::TransitionThenOuter,
      Spider1OuterRoute::ResetThenPrimary,
      Spider1OuterRoute::TransitionThenOuterWithFlag,
  };
  unsigned preserving = 0;
  for (const Spider1OuterRoute route : routes) {
    preserving += spider1ModePreservesOuterClear(route) ? 1u : 0u;
  }
  CHECK_EQ(preserving, 1u);
  CHECK(spider1ModePreservesOuterClear(Spider1OuterRoute::Menu));
  CHECK(!spider1ModePreservesOuterClear(Spider1OuterRoute::Invalid));
  // AND THE LINK TO THE TABLE: both menu selectors must actually reach the menu route, or the rule
  // would preserve the clear for selectors the title never sends there.
  CHECK(spider1OuterRoute(2u) == Spider1OuterRoute::Menu);
  CHECK(spider1OuterRoute(9u) == Spider1OuterRoute::Menu);
}

// ---- the 3D wipe's darkening -------------------------------------------------------------------
void test_wipe_darkening_is_the_retail_channel_arithmetic(void) {
  // Computed from the rule itself, written out longhand here on purpose: these are the expected
  // values, not a second call into the code under test.
  //   sum = r + g + b (five bits each); dim = 341*sum >> 11; strong = 1365*sum >> 12;
  //   out = (sign ? 0x8000 : 0) | strong<<10 | dim<<5 | dim
  const auto expected = [](uint16_t pixel) -> uint16_t {
    const uint32_t sum =
        static_cast<uint32_t>(pixel & 31u) + ((pixel >> 5u) & 31u) + ((pixel >> 10u) & 31u);
    const uint32_t dim = (341u * sum) >> 11u;
    const uint32_t strong = (1365u * sum) >> 12u;
    return static_cast<uint16_t>((pixel & 0x8000u) | (strong << 10u) | (dim << 5u) | dim);
  };
  const uint16_t pixels[] = {
      0x0000u,
      0x0001u,
      0x001Fu,
      0x7FFFu,
      0x8000u,
      0xFFFFu,
      0x1234u,
      0xABCDu,
      0x5A5Au,
      0x2100u,
  };
  for (const uint16_t pixel : pixels) {
    CHECK_EQ(Spider1TransitionWipe::darkenPixel(pixel), expected(pixel));
  }
  // The two properties the wipe depends on, stated as properties over a sweep rather than as one
  // sample: the sign bit survives every pixel, and the structure of the result is exactly
  // `sign | strong<<10 | dim<<5 | dim`. The second is what makes the wipe read as a scan TOWARD
  // RED: green and blue are the SAME value, and red is never smaller than they are. Counted over
  // the whole sweep, so a rule that permuted two channels would fail.
  unsigned signPreserved = 0;
  unsigned structureHeld = 0;
  unsigned scanned = 0;
  for (uint32_t value = 0; value < 0x10000u; value += 7u) {
    const uint16_t pixel = static_cast<uint16_t>(value);
    const uint16_t out = Spider1TransitionWipe::darkenPixel(pixel);
    ++scanned;
    signPreserved += (out & 0x8000u) == (pixel & 0x8000u) ? 1u : 0u;
    const uint32_t red = (out >> 10u) & 31u;
    const uint32_t green = (out >> 5u) & 31u;
    const uint32_t blue = out & 31u;
    structureHeld += (green == blue && red >= green) ? 1u : 0u;
  }
  CHECK_EQ(scanned, (0x10000u + 6u) / 7u);
  CHECK_EQ(signPreserved, scanned);
  CHECK_EQ(structureHeld, scanned);
  CHECK_EQ(Spider1TransitionWipe::darkenPixel(0x0000u), 0x0000u);
  CHECK(Spider1TransitionWipe::darkenPixel(0xFFFFu) < 0xFFFFu);
  CHECK(Spider1TransitionWipe::darkenPixel(0x7FFFu) < 0x7FFFu);
  // AND WHAT THE WIPE IS NOT, pinned because it is the assumption a reader reaches for and it is
  // false: the wipe is a channel REMAP toward red, not a monotonic darkening. A near-black blue
  // pixel comes back BRIGHTER, because its red share is taken from the total of all three channels
  // and its own blue share is nearly nothing. Asserting the exact value stops anyone "fixing" that
  // into a plain multiply.
  CHECK_EQ(Spider1TransitionWipe::darkenPixel(0x001Fu), 0x28A5u);
  CHECK(Spider1TransitionWipe::darkenPixel(0x001Fu) > 0x001Fu);
}

} // namespace

int main(void) {
  RUN(frame_waits_only_when_no_field_passed);
  RUN(draw_sync_is_complete_only_on_a_zero_result);
  RUN(invalid_input_needs_a_release_and_not_just_a_press);
  RUN(alternate_exit_distinguishes_a_press_from_the_flag);
  RUN(alternate_repeats_needs_both_the_byte_and_the_bytes_read);
  RUN(level_counter_saturates_and_never_wraps);
  RUN(only_the_menu_route_preserves_the_outer_clear);
  RUN(wipe_darkening_is_the_retail_channel_arithmetic);
  return pt_summary();
}
