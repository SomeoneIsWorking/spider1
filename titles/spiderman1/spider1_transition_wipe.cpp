#include "spider1_transition_wipe.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_host.h"

#include "core.h"

namespace spider::spider1 {
namespace {

// Restored explicitly on return because a wipe field can be suspended by a yield.
constexpr uint32_t kGuestStackBytes = 72u;

// `jal` return addresses of the wipe's call sites, in call order.
constexpr uint32_t kFirstDrawSyncReturnPc = 0x800604ECu;
constexpr uint32_t kCopyImageReturnPc = 0x80060534u;
constexpr uint32_t kDrawSyncReturnPc = 0x8006053Cu;
constexpr uint32_t kFirstFrameBeginReturnPc = 0x80060550u;
constexpr uint32_t kFirstSubmitReturnPc = 0x80060558u;
constexpr uint32_t kAllocateReturnPc = 0x80060568u;
constexpr uint32_t kStoreRowReturnPc = 0x800605C8u;
constexpr uint32_t kLoadRowReturnPc = 0x800606D4u;
constexpr uint32_t kReleaseReturnPc = 0x800606F8u;
constexpr uint32_t kSecondFrameBeginReturnPc = 0x80060700u;
constexpr uint32_t kSecondSubmitReturnPc = 0x80060708u;
constexpr uint32_t kThirdFrameBeginReturnPc = 0x80060710u;

uint32_t darkenWord(uint32_t word) {
  return static_cast<uint32_t>(Spider1TransitionWipe::darkenPixel(static_cast<uint16_t>(word))) |
         (static_cast<uint32_t>(
              Spider1TransitionWipe::darkenPixel(static_cast<uint16_t>(word >> 16u)))
          << 16u);
}

} // namespace

Spider1TransitionWipe::Spider1TransitionWipe(Spider1ModeHost &host) : host_(host) {}

void Spider1TransitionWipe::begin(Destination continuation, uint16_t startOffset) {
  continuation_ = continuation;
  startOffset_ = startOffset;
}

uint16_t Spider1TransitionWipe::darkenPixel(uint16_t pixel) {
  // Each channel becomes a share of the sum of the three; the shares differ, so the picture reddens
  // as it darkens.
  const uint32_t sum = (pixel & transitionChannelMask) + ((pixel >> 5u) & transitionChannelMask) +
                       ((pixel >> 10u) & transitionChannelMask);
  const uint32_t dim = (transitionDimNumerator * sum) >> transitionDimShift;
  const uint32_t strong = (transitionStrongNumerator * sum) >> transitionStrongShift;
  return static_cast<uint16_t>((pixel & transitionSignBit) | (strong << 10u) | (dim << 5u) | dim);
}

void Spider1TransitionWipe::stepFirst(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFirstDrawSyncReturnPc);
  // The rect's y is the other half's offset: it copies the frame being left.
  const uint32_t rect = stack.at(transitionRectSlot);
  core.mem_w16(rect + transitionRectX, 0);
  core.mem_w16(rect + transitionRectY, startOffset_);
  core.mem_w16(rect + transitionRectWidth, static_cast<uint16_t>(displayWidth));
  core.mem_w16(rect + transitionRectHeight, static_cast<uint16_t>(displayHeight));
  core.r[4] = rect;
  core.r[5] = 0;
  core.r[6] = startOffset_ == 0 ? static_cast<uint32_t>(displayBufferHeight) : 0u;
  enterGuestCall(core, transitionCopyImage, kCopyImageReturnPc);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kDrawSyncReturnPc);
  host_.waitFields(core, oneField);
  core.mem_w8(displayBuffer0 + displayBufferTableByte18, 0);
  core.mem_w8(displayBuffer0 + displayBufferTableByte90, 0);
  enterGuestCall(core, frameBegin, kFirstFrameBeginReturnPc);
  enterGuestCall(core, submitFrame, kFirstSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  stack.restore();
}

Spider1TransitionWipe::Destination Spider1TransitionWipe::stepSecond(Core &core) {
  {
    const GuestStackFrame stack(core, kGuestStackBytes);
    // The scratch is four rows deep so a row can be darkened while the GPU still holds the next.
    core.r[4] = transitionScratchBytes;
    core.r[5] = 0;
    core.r[6] = 1;
    enterGuestCall(core, guestAllocate, kAllocateReturnPc);
    const uint32_t pixels = core.r[2];
    const uint32_t row = stack.at(transitionRowSlot);
    core.mem_w16(row + transitionRowRect, 0);
    core.mem_w16(row + transitionRowY, startOffset_);
    core.mem_w16(row + transitionRowWidth, static_cast<uint16_t>(displayWidth));
    core.mem_w16(row + transitionRowCount, 1);
    for (uint32_t line = 0; line < displayHeight; ++line) {
      const uint32_t rowPixels = pixels + (line % transitionRowGroupRows) * transitionRowGroupBytes;
      core.r[4] = row;
      core.r[5] = rowPixels;
      enterGuestCall(core, transitionStoreRow, kStoreRowReturnPc);
      for (uint32_t word = 0; word < transitionRowWords; ++word) {
        const uint32_t address = rowPixels + word * 4u;
        core.mem_w32(address, darkenWord(core.mem_r32(address)));
      }
      core.r[4] = row;
      core.r[5] = rowPixels;
      enterGuestCall(core, transitionLoadRow, kLoadRowReturnPc);
      core.mem_w16(row + transitionRowY,
                   static_cast<uint16_t>(core.mem_r16(row + transitionRowY) + 1u));
    }
    core.r[4] = pixels;
    enterGuestCall(core, guestRelease, kReleaseReturnPc);
    enterGuestCall(core, frameBegin, kSecondFrameBeginReturnPc);
    enterGuestCall(core, submitFrame, kSecondSubmitReturnPc);
    host_.commitSubmittedFrame(core);
    enterGuestCall(core, frameBegin, kThirdFrameBeginReturnPc);
    core.mem_w8(displayBuffer0 + displayBufferTableByte18, 1);
    core.mem_w8(displayBuffer0 + displayBufferTableByte90, 1);
    core.mem_w32(asyncModeState, asyncModeStateAfterWipe);
    stack.restore();
  }
  return continuation_;
}

} // namespace spider::spider1
