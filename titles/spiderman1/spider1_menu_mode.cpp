#include "spider1_menu_mode.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_decisions.h"
#include "spider1_mode_frame_boundary.h"

#include "core.h"
#include "native_dispatch.h"

namespace spider::spider1 {
namespace {

// `jal` return addresses of this mode's own call sites, in call order.
constexpr uint32_t kBeginPadReadReturnPc = 0x80016114u;
constexpr uint32_t kBeginPadResetReturnPc = 0x80016120u;
constexpr uint32_t kBeginSetDepthReturnPc = 0x80016128u;
constexpr uint32_t kBeginCopyImageReturnPc = 0x80016174u;
constexpr uint32_t kBeginAllocateReturnPc = 0x80016184u;
constexpr uint32_t kBeginConstructReturnPc = 0x800161B4u;
constexpr uint32_t kBeginBindFirstReturnPc = 0x800161D8u;
constexpr uint32_t kBeginBindSecondReturnPc = 0x800161E4u;

constexpr uint32_t kStepPreFrameReturnPc = 0x800161F8u;
constexpr uint32_t kStepFrameBeginReturnPc = 0x80016200u;
constexpr uint32_t kStepPoolRotateReturnPc = 0x80016208u;
constexpr uint32_t kStepUpdateReturnPc = 0x8001621Cu;
constexpr uint32_t kStepTextReturnPc = 0x80016230u;
constexpr uint32_t kStepQuadReturnPc = 0x8001624Cu;
constexpr uint32_t kStepIntroEffectReturnPc = 0x80016264u;
constexpr uint32_t kStepPadReadReturnPc = 0x8001626Cu;
constexpr uint32_t kStepReadInputReturnPc = 0x80016280u;
constexpr uint32_t kStepFinishFrameReturnPc = 0x80016288u;
constexpr uint32_t kStepSoundReturnPc = 0x800162A4u;
constexpr uint32_t kStepExitTestReturnPc = 0x800162B0u;
constexpr uint32_t kStepDrawSyncReturnPc = 0x80016304u;
constexpr uint32_t kStepFieldServiceReturnPc = 0x800162F4u;
constexpr uint32_t kStepSubmitReturnPc = 0x80016314u;
constexpr uint32_t kStepHandshakeFieldServiceReturnPc = 0x80016330u;

constexpr uint32_t kFinishDrawSyncReturnPc = 0x80016354u;
constexpr uint32_t kFinishSubmitReturnPc = 0x8001635Cu;
constexpr uint32_t kFinishTrailingDrawSyncReturnPc = 0x80016364u;
constexpr uint32_t kFinishVtableReturnPc = 0x80016388u;
constexpr uint32_t kFinishFrameBeginReturnPc = 0x8001639Cu;

// Guest stack window `begin` and `step` build their outgoing argument blocks in.
constexpr uint32_t kGuestStackBytes = 88u;

} // namespace

void Spider1MenuMode::begin(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  enterGuestCall(core, padRead, kBeginPadReadReturnPc);
  core.r[4] = padState;
  enterGuestCall(core, padReset, kBeginPadResetReturnPc);
  core.r[4] = uiDepth;
  enterGuestCall(core, uiSetDepth, kBeginSetDepthReturnPc);

  // Copy the other display half into this one so the menu appears over the frame the transition
  // left.
  const uint32_t rect = stack.at(menuRectSlot);
  core.mem_w16(rect + menuRectX, 0);
  core.mem_w16(rect + menuRectY,
               core.mem_r32(currentDrawBuffer) == displayBuffer0
                   ? 0
                   : static_cast<uint16_t>(displayBufferHeight));
  core.mem_w16(rect + menuRectWidth, static_cast<uint16_t>(displayWidth));
  core.mem_w16(rect + menuRectHeight, static_cast<uint16_t>(displayBufferHeight));
  core.r[4] = rect;
  core.r[5] = 0;
  core.r[6] = core.mem_r16(rect + menuRectY) == 0 ? static_cast<uint32_t>(displayBufferHeight) : 0u;
  enterGuestCall(core, transitionCopyImage, kBeginCopyImageReturnPc);
  core.mem_w8(displayBuffer0 + displayBufferTableByte18, 0);
  core.mem_w8(displayBuffer0 + displayBufferTableByte90, 0);

  core.r[4] = menuAllocateBytes;
  enterGuestCall(core, uiAllocate, kBeginAllocateReturnPc);
  object_ = core.r[2];
  if (object_ != 0) {
    core.r[4] = object_;
    core.r[5] = menuConstructWidth;
    core.r[6] = menuConstructHeight;
    core.r[7] = 0;
    const uint32_t construct = stack.at(menuConstructSlot);
    core.mem_w32(construct + menuConstructWidthSlot, menuConstructWidth);
    core.mem_w32(construct + menuConstructHeightSlot, menuConstructHeight);
    core.mem_w32(construct + kUnattributedMenuConstructWord3, kUnattributedMenuConstructWord3Value);
    enterGuestCall(core, uiConstruct, kBeginConstructReturnPc);
    object_ = core.r[2];
  }
  core.r[4] = object_;
  core.r[5] = core.mem_r32(uiTable + uiTableBindEntryFirst);
  enterGuestCall(core, uiBind, kBeginBindFirstReturnPc);
  core.r[4] = object_;
  core.r[5] = core.mem_r32(uiTable + uiTableBindEntrySecond);
  enterGuestCall(core, uiBind, kBeginBindSecondReturnPc);
  core.mem_w8(object_ + kUnattributedMenuObjectByte0B, 0);
  core.mem_w8(object_ + kUnattributedMenuObjectByte18, 1);
  fieldCount_ = 0;
  accepted_ = false;
  stack.restore();
}

Spider1MenuMode::Outcome Spider1MenuMode::step(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  enterGuestCall(core, primaryPreFrame, kStepPreFrameReturnPc);
  enterGuestCall(core, frameBegin, kStepFrameBeginReturnPc);
  enterGuestCall(core, poolRotate, kStepPoolRotateReturnPc);
  const uint32_t vblankAtFrameStart = core.mem_r32(gameVblankCount);

  core.r[4] = object_;
  enterGuestCall(core, uiUpdate, kStepUpdateReturnPc);
  core.r[4] = menuTextX;
  core.r[5] = menuTextY;
  core.r[6] = menuTextLength;
  core.r[7] = 0;
  enterGuestCall(core, uiText, kStepTextReturnPc);
  core.r[4] = menuQuadX;
  core.r[5] = menuQuadY;
  core.r[6] = core.mem_r32(uiTable + uiTableQuadAssetRoot);
  core.r[7] = 0;
  core.mem_w32(stack.at(uiQuadSizeWordSlot), uiQuadSizeWord);
  enterGuestCall(core, uiQuad, kStepQuadReturnPc);
  if (fieldCount_ < menuIntroEffectFrames) {
    core.r[4] = menuEffectX;
    core.r[5] = menuEffectY;
    enterGuestCall(core, uiEffect, kStepIntroEffectReturnPc);
  }
  ++fieldCount_;
  enterGuestCall(core, padRead, kStepPadReadReturnPc);
  const uint32_t input = stack.at(menuInputSlot);
  core.r[4] = input + menuInputButtonsSlot;
  core.r[5] = input + menuInputButtonsHeldSlot;
  core.r[6] = input + menuInputButtonsNewSlot;
  core.r[7] = input + menuInputButtonsReleasedSlot;
  enterGuestCall(core, uiReadInput, kStepReadInputReturnPc);
  core.r[4] = object_;
  enterGuestCall(core, uiFinishFrame, kStepFinishFrameReturnPc);

  if (core.mem_r32(input + menuInputButtonsSlot) != 0) {
    core.r[4] = menuExitSoundId;
    core.r[5] = exitSoundVolume;
    core.r[6] = 0;
    enterGuestCall(core, uiSound, kStepSoundReturnPc);
    core.r[4] = object_;
    core.r[5] = core.mem_r32(uiTable + uiTableBindEntryFirst);
    enterGuestCall(core, uiExitTest, kStepExitTestReturnPc);
    accepted_ = core.r[2] != 0;
    finish(core);
    stack.restore();
    return Outcome::Finished;
  }

  modeWaitUnadvancedField(core, host_, vblankAtFrameStart);
  core.mem_w32(frameHandshake, 0);
  modeDrainDrawFields(core, host_, kStepDrawSyncReturnPc, kStepFieldServiceReturnPc);
  enterGuestCall(core, submitFrame, kStepSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  modeCompleteFrameHandshake(core, kStepHandshakeFieldServiceReturnPc);
  stack.restore();
  return Outcome::Continue;
}

void Spider1MenuMode::finish(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, oneField);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFinishDrawSyncReturnPc);
  enterGuestCall(core, submitFrame, kFinishSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFinishTrailingDrawSyncReturnPc);

  // The ending goes through the object's vtable: +8 is a signed halfword offset added to the
  // object, +12 is the function entered, and the object is passed that resolved address.
  if (object_ != 0) {
    const uint32_t vtable = core.mem_r32(object_);
    core.r[4] = object_ + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(
                              core.mem_r16(vtable + menuVtableTargetOffsetSlot))));
    core.r[5] = menuVtableCallArgument;
    core.r[31] = kFinishVtableReturnPc;
    psx::cpu::dispatchGuestToReturn(core,
                                    core.mem_r32(vtable + menuVtableEntrySlot),
                                    psx::cpu::ExecutionBudget::currentTurn(core),
                                    "Spider-Man menu vtable synchronous guest call");
  }
  core.mem_w8(displayBuffer0 + displayBufferTableByte18, 1);
  core.mem_w8(displayBuffer0 + displayBufferTableByte90, 1);
  enterGuestCall(core, frameBegin, kFinishFrameBeginReturnPc);
  core.mem_w32(asyncModeState, asyncModeStateAfterMenu);
}

} // namespace spider::spider1
