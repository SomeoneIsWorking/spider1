#include "spider1_transition_wipe.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_host.h"

#include "core.h"

namespace spider {
namespace {

// The guest stack window the wipe builds its outgoing argument blocks in, restored explicitly on
// the normal return because a wipe field can be suspended by a yield.
constexpr uint32_t kGuestStackBytes = 72u;

// The authenticated `jal` return addresses for the wipe's own call sites, in call order.
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

// One 32-bit word through the wipe's darkening: two pixels, each darkened on its own.
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
  // Each channel's new value is a share of the SUM of the three old ones, and the two shares
  // differ, so the red channel keeps the smaller of them and the picture reddens as it darkens.
  const uint32_t sum = (pixel & spider1::transitionChannelMask) +
                       ((pixel >> 5u) & spider1::transitionChannelMask) +
                       ((pixel >> 10u) & spider1::transitionChannelMask);
  const uint32_t dim = (spider1::transitionDimNumerator * sum) >> spider1::transitionDimShift;
  const uint32_t strong =
      (spider1::transitionStrongNumerator * sum) >> spider1::transitionStrongShift;
  return static_cast<uint16_t>((pixel & spider1::transitionSignBit) | (strong << 10u) |
                               (dim << 5u) | dim);
}

void Spider1TransitionWipe::stepFirst(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFirstDrawSyncReturnPc);
  // The rect copies the frame the wipe is LEAVING, and its y is the OTHER half's offset: a wipe
  // that starts on the upper half reads from the lower one and vice versa.
  const uint32_t rect = stack.at(spider1::transitionRectSlot);
  core.mem_w16(rect + spider1::transitionRectX, 0);
  core.mem_w16(rect + spider1::transitionRectY, startOffset_);
  core.mem_w16(rect + spider1::transitionRectWidth, static_cast<uint16_t>(spider1::displayWidth));
  core.mem_w16(rect + spider1::transitionRectHeight, static_cast<uint16_t>(spider1::displayHeight));
  core.r[4] = rect;
  core.r[5] = 0;
  core.r[6] = startOffset_ == 0 ? static_cast<uint32_t>(spider1::displayBufferHeight) : 0u;
  spider1CallGuest(core, spider1::transitionCopyImage, kCopyImageReturnPc);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kDrawSyncReturnPc);
  host_.waitFields(core, spider1::oneField);
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte18, 0);
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte90, 0);
  spider1CallGuest(core, spider1::frameBegin, kFirstFrameBeginReturnPc);
  spider1CallGuest(core, spider1::submitFrame, kFirstSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  stack.restore();
}

Spider1TransitionWipe::Destination Spider1TransitionWipe::stepSecond(Core &core) {
  {
    const GuestStackFrame stack(core, kGuestStackBytes);
    // The scratch is four rows deep, so a row can be read back and darkened while the GPU still
    // holds the next one. That is why the row's address is the line number modulo four times a
    // whole group's bytes, and why the inner loop is a whole group's WORDS.
    core.r[4] = spider1::transitionScratchBytes;
    core.r[5] = 0;
    core.r[6] = 1;
    spider1CallGuest(core, spider1::guestAllocate, kAllocateReturnPc);
    const uint32_t pixels = core.r[2];
    const uint32_t row = stack.at(spider1::transitionRowSlot);
    core.mem_w16(row + spider1::transitionRowRect, 0);
    core.mem_w16(row + spider1::transitionRowY, startOffset_);
    core.mem_w16(row + spider1::transitionRowWidth, static_cast<uint16_t>(spider1::displayWidth));
    core.mem_w16(row + spider1::transitionRowCount, 1);
    for (uint32_t line = 0; line < spider1::displayHeight; ++line) {
      const uint32_t rowPixels =
          pixels + (line % spider1::transitionRowGroupRows) * spider1::transitionRowGroupBytes;
      core.r[4] = row;
      core.r[5] = rowPixels;
      spider1CallGuest(core, spider1::transitionStoreRow, kStoreRowReturnPc);
      for (uint32_t word = 0; word < spider1::transitionRowWords; ++word) {
        const uint32_t address = rowPixels + word * 4u;
        core.mem_w32(address, darkenWord(core.mem_r32(address)));
      }
      core.r[4] = row;
      core.r[5] = rowPixels;
      spider1CallGuest(core, spider1::transitionLoadRow, kLoadRowReturnPc);
      core.mem_w16(row + spider1::transitionRowY,
                   static_cast<uint16_t>(core.mem_r16(row + spider1::transitionRowY) + 1u));
    }
    core.r[4] = pixels;
    spider1CallGuest(core, spider1::guestRelease, kReleaseReturnPc);
    spider1CallGuest(core, spider1::frameBegin, kSecondFrameBeginReturnPc);
    spider1CallGuest(core, spider1::submitFrame, kSecondSubmitReturnPc);
    host_.commitSubmittedFrame(core);
    spider1CallGuest(core, spider1::frameBegin, kThirdFrameBeginReturnPc);
    core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte18, 1);
    core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte90, 1);
    core.mem_w32(spider1::asyncModeState, spider1::asyncModeStateAfterWipe);
    stack.restore();
  }
  return continuation_;
}

} // namespace spider
