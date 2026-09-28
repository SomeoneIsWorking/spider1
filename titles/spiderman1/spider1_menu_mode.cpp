#include "spider1_menu_mode.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_decisions.h"
#include "spider1_mode_frame_boundary.h"

#include "core.h"
#include "native_dispatch.h"

namespace spider {
namespace {

// The authenticated `jal` return addresses for this mode's own call sites, in call order. Each is
// the address the guest itself recorded when it called the entry, read out of the executable.
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

// The guest stack window both `begin` and `step` build their outgoing argument blocks in.
constexpr uint32_t kGuestStackBytes = 88u;

} // namespace

void Spider1MenuMode::begin(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  spider1CallGuest(core, spider1::padRead, kBeginPadReadReturnPc);
  core.r[4] = spider1::padState;
  spider1CallGuest(core, spider1::padReset, kBeginPadResetReturnPc);
  core.r[4] = spider1::uiDepth;
  spider1CallGuest(core, spider1::uiSetDepth, kBeginSetDepthReturnPc);

  // The menu paints its background by copying the OTHER display half into this one, so the menu
  // appears over the frame the transition just left rather than over a blank canvas.
  const uint32_t rect = stack.at(spider1::menuRectSlot);
  core.mem_w16(rect + spider1::menuRectX, 0);
  core.mem_w16(rect + spider1::menuRectY,
               core.mem_r32(spider1::currentDrawBuffer) == spider1::displayBuffer0
                   ? 0
                   : static_cast<uint16_t>(spider1::displayBufferHeight));
  core.mem_w16(rect + spider1::menuRectWidth, static_cast<uint16_t>(spider1::displayWidth));
  core.mem_w16(rect + spider1::menuRectHeight, static_cast<uint16_t>(spider1::displayBufferHeight));
  core.r[4] = rect;
  core.r[5] = 0;
  core.r[6] = core.mem_r16(rect + spider1::menuRectY) == 0
                  ? static_cast<uint32_t>(spider1::displayBufferHeight)
                  : 0u;
  spider1CallGuest(core, spider1::transitionCopyImage, kBeginCopyImageReturnPc);
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte18, 0);
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte90, 0);

  core.r[4] = spider1::menuAllocateBytes;
  spider1CallGuest(core, spider1::uiAllocate, kBeginAllocateReturnPc);
  object_ = core.r[2];
  if (object_ != 0) {
    core.r[4] = object_;
    core.r[5] = spider1::menuConstructWidth;
    core.r[6] = spider1::menuConstructHeight;
    core.r[7] = 0;
    const uint32_t construct = stack.at(spider1::menuConstructSlot);
    core.mem_w32(construct + spider1::menuConstructWidthSlot, spider1::menuConstructWidth);
    core.mem_w32(construct + spider1::menuConstructHeightSlot, spider1::menuConstructHeight);
    core.mem_w32(construct + spider1::kUnattributedMenuConstructWord3,
                 spider1::kUnattributedMenuConstructWord3Value);
    spider1CallGuest(core, spider1::uiConstruct, kBeginConstructReturnPc);
    object_ = core.r[2];
  }
  core.r[4] = object_;
  core.r[5] = core.mem_r32(spider1::uiTable + spider1::uiTableBindEntryFirst);
  spider1CallGuest(core, spider1::uiBind, kBeginBindFirstReturnPc);
  core.r[4] = object_;
  core.r[5] = core.mem_r32(spider1::uiTable + spider1::uiTableBindEntrySecond);
  spider1CallGuest(core, spider1::uiBind, kBeginBindSecondReturnPc);
  core.mem_w8(object_ + spider1::kUnattributedMenuObjectByte0B, 0);
  core.mem_w8(object_ + spider1::kUnattributedMenuObjectByte18, 1);
  fieldCount_ = 0;
  accepted_ = false;
  stack.restore();
}

Spider1MenuMode::Outcome Spider1MenuMode::step(Core &core) {
  const GuestStackFrame stack(core, kGuestStackBytes);
  spider1CallGuest(core, spider1::primaryPreFrame, kStepPreFrameReturnPc);
  spider1CallGuest(core, spider1::frameBegin, kStepFrameBeginReturnPc);
  spider1CallGuest(core, spider1::poolRotate, kStepPoolRotateReturnPc);
  const uint32_t vblankAtFrameStart = core.mem_r32(spider1::gameVblankCount);

  core.r[4] = object_;
  spider1CallGuest(core, spider1::uiUpdate, kStepUpdateReturnPc);
  core.r[4] = spider1::menuTextX;
  core.r[5] = spider1::menuTextY;
  core.r[6] = spider1::menuTextLength;
  core.r[7] = 0;
  spider1CallGuest(core, spider1::uiText, kStepTextReturnPc);
  core.r[4] = spider1::menuQuadX;
  core.r[5] = spider1::menuQuadY;
  core.r[6] = core.mem_r32(spider1::uiTable + spider1::uiTableQuadAssetRoot);
  core.r[7] = 0;
  core.mem_w32(stack.at(spider1::uiQuadSizeWordSlot), spider1::uiQuadSizeWord);
  spider1CallGuest(core, spider1::uiQuad, kStepQuadReturnPc);
  if (fieldCount_ < spider1::menuIntroEffectFrames) {
    core.r[4] = spider1::menuEffectX;
    core.r[5] = spider1::menuEffectY;
    spider1CallGuest(core, spider1::uiEffect, kStepIntroEffectReturnPc);
  }
  ++fieldCount_;
  spider1CallGuest(core, spider1::padRead, kStepPadReadReturnPc);
  const uint32_t input = stack.at(spider1::menuInputSlot);
  core.r[4] = input + spider1::menuInputButtonsSlot;
  core.r[5] = input + spider1::menuInputButtonsHeldSlot;
  core.r[6] = input + spider1::menuInputButtonsNewSlot;
  core.r[7] = input + spider1::menuInputButtonsReleasedSlot;
  spider1CallGuest(core, spider1::uiReadInput, kStepReadInputReturnPc);
  core.r[4] = object_;
  spider1CallGuest(core, spider1::uiFinishFrame, kStepFinishFrameReturnPc);

  if (core.mem_r32(input + spider1::menuInputButtonsSlot) != 0) {
    core.r[4] = spider1::menuExitSoundId;
    core.r[5] = spider1::exitSoundVolume;
    core.r[6] = 0;
    spider1CallGuest(core, spider1::uiSound, kStepSoundReturnPc);
    core.r[4] = object_;
    core.r[5] = core.mem_r32(spider1::uiTable + spider1::uiTableBindEntryFirst);
    spider1CallGuest(core, spider1::uiExitTest, kStepExitTestReturnPc);
    accepted_ = core.r[2] != 0;
    finish(core);
    stack.restore();
    return Outcome::Finished;
  }

  spider1ModeWaitUnadvancedField(core, host_, vblankAtFrameStart);
  core.mem_w32(spider1::frameHandshake, 0);
  spider1ModeDrainDrawFields(core, host_, kStepDrawSyncReturnPc, kStepFieldServiceReturnPc);
  spider1CallGuest(core, spider1::submitFrame, kStepSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  spider1ModeCompleteFrameHandshake(core, kStepHandshakeFieldServiceReturnPc);
  stack.restore();
  return Outcome::Continue;
}

void Spider1MenuMode::finish(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, spider1::oneField);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFinishDrawSyncReturnPc);
  spider1CallGuest(core, spider1::submitFrame, kFinishSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFinishTrailingDrawSyncReturnPc);

  // The menu's ending goes through the OBJECT'S OWN VTABLE, not through a fixed entry: the object's
  // first word is its vtable, slot `+8` is a signed halfword offset the object adds to itself, and
  // slot `+12` is the function that call enters. The object is given that resolved address as its
  // argument, so the guest's own class decides what ending this is.
  if (object_ != 0) {
    const uint32_t vtable = core.mem_r32(object_);
    core.r[4] = object_ + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(
                              core.mem_r16(vtable + spider1::menuVtableTargetOffsetSlot))));
    core.r[5] = spider1::menuVtableCallArgument;
    core.r[31] = kFinishVtableReturnPc;
    psx::cpu::dispatchGuestToReturn(core,
                                    core.mem_r32(vtable + spider1::menuVtableEntrySlot),
                                    psx::cpu::ExecutionBudget::currentTurn(core),
                                    "Spider-Man menu vtable synchronous guest call");
  }
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte18, 1);
  core.mem_w8(spider1::displayBuffer0 + spider1::displayBufferTableByte90, 1);
  spider1CallGuest(core, spider1::frameBegin, kFinishFrameBeginReturnPc);
  core.mem_w32(spider1::asyncModeState, spider1::asyncModeStateAfterMenu);
}

} // namespace spider
