// Spider-Man 1 mode decisions over their whole input space; Core-free.

#include "spider1_mode_decisions.h"
#include "spider1_mode_driver.h"
#include "spider1_transition_wipe.h"

#include "testutil.h"

#include <cstdint>

using namespace spider::spider1;

namespace {

void test_frame_waits_only_when_no_field_passed(void) {
  // A frame that advanced the counter by none waits; by one does not.
  CHECK(modeFrameNeedsFieldWait(100u, 100u));
  CHECK(!modeFrameNeedsFieldWait(100u, 101u));
  // Every non-zero advance never waits.
  for (uint32_t advance = 0; advance <= 3u; ++advance) {
    const uint32_t waits = modeFrameNeedsFieldWait(7u, 7u + advance) ? 1u : 0u;
    CHECK_EQ(waits, advance == 0u ? 1u : 0u);
  }
}

void test_draw_sync_is_complete_only_on_a_zero_result(void) {
  CHECK(modeDrawSyncComplete(0u));
  CHECK(!modeDrawSyncComplete(1u));
  CHECK(!modeDrawSyncComplete(0xFFFFFFFFu));
  // Exactly one word completes; "keep servicing" is its negation.
  for (uint32_t result = 0; result < 2u; ++result) {
    CHECK_EQ(modeDrawSyncComplete(result) ? 1u : 0u, result == 0u ? 1u : 0u);
  }
}

void test_invalid_input_needs_a_release_and_not_just_a_press(void) {
  // A never-latched key cannot release the gate, whatever it reads.
  CHECK(!modeInvalidInputSatisfied(false, 0u, false, 0u, 0u, 1800u));
  CHECK(!modeInvalidInputSatisfied(false, 1u, false, 1u, 0u, 1800u));
  CHECK(!modeInvalidInputSatisfied(false, 0u, false, 0u, 1799u, 1800u));
  // A latched key releases on the first later non-zero poll, for either key.
  CHECK(modeInvalidInputSatisfied(true, 1u, false, 0u, 0u, 1800u));
  CHECK(modeInvalidInputSatisfied(false, 0u, true, 1u, 0u, 1800u));
  CHECK(modeInvalidInputSatisfied(true, 1u, true, 1u, 0u, 1800u));
  // A latched key still reading zero is a held button, not a release.
  CHECK(!modeInvalidInputSatisfied(true, 0u, false, 0u, 0u, 1800u));
  // The timeout uses >=, firing on the boundary field and with no press.
  CHECK(!modeInvalidInputSatisfied(false, 0u, false, 0u, 1799u, 1800u));
  CHECK(modeInvalidInputSatisfied(false, 0u, false, 0u, 1800u, 1800u));
  CHECK(modeInvalidInputSatisfied(false, 0u, false, 0u, 1801u, 1800u));
  // A long-past timeout still fires; the elapsed count only grows.
  CHECK(modeInvalidInputSatisfied(false, 0u, false, 0u, 0xFFFFFFFFu, 1800u));
  // Counted over the latch/pad space so an always-true rule fails.
  unsigned opened = 0;
  unsigned expected = 0;
  for (unsigned latch = 0; latch < 4u; ++latch) {
    for (unsigned value = 0; value < 2u; ++value) {
      const bool sawSecond = (latch & 1u) != 0u;
      const bool sawFirst = (latch & 2u) != 0u;
      const uint8_t secondKey = static_cast<uint8_t>(value);
      const uint8_t firstKey = static_cast<uint8_t>(value);
      const bool openedHere =
          modeInvalidInputSatisfied(sawSecond, secondKey, sawFirst, firstKey, 0u, 1800u);
      const bool shouldOpen = (sawSecond && secondKey != 0u) || (sawFirst && firstKey != 0u);
      opened += openedHere ? 1u : 0u;
      expected += shouldOpen ? 1u : 0u;
      CHECK_EQ(openedHere ? 1u : 0u, shouldOpen ? 1u : 0u);
    }
  }
  CHECK_EQ(opened, expected);
  // Exactly three of the eight combinations open the gate.
  CHECK_EQ(opened, 3u);
}

void test_alternate_exit_distinguishes_a_press_from_the_flag(void) {
  // Only a press is consumed before the mode acts on it; byPad carries that.
  const Spider1AlternateExit running = modeAlternateExit(true, false);
  CHECK(!running.exits);
  CHECK(!running.byPad);
  const Spider1AlternateExit byPress = modeAlternateExit(true, true);
  CHECK(byPress.exits);
  CHECK(byPress.byPad);
  const Spider1AlternateExit byFlag = modeAlternateExit(false, false);
  CHECK(byFlag.exits);
  CHECK(!byFlag.byPad);
  // A press while the flag has already ended the mode is neither consumed nor audible.
  const Spider1AlternateExit both = modeAlternateExit(false, true);
  CHECK(both.exits);
  CHECK(!both.byPad);
  // One combination consumes a press; three end the mode.
  unsigned consuming = 0;
  unsigned exiting = 0;
  for (unsigned flag = 0; flag < 2u; ++flag) {
    for (unsigned press = 0; press < 2u; ++press) {
      const Spider1AlternateExit outcome = modeAlternateExit(flag != 0u, press != 0u);
      consuming += outcome.byPad ? 1u : 0u;
      exiting += outcome.exits ? 1u : 0u;
    }
  }
  CHECK_EQ(consuming, 1u);
  CHECK_EQ(exiting, 3u);
}

void test_alternate_repeats_needs_both_the_byte_and_the_bytes_read(void) {
  // Both conditions are required: the byte alone loops on an unreadable file, the size alone
  // repeats a released object.
  CHECK(!modeAlternateRepeats(true, 0u));
  CHECK(!modeAlternateRepeats(false, 0x1000u));
  CHECK(!modeAlternateRepeats(false, 0u));
  CHECK(modeAlternateRepeats(true, 1u));
  CHECK(modeAlternateRepeats(true, 0xFFFFFFFFu));
  unsigned repeats = 0;
  for (unsigned asked = 0; asked < 2u; ++asked) {
    for (unsigned size = 0; size < 2u; ++size) {
      repeats += modeAlternateRepeats(asked != 0u, size) ? 1u : 0u;
    }
  }
  CHECK_EQ(repeats, 1u);
}

void test_level_counter_saturates_and_never_wraps(void) {
  CHECK_EQ(modeSaturatingIncrement(0u), 1u);
  CHECK_EQ(modeSaturatingIncrement(254u), 255u);
  CHECK_EQ(modeSaturatingIncrement(255u), 255u);
  // A wrapped counter would read as an unvisited level; the saturated value never decreases.
  for (uint32_t counter = 0; counter < 256u; ++counter) {
    const uint8_t next = modeSaturatingIncrement(static_cast<uint8_t>(counter));
    CHECK(next >= counter);
    CHECK(next <= 255u);
  }
  // Exactly one of the 256 counters is the saturated value, and it maps to itself.
  unsigned saturated = 0;
  for (uint32_t counter = 0; counter < 256u; ++counter) {
    saturated += modeSaturatingIncrement(static_cast<uint8_t>(counter)) == 0xFFu ? 1u : 0u;
  }
  CHECK_EQ(saturated, 2u);
}

void test_only_the_menu_route_preserves_the_outer_clear(void) {
  // The rule takes the route; exactly one route preserves the clear.
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
    preserving += modePreservesOuterClear(route) ? 1u : 0u;
  }
  CHECK_EQ(preserving, 1u);
  CHECK(modePreservesOuterClear(Spider1OuterRoute::Menu));
  CHECK(!modePreservesOuterClear(Spider1OuterRoute::Invalid));
  // Both menu selectors must reach the menu route.
  CHECK(outerRouteFor(2u) == Spider1OuterRoute::Menu);
  CHECK(outerRouteFor(9u) == Spider1OuterRoute::Menu);
}

void test_wipe_darkening_is_the_retail_channel_arithmetic(void) {
  // Expected values written out longhand:
  // sum = r+g+b (5 bits each); dim = 341*sum>>11; strong = 1365*sum>>12;
  // out = (sign ? 0x8000 : 0) | strong<<10 | dim<<5 | dim
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
  // The sign bit survives; green and blue are equal and red is never smaller (scan toward red).
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
  // The wipe remaps channels toward red rather than darkening: a near-black blue pixel comes back
  // brighter.
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
