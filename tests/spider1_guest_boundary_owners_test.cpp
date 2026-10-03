// spider1_guest_boundary_owners_test.cpp — the guest-boundary owners, driven against a real `Core`.
//
// WHAT THIS IS. The small owners extracted out of this repository's two largest files each have a
// decision that a single run of the game would exercise only one branch of. This drives them
// against a bare `Core` with native-override probes standing in for the retail guest bodies, so
// each decision is asserted from BOTH sides without a disc, a GPU, a window, or the product.
//
// THE PROBES ARE THE POINT, NOT A SHORTCUT. A probe records the call ORDER as well as the count, so
// "the finalize step is taken only for display mode 1, and only last" is an assertion about a
// sequence rather than about a total. A refactor that reordered two guest calls fails here even
// though the game would still run.
//
// THE PROBE MUST BE ABLE TO SAY "I DID NOT MATCH". Every probe records the address it was actually
// entered at and this suite asserts that every one of those was an address it installed a probe
// for. Without that, a probe that stopped matching the entry it stands for would silently read the
// default result and let a wrong case pass — which is the same failure shape as a diagnostic that
// reports "matched none" and is read as "found none".
//
// HERMETIC. No image, no GPU, no window, no network, no sleeping. `testutil.h`'s rules are the
// reason there is no skip path: a suite that reports "skipped" reads as green while covering
// nothing.

#include "spider1_cd_initialization.h"
#include "spider1_field_clock.h"
#include "spider1_gpu_reset.h"
#include "spider1_guest_layout.h"
#include "spider1_host_stepped_fiber.h"
#include "spider1_invalid_selector_input.h"
#include "spider1_mode_frame_boundary.h"
#include "spider1_platform_facts.h"

#include "core.h"
#include "native_execution.h"
#include "testutil.h"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

using namespace spider;

namespace {

// One resident image covering every address these owners touch, so a native override may be
// installed there. The range is the title's own main-RAM window in PHYSICAL terms, which is what
// the image catalog resolves a guest address against; nothing in this test executes the bytes it
// covers.
constexpr uint32_t kResidentStart = 0x00010000u;
constexpr uint32_t kResidentBytes = 0x00200000u;

// A `Core` is 2 MB of emulated RAM, so it is heap-allocated rather than sat in a test's frame.
std::unique_ptr<Core> makeCore(const char *name) {
  auto core = std::make_unique<Core>();
  core->imageCatalog().activate(name, {kResidentStart, kResidentStart + kResidentBytes}, 1u);
  return core;
}

// The recorded call sequence, and the result each probed entry hands back.
std::vector<uint32_t> g_calls;
std::map<uint32_t, uint32_t> g_results;
// Entries that report "not finished" a fixed number of times before reporting finished. The drain
// loop is unbounded by design — a GPU that never reports done must keep being serviced — so a test
// that wants to drive it out has to say when the GPU reports done.
std::map<uint32_t, unsigned> g_notFinishedTimes;
unsigned g_probeMisses = 0;

void probeEntered(Core *core) {
  const uint32_t entry = core->pc;
  g_calls.push_back(entry);
  const auto countdown = g_notFinishedTimes.find(entry);
  if (countdown != g_notFinishedTimes.end()) {
    core->r[2] = countdown->second > 0u ? 1u : 0u;
    if (countdown->second > 0u) {
      --countdown->second;
    }
    return;
  }
  const auto found = g_results.find(entry);
  if (found == g_results.end()) {
    ++g_probeMisses;
    core->r[2] = 0;
    return;
  }
  core->r[2] = found->second;
}

void resetCalls() {
  g_calls.clear();
  g_probeMisses = 0;
  g_notFinishedTimes.clear();
}

void installProbe(Core &core, uint32_t entry, uint32_t result = 0) {
  g_results[entry] = result;
  psx::cpu::installNativeOverride(core, entry, "spider1 test probe", &probeEntered);
}

unsigned countOf(const std::vector<uint32_t> &calls, uint32_t entry) {
  unsigned total = 0;
  for (const uint32_t seen : calls) {
    total += seen == entry ? 1u : 0u;
  }
  return total;
}

} // namespace

// ---- the display field clock -------------------------------------------------------------------

void test_field_clock_measures_since_the_previous_field(void) {
  const std::unique_ptr<Core> core = makeCore("field clock");
  // The title holds a POINTER to its horizontal counter, not the counter. A null pointer reads as
  // zero rather than faulting, which is what the retail load would have produced.
  core->mem_w32(spider1::horizontalCounterPointer, 0u);
  CHECK_EQ(Spider1FieldClock::horizontalCounter(*core), 0u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0u);

  // Now point it at a counter and set a baseline. The return value is the ADVANCE since the last
  // field, truncated to sixteen bits — not the counter itself, which is the mistake a reader makes
  // when the subtraction is written out by hand at three call sites.
  core->mem_w32(spider1::horizontalCounterPointer, 0x800A0000u);
  core->mem_w32(0x800A0000u, 1000u);
  core->mem_w32(spider1::horizontalCounterBaseline, 900u);
  CHECK_EQ(Spider1FieldClock::horizontalCounter(*core), 1000u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 100u);

  // THE TRUNCATION IS THE CONTRACT, in both directions. A counter that wrapped past 65535 must
  // report the wrapped advance, and a baseline ahead of the counter must report the wrapped value
  // too — both are the sixteen-bit difference the guest computes, and an unsigned-thirty-two-bit
  // subtraction would report a huge number for the second case.
  core->mem_w32(0x800A0000u, 5u);
  core->mem_w32(spider1::horizontalCounterBaseline, 0xFFFFu);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 6u);
  core->mem_w32(0x800A0000u, 0xFFF0u);
  core->mem_w32(spider1::horizontalCounterBaseline, 0x0010u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0xFFE0u);
}

void test_field_clock_rebases_only_after_the_value_was_read(void) {
  const std::unique_ptr<Core> core = makeCore("field clock rebase");
  core->mem_w32(spider1::horizontalCounterPointer, 0x800A0000u);
  core->mem_w32(0x800A0000u, 4242u);
  core->mem_w32(spider1::horizontalCounterBaseline, 4000u);
  core->mem_w32(spider1::libetcVblankCountAddress, 77u);

  const uint32_t returnValue = Spider1FieldClock::vsyncReturnValue(*core);
  CHECK_EQ(returnValue, 242u);
  Spider1FieldClock::recordDeliveredField(*core);
  // Recording publishes WHICH field completed and rebases the counter, so the NEXT VSync(0)
  // measures from here. Without the rebase every later field would report a growing value forever,
  // which is the failure the ordering here prevents: the value is read BEFORE the record.
  CHECK_EQ(core->mem_r32(spider1::lastVsyncField), 77u);
  CHECK_EQ(core->mem_r32(spider1::horizontalCounterBaseline), 4242u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0u);
  // The field counter is the title's own word, advanced by the display-field owner and read by
  // every mode's "did a field pass during this frame?" question.
  CHECK_EQ(Spider1FieldClock::fieldCount(*core), 77u);
}

// ---- ResetGraph's GPU half ---------------------------------------------------------------------

void test_gpu_reset_orders_its_steps_and_keeps_both_decisions(void) {
  const std::unique_ptr<Core> core = makeCore("gpu reset");
  // Every entry gets a probe, so the sequence asserted below is the order the retail body performs
  // the steps in. The addresses are deliberately non-monotonic and must not be sorted.
  const uint32_t kBegin = 0x8008C000u;
  const uint32_t kReadMode = 0x80084E00u;
  const uint32_t kProbe = 0x8008C010u;
  const uint32_t kApply = 0x8008C020u;
  const uint32_t kStep0 = 0x8008C210u;
  const uint32_t kStep1 = 0x8008C10Cu;
  const uint32_t kStep2 = 0x8008C1A0u;
  const uint32_t kStep3 = 0x8008C030u;
  const uint32_t kFinalize = 0x800848F0u;
  for (const uint32_t entry :
       {kBegin, kReadMode, kProbe, kApply, kStep0, kStep1, kStep2, kStep3, kFinalize}) {
    installProbe(*core, entry, 0u);
  }
  // The read-mode probe is the one whose result matters, so it reports display mode 1 by default
  // and the reset probe reports "reset".
  g_results[kReadMode] = 1u;
  g_results[kProbe] = 1u;

  // CASE 1: display mode 1, probe says the GPU is reset. Eight steps plus the finalize, and the
  // finalize is LAST — the one sequence fact a call count alone could not catch.
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 0u));
  CHECK_EQ(g_calls.size(), 9u);
  CHECK_EQ(g_calls.back(), kFinalize);
  CHECK_EQ(g_calls[0], kBegin);
  CHECK_EQ(g_calls[1], kReadMode);
  CHECK_EQ(g_calls[2], kProbe);
  CHECK_EQ(g_calls[3], kApply);
  CHECK_EQ(g_calls[4], kStep0);
  CHECK_EQ(g_calls[5], kStep1);
  CHECK_EQ(g_calls[6], kStep2);
  CHECK_EQ(g_calls[7], kStep3);
  CHECK_EQ(core->r[4], 0u);
  CHECK_EQ(g_probeMisses, 0u);

  // CASE 2: display mode 0 takes NO finalize step at all. The rule is about the mode READ BACK
  // before the reset, not the mode the caller asked for.
  g_results[kReadMode] = 0u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 0u));
  CHECK_EQ(g_calls.size(), 8u);
  CHECK_EQ(countOf(g_calls, kFinalize), 0u);

  // CASE 3: the reset probe reports NOT reset. The caller's requested mode is DISCARDED, not
  // merged: the apply step must be handed zero whatever was asked for. This decision is invisible
  // without a probe, because the retail code reads like a default.
  g_results[kReadMode] = 1u;
  g_results[kProbe] = 0u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 2u));
  CHECK_EQ(core->r[4], 0u);
  // ...and the same request SURVIVES when the probe does report reset, which is the other side of
  // the same decision. Without this second half a rule that always sent zero would pass.
  g_results[kProbe] = 1u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 2u));
  CHECK_EQ(core->r[4], 2u);
  CHECK_EQ(g_probeMisses, 0u);
}

// ---- the CdInit replacement --------------------------------------------------------------------

void test_cd_initialization_installs_every_slot_and_keeps_the_guests_own_mask_bits(void) {
  // The slot half, against real guest RAM. All four slots from the authenticated CdInit, plus the
  // success result its public contract owes.
  const std::unique_ptr<Core> core = makeCore("cd init");
  Spider1CdInitialization::installCallbackSlots(*core);
  CHECK_EQ(core->mem_r32(spider1::cdSyncCallbackSlot), spider1::cdSyncCallback);
  CHECK_EQ(core->mem_r32(spider1::cdReadyCallbackSlot), spider1::cdReadyCallback);
  CHECK_EQ(core->mem_r32(spider1::cdEventCallbackSlot), spider1::cdEventCallback);
  CHECK_EQ(core->mem_r32(spider1::cdEventUnusedSlot), 0u);
  CHECK_EQ(core->r[2], 1u);
  // Running it twice is the same four words again, because the whole contract is a write of
  // constants rather than an increment.
  Spider1CdInitialization::installCallbackSlots(*core);
  CHECK_EQ(core->mem_r32(spider1::cdSyncCallbackSlot), spider1::cdSyncCallback);
  CHECK_EQ(core->mem_r32(spider1::cdReadyCallbackSlot), spider1::cdReadyCallback);
  CHECK_EQ(core->mem_r32(spider1::cdEventCallbackSlot), spider1::cdEventCallback);
  CHECK_EQ(core->mem_r32(spider1::cdEventUnusedSlot), 0u);

  // THE ARM'S ARITHMETIC, over a sweep of masks. The arm reads the CURRENT mask back and OR-s the
  // CD bit in, because a literal write would clear whatever VBlank and DMA enables the guest had
  // already asked for. So the rule must never clear a bit, and must change the value exactly when
  // the CD bit was absent. The device write that APPLIES this is the framework's memory path, and
  // the shipping path through the real native override is `spider1_runtime_services` — which is the
  // test that builds a `Game`, and the reason this arithmetic is a rule rather than open-coded.
  unsigned preserved = 0;
  unsigned armed = 0;
  for (uint32_t before = 0; before < 0x200u; ++before) {
    const uint32_t after = Spider1CdInitialization::armedMask(before);
    preserved += (after & before) == before ? 1u : 0u;
    armed += after != before ? 1u : 0u;
  }
  CHECK_EQ(preserved, 0x200u);
  CHECK_EQ(armed, 0x200u - (0x200u / 2u));
  // The measured mask the boundary was diagnosed on: 0x009 becomes 0x00D, and every bit the guest
  // held survives. That is the arithmetic the delivery gate turned on.
  CHECK_EQ(Spider1CdInitialization::armedMask(0x009u), 0x00Du);
  CHECK_EQ(Spider1CdInitialization::armedMask(0x009u) & 0x009u, 0x009u);
  // A mask the guest has already widened keeps everything it had — the property the OR exists for.
  CHECK_EQ(Spider1CdInitialization::armedMask(0x0C9u), 0x0CDu);
  CHECK_EQ(Spider1CdInitialization::armedMask(0x0C9u) & 0x0C8u, 0x0C8u);
  // And an already-armed mask is the no-write case, which is what the caller detects by comparing.
  CHECK_EQ(Spider1CdInitialization::armedMask(0x00Du), 0x00Du);
}

// ---- the invalid-selector input wait -----------------------------------------------------------

namespace {

// A pad read that stages the two bytes the gate reads, so what is under test is the gate's own
// latch behaviour and not the guest's pad plumbing.
struct PadStage {
  static uint8_t secondKey;
  static uint8_t firstKey;
  static void read(Core *core) {
    core->mem_w8(spider1::padState + spider1::padStateByteE0, secondKey);
    core->mem_w8(spider1::padState + spider1::padStateByte30, firstKey);
  }
};

uint8_t PadStage::secondKey = 0;
uint8_t PadStage::firstKey = 0;

} // namespace

void test_invalid_input_latches_a_press_before_it_accepts_a_release(void) {
  const std::unique_ptr<Core> core = makeCore("invalid input");
  psx::cpu::installNativeOverride(*core, spider1::padRead, "test pad read", &PadStage::read);
  psx::cpu::installNativeOverride(
      *core, spider1::invalidWaitSetup, "test invalid setup", [](Core *c) {
        c->r[2] = 0;
      });
  // The gate records the field it started on, so the timeout is measured from the start and not
  // from an absolute counter the test cannot reach.
  core->mem_w32(spider1::gameVblankCount, 10u);
  Spider1InvalidSelectorInput gate;
  gate.begin(*core, spider1::invalidInputWaitFields);
  CHECK_EQ(gate.startField(), 10u);

  // A pad byte that is ALWAYS non-zero never opens the gate: it was never seen pressed and
  // released.
  PadStage::secondKey = 1;
  PadStage::firstKey = 1;
  for (uint32_t poll = 0; poll < 8u; ++poll) {
    CHECK(!gate.poll(*core));
  }
  CHECK_EQ(gate.lastSecondKeyByte(), 1u);
  CHECK_EQ(gate.lastFirstKeyByte(), 1u);

  // Reading zero LATCHES the press. Still reading zero is a held button, not a release.
  PadStage::secondKey = 0;
  CHECK(!gate.poll(*core));
  CHECK(!gate.poll(*core));

  // The first LATER poll that reads non-zero opens it. This asymmetry is the whole rule, asserted
  // through the class so the wiring of the pure decision to the guest read is covered as well.
  PadStage::secondKey = 1;
  CHECK(gate.poll(*core));

  // A fresh gate times out on its own, AT the boundary field and not one before. The elapsed count
  // is a subtraction from the start field, so the test moves the counter rather than the timeout.
  Spider1InvalidSelectorInput timing;
  PadStage::secondKey = 0;
  PadStage::firstKey = 0;
  core->mem_w32(spider1::gameVblankCount, 100u);
  timing.begin(*core, spider1::invalidInputWaitFields);
  CHECK_EQ(timing.startField(), 100u);
  core->mem_w32(spider1::gameVblankCount, 100u + spider1::invalidInputTimeoutFields - 1u);
  CHECK(!timing.poll(*core));
  core->mem_w32(spider1::gameVblankCount, 100u + spider1::invalidInputTimeoutFields);
  CHECK(timing.poll(*core));
}

// ---- the mode frame boundary -------------------------------------------------------------------

namespace {

// A mode host that only counts what it was asked to do, because the frame boundary's contract is
// about how many fields it consumed and how many guest steps it took, not about presentation.
struct CountingModeHost final : Spider1ModeHost {
  unsigned fieldsWaited = 0;
  void waitFields(Core &, uint32_t count) override {
    fieldsWaited += count;
  }
  void commitSubmittedFrame(Core &) override {}
  void commitRepeatedFieldFrame(Core &) override {}
  void commitUnpresentedFrame(Core &) override {}
};

} // namespace

void test_frame_boundary_drains_fields_until_the_gpu_says_it_is_done(void) {
  const std::unique_ptr<Core> core = makeCore("frame boundary");
  CountingModeHost host;
  installProbe(*core, spider1::drawSync, 0u);
  installProbe(*core, spider1::fieldService, 0u);

  // FINISHED ON THE FIRST ASK: one field, one draw sync, and NO field service. A loop that serviced
  // a field after the GPU reported done would be pacing one field too many — the guest would wait
  // for a display that had already been presented.
  resetCalls();
  host.fieldsWaited = 0;
  spider1ModeDrainDrawFields(*core, host, 0u, 0u);
  CHECK_EQ(host.fieldsWaited, 1u);
  CHECK_EQ(countOf(g_calls, spider1::drawSync), 1u);
  CHECK_EQ(countOf(g_calls, spider1::fieldService), 0u);

  // NOT FINISHED TWICE: three fields, three draw syncs, and two field services — one for each field
  // the GPU was still busy through, and none for the field it finished on. Both counts together pin
  // the loop's shape: a rule that serviced one field too many, or one too few, fails one of them.
  resetCalls();
  g_notFinishedTimes[spider1::drawSync] = 2u;
  host.fieldsWaited = 0;
  spider1ModeDrainDrawFields(*core, host, 0u, 0u);
  CHECK_EQ(host.fieldsWaited, 3u);
  CHECK_EQ(countOf(g_calls, spider1::drawSync), 3u);
  CHECK_EQ(countOf(g_calls, spider1::fieldService), 2u);
  CHECK_EQ(g_probeMisses, 0u);
}

void test_frame_boundary_hands_the_frame_over_once_and_waits_only_when_no_field_passed(void) {
  const std::unique_ptr<Core> core = makeCore("frame boundary handshake");
  CountingModeHost host;
  installProbe(*core, spider1::fieldService, 0u);

  // The handshake word is what makes the field service happen ONCE: the frame body clears it, the
  // boundary arms it, and a second call must be a no-op rather than a second field.
  resetCalls();
  core->mem_w32(spider1::frameHandshake, 0u);
  spider1ModeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(core->mem_r32(spider1::frameHandshake), 1u);
  CHECK_EQ(g_calls.size(), 1u);
  spider1ModeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(g_calls.size(), 1u);
  // A body that cleared the word again DOES get its field, so the arm is not one-shot forever.
  core->mem_w32(spider1::frameHandshake, 0u);
  spider1ModeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(g_calls.size(), 2u);

  // THE FRAME'S OWN FIELD WAIT: a frame whose work already crossed a field presents without
  // waiting, and one that did not waits exactly one.
  core->mem_w32(spider1::gameVblankCount, 50u);
  host.fieldsWaited = 0;
  spider1ModeWaitUnadvancedField(*core, host, 50u);
  CHECK_EQ(host.fieldsWaited, 1u);
  core->mem_w32(spider1::gameVblankCount, 51u);
  host.fieldsWaited = 0;
  spider1ModeWaitUnadvancedField(*core, host, 50u);
  CHECK_EQ(host.fieldsWaited, 0u);
}

// ---- the host-stepped fiber --------------------------------------------------------------------

void test_fiber_reports_its_phase_and_its_outstanding_field(void) {
  const std::unique_ptr<Core> core = makeCore("fiber");
  Spider1HostSteppedFiber fiber;
  // A fiber that does not exist is NOT "done": reading it as finished is how a missing owner turns
  // into a silent success, so the two questions are asked separately.
  CHECK(!fiber.active());
  CHECK(!fiber.done());
  CHECK(fiber.phase() == Spider1FiberPhase::None);
  CHECK(!fiber.fieldWaitOutstanding());
  CHECK(!fiber.runningOn(*core));

  // A body that yields leaves the fiber alive and the wait OUTSTANDING, which is the fact the frame
  // driver's resume plan is built on.
  unsigned runs = 0;
  fiber.beginModeStep(*core, [&fiber, core = core.get(), &runs] {
    if (runs++ == 0u) {
      fiber.yieldField(*core, 0x00001234u);
    }
  });
  CHECK(fiber.active());
  CHECK(!fiber.done());
  CHECK(fiber.runningOn(*core));
  CHECK(fiber.phase() == Spider1FiberPhase::Mode);
  fiber.resume();
  CHECK(!fiber.done());
  CHECK(fiber.fieldWaitOutstanding());
  CHECK(!fiber.fieldSatisfied());

  // The outstanding wait is cleared before the next resume, and the next resume runs the body to
  // completion — which is the whole ping-pong the host step drives.
  fiber.clearFieldWait();
  CHECK(!fiber.fieldWaitOutstanding());
  fiber.resume();
  CHECK(fiber.done());
  CHECK(!fiber.fieldWaitOutstanding());

  // A mode fiber that finished is retired by dropping it; the phase goes back to none so a second
  // mode step may start.
  fiber.drop();
  CHECK(!fiber.active());
  CHECK(!fiber.done());
  CHECK(fiber.phase() == Spider1FiberPhase::None);
  fiber.beginModeStep(*core, [] {});
  CHECK(fiber.phase() == Spider1FiberPhase::Mode);
  fiber.resume();
  CHECK(fiber.done());
  fiber.drop();
}

int main(void) {
  RUN(field_clock_measures_since_the_previous_field);
  RUN(field_clock_rebases_only_after_the_value_was_read);
  RUN(gpu_reset_orders_its_steps_and_keeps_both_decisions);
  RUN(cd_initialization_installs_every_slot_and_keeps_the_guests_own_mask_bits);
  RUN(invalid_input_latches_a_press_before_it_accepts_a_release);
  RUN(frame_boundary_drains_fields_until_the_gpu_says_it_is_done);
  RUN(frame_boundary_hands_the_frame_over_once_and_waits_only_when_no_field_passed);
  RUN(fiber_reports_its_phase_and_its_outstanding_field);
  return pt_summary();
}
