// Guest-boundary owners driven against a bare Core with native-override probes.

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

using namespace spider::spider1;

namespace {

// Resident image over main RAM in physical terms; nothing executes the bytes.
constexpr uint32_t kResidentStart = 0x00010000u;
constexpr uint32_t kResidentBytes = 0x00200000u;

// Core is 2 MB of RAM, so it lives on the heap.
std::unique_ptr<Core> makeCore(const char *name) {
  auto core = std::make_unique<Core>();
  core->imageCatalog().activate(name, {kResidentStart, kResidentStart + kResidentBytes}, 1u);
  return core;
}

std::vector<uint32_t> g_calls;
std::map<uint32_t, uint32_t> g_results;
// Entries report "not finished" a fixed number of times; the drain loop is unbounded.
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

void test_field_clock_measures_since_the_previous_field(void) {
  const std::unique_ptr<Core> core = makeCore("field clock");
  // A null counter pointer reads as zero, as the retail load does.
  core->mem_w32(horizontalCounterPointer, 0u);
  CHECK_EQ(Spider1FieldClock::horizontalCounter(*core), 0u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0u);

  // Returns the advance since the last field, truncated to 16 bits.
  core->mem_w32(horizontalCounterPointer, 0x800A0000u);
  core->mem_w32(0x800A0000u, 1000u);
  core->mem_w32(horizontalCounterBaseline, 900u);
  CHECK_EQ(Spider1FieldClock::horizontalCounter(*core), 1000u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 100u);

  // The 16-bit wrap holds both ways: counter wrapped, and baseline ahead of counter.
  core->mem_w32(0x800A0000u, 5u);
  core->mem_w32(horizontalCounterBaseline, 0xFFFFu);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 6u);
  core->mem_w32(0x800A0000u, 0xFFF0u);
  core->mem_w32(horizontalCounterBaseline, 0x0010u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0xFFE0u);
}

void test_field_clock_rebases_only_after_the_value_was_read(void) {
  const std::unique_ptr<Core> core = makeCore("field clock rebase");
  core->mem_w32(horizontalCounterPointer, 0x800A0000u);
  core->mem_w32(0x800A0000u, 4242u);
  core->mem_w32(horizontalCounterBaseline, 4000u);
  core->mem_w32(libetcVblankCountAddress, 77u);

  const uint32_t returnValue = Spider1FieldClock::vsyncReturnValue(*core);
  CHECK_EQ(returnValue, 242u);
  Spider1FieldClock::recordDeliveredField(*core);
  // Recording rebases the counter; the value is read before the record.
  CHECK_EQ(core->mem_r32(lastVsyncField), 77u);
  CHECK_EQ(core->mem_r32(horizontalCounterBaseline), 4242u);
  CHECK_EQ(Spider1FieldClock::vsyncReturnValue(*core), 0u);
  CHECK_EQ(Spider1FieldClock::fieldCount(*core), 77u);
}

void test_gpu_reset_orders_its_steps_and_keeps_both_decisions(void) {
  const std::unique_ptr<Core> core = makeCore("gpu reset");
  // Addresses are deliberately non-monotonic; the asserted order is the retail order.
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
  // The read-mode probe reports display mode 1 by default.
  g_results[kReadMode] = 1u;
  g_results[kProbe] = 1u;

  // Mode 1, GPU reset: eight steps plus the finalize, finalize last.
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

  // Mode 0 takes no finalize; the rule uses the mode read back before the reset.
  g_results[kReadMode] = 0u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 0u));
  CHECK_EQ(g_calls.size(), 8u);
  CHECK_EQ(countOf(g_calls, kFinalize), 0u);

  // Reset not reported: the requested mode is discarded and apply gets zero.
  g_results[kReadMode] = 1u;
  g_results[kProbe] = 0u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 2u));
  CHECK_EQ(core->r[4], 0u);
  // The request survives when the probe reports reset.
  g_results[kProbe] = 1u;
  resetCalls();
  CHECK(Spider1GpuReset::reset(*core, 2u));
  CHECK_EQ(core->r[4], 2u);
  CHECK_EQ(g_probeMisses, 0u);
}

void test_cd_initialization_installs_every_slot_and_keeps_the_guests_own_mask_bits(void) {
  // Slot half against real guest RAM: four slots plus the success result.
  const std::unique_ptr<Core> core = makeCore("cd init");
  Spider1CdInitialization::installCallbackSlots(*core);
  CHECK_EQ(core->mem_r32(cdSyncCallbackSlot), cdSyncCallback);
  CHECK_EQ(core->mem_r32(cdReadyCallbackSlot), cdReadyCallback);
  CHECK_EQ(core->mem_r32(cdEventCallbackSlot), cdEventCallback);
  CHECK_EQ(core->mem_r32(cdEventUnusedSlot), 0u);
  CHECK_EQ(core->r[2], 1u);
  // A second run writes the same constants.
  Spider1CdInitialization::installCallbackSlots(*core);
  CHECK_EQ(core->mem_r32(cdSyncCallbackSlot), cdSyncCallback);
  CHECK_EQ(core->mem_r32(cdReadyCallbackSlot), cdReadyCallback);
  CHECK_EQ(core->mem_r32(cdEventCallbackSlot), cdEventCallback);
  CHECK_EQ(core->mem_r32(cdEventUnusedSlot), 0u);

  // 0x8008BBD0 ORs the source bit into the enable mask (0x800B28B4) and I_MASK; never clears a bit.
  const uint32_t cdBit = 1u << guestInterruptSourceCdRom;
  unsigned enablePreserved = 0;
  unsigned maskPreserved = 0;
  unsigned enableArmed = 0;
  unsigned maskArmed = 0;
  for (uint32_t before = 0; before < 0x200u; ++before) {
    const uint32_t after = before | cdBit;
    enablePreserved += (after & before) == before ? 1u : 0u;
    maskPreserved += (after & before) == before ? 1u : 0u;
    enableArmed += after != before ? 1u : 0u;
    maskArmed += after != before ? 1u : 0u;
  }
  CHECK_EQ(enablePreserved, 0x200u);
  CHECK_EQ(maskPreserved, 0x200u);
  CHECK_EQ(enableArmed, 0x200u - (0x200u / 2u));
  CHECK_EQ(maskArmed, 0x200u - (0x200u / 2u));
  // Source 0 and 3 give 0x0009 -> 0x000D; the CD-ROM source is bit 2 (0x4).
  CHECK_EQ(0x0009u | cdBit, 0x000Du);
  CHECK_EQ(0x009u | cdBit, 0x00Du);
  CHECK_EQ((0x009u | cdBit) & 0x009u, 0x009u);
  // A widened mask keeps everything it had.
  CHECK_EQ((0x0C9u | cdBit) & 0x0C8u, 0x0C8u);
  // An already-armed mask is the no-change case.
  CHECK_EQ(0x00Du | cdBit, 0x00Du);

  // The registration names the handler in slot 2 of the table at 0x800B2888.
  CHECK_EQ(guestInterruptHandlerTable + guestInterruptSourceCdRom * 4u, 0x800B2890u);
  CHECK_EQ(guestCdRomInterruptHandler, 0x8008DA24u);
  CHECK_EQ(guestInterruptSourceCdRom, 2u);
  CHECK_EQ(cdBit, interruptMaskCdBit);
}

namespace {

// Pad read that stages the two bytes the gate reads.
struct PadStage {
  static uint8_t secondKey;
  static uint8_t firstKey;
  static void read(Core *core) {
    core->mem_w8(padState + padStateByteE0, secondKey);
    core->mem_w8(padState + padStateByte30, firstKey);
  }
};

uint8_t PadStage::secondKey = 0;
uint8_t PadStage::firstKey = 0;

} // namespace

void test_invalid_input_latches_a_press_before_it_accepts_a_release(void) {
  const std::unique_ptr<Core> core = makeCore("invalid input");
  psx::cpu::installNativeOverride(*core, padRead, "test pad read", &PadStage::read);
  psx::cpu::installNativeOverride(*core, invalidWaitSetup, "test invalid setup", [](Core *c) {
    c->r[2] = 0;
  });
  // The timeout is measured from the field the gate started on.
  core->mem_w32(gameVblankCount, 10u);
  Spider1InvalidSelectorInput gate;
  gate.begin(*core, invalidInputWaitFields);
  CHECK_EQ(gate.startField(), 10u);

  // A byte that is always non-zero never opens the gate.
  PadStage::secondKey = 1;
  PadStage::firstKey = 1;
  for (uint32_t poll = 0; poll < 8u; ++poll) {
    CHECK(!gate.poll(*core));
  }
  CHECK_EQ(gate.lastSecondKeyByte(), 1u);
  CHECK_EQ(gate.lastFirstKeyByte(), 1u);

  // Reading zero latches the press; held zero is not a release.
  PadStage::secondKey = 0;
  CHECK(!gate.poll(*core));
  CHECK(!gate.poll(*core));

  // The first later non-zero poll opens the gate.
  PadStage::secondKey = 1;
  CHECK(gate.poll(*core));

  // Times out at the boundary field, not before.
  Spider1InvalidSelectorInput timing;
  PadStage::secondKey = 0;
  PadStage::firstKey = 0;
  core->mem_w32(gameVblankCount, 100u);
  timing.begin(*core, invalidInputWaitFields);
  CHECK_EQ(timing.startField(), 100u);
  core->mem_w32(gameVblankCount, 100u + invalidInputTimeoutFields - 1u);
  CHECK(!timing.poll(*core));
  core->mem_w32(gameVblankCount, 100u + invalidInputTimeoutFields);
  CHECK(timing.poll(*core));
}

namespace {

// Mode host that counts fields consumed and guest steps taken.
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
  installProbe(*core, drawSync, 0u);
  installProbe(*core, fieldService, 0u);

  // Finished on the first ask: one field, one draw sync, no field service.
  resetCalls();
  host.fieldsWaited = 0;
  modeDrainDrawFields(*core, host, 0u, 0u);
  CHECK_EQ(host.fieldsWaited, 1u);
  CHECK_EQ(countOf(g_calls, drawSync), 1u);
  CHECK_EQ(countOf(g_calls, fieldService), 0u);

  // Not finished twice: three fields, three draw syncs, two field services.
  resetCalls();
  g_notFinishedTimes[drawSync] = 2u;
  host.fieldsWaited = 0;
  modeDrainDrawFields(*core, host, 0u, 0u);
  CHECK_EQ(host.fieldsWaited, 3u);
  CHECK_EQ(countOf(g_calls, drawSync), 3u);
  CHECK_EQ(countOf(g_calls, fieldService), 2u);
  CHECK_EQ(g_probeMisses, 0u);
}

void test_frame_boundary_hands_the_frame_over_once_and_waits_only_when_no_field_passed(void) {
  const std::unique_ptr<Core> core = makeCore("frame boundary handshake");
  CountingModeHost host;
  installProbe(*core, fieldService, 0u);

  // The handshake word makes the field service run once; a second call is a no-op.
  resetCalls();
  core->mem_w32(frameHandshake, 0u);
  modeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(core->mem_r32(frameHandshake), 1u);
  CHECK_EQ(g_calls.size(), 1u);
  modeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(g_calls.size(), 1u);
  // A body that clears the word again gets its field.
  core->mem_w32(frameHandshake, 0u);
  modeCompleteFrameHandshake(*core, 0u);
  CHECK_EQ(g_calls.size(), 2u);

  // A frame that already crossed a field presents without waiting; otherwise it waits one.
  core->mem_w32(gameVblankCount, 50u);
  host.fieldsWaited = 0;
  modeWaitUnadvancedField(*core, host, 50u);
  CHECK_EQ(host.fieldsWaited, 1u);
  core->mem_w32(gameVblankCount, 51u);
  host.fieldsWaited = 0;
  modeWaitUnadvancedField(*core, host, 50u);
  CHECK_EQ(host.fieldsWaited, 0u);
}

void test_fiber_reports_its_phase_and_its_outstanding_field(void) {
  const std::unique_ptr<Core> core = makeCore("fiber");
  Spider1HostSteppedFiber fiber;
  // A missing fiber is not "done"; the two questions are separate.
  CHECK(!fiber.active());
  CHECK(!fiber.done());
  CHECK(fiber.phase() == Spider1FiberPhase::None);
  CHECK(!fiber.fieldWaitOutstanding());
  CHECK(!fiber.runningOn(*core));

  // A yielding body leaves the fiber alive with the wait outstanding.
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

  // The wait is cleared before resume, and the next resume runs the body to completion.
  fiber.clearFieldWait();
  CHECK(!fiber.fieldWaitOutstanding());
  fiber.resume();
  CHECK(fiber.done());
  CHECK(!fiber.fieldWaitOutstanding());

  // A finished fiber is retired by dropping it; phase returns to none.
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
