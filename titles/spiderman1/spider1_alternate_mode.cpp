#include "spider1_alternate_mode.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_decisions.h"
#include "spider1_mode_frame_boundary.h"

#include "core.h"

namespace spider::spider1 {
namespace {

// Authenticated `jal` return addresses for this mode's call sites, in call order.
constexpr uint32_t kBeginReturnPc = 0x8006F2FCu;
constexpr uint32_t kStepPreFrameReturnPc = 0x8006F304u;
constexpr uint32_t kStepFrameBeginReturnPc = 0x8006F30Cu;
constexpr uint32_t kStepPoolRotateReturnPc = 0x8006F314u;
constexpr uint32_t kStepEffectReturnPc = 0x8006F32Cu;
constexpr uint32_t kStepSetDepthReturnPc = 0x8006F334u;
constexpr uint32_t kStepUpdateReturnPc = 0x8006F344u;
constexpr uint32_t kStepTextReturnPc = 0x8006F358u;
constexpr uint32_t kStepQuadReturnPc = 0x8006F374u;
constexpr uint32_t kStepOverlayReturnPc = 0x8006F380u;
constexpr uint32_t kStepPadReadReturnPc = 0x8006F388u;
constexpr uint32_t kStepFinishFrameReturnPc = 0x8006F3B0u;
constexpr uint32_t kStepSoundReturnPc = 0x8006F3ECu;
constexpr uint32_t kStepDrawSyncReturnPc = 0x8006F438u;
constexpr uint32_t kStepFieldServiceReturnPc = 0x8006F428u;
constexpr uint32_t kStepSubmitReturnPc = 0x8006F448u;
constexpr uint32_t kStepHandshakeFieldServiceReturnPc = 0x8006F464u;

constexpr uint32_t kFinishDrawSyncReturnPc = 0x8006F484u;
constexpr uint32_t kFinishSubmitReturnPc = 0x8006F48Cu;
constexpr uint32_t kFinishTrailingDrawSyncReturnPc = 0x8006F494u;
constexpr uint32_t kFinishOuterResetReturnPc = 0x8006F49Cu;
constexpr uint32_t kFinishPostResetDrawSyncReturnPc = 0x8006F4A4u;
constexpr uint32_t kFinishPadResetReturnPc = 0x8006F4ACu;
constexpr uint32_t kFinishReleaseReturnPc = 0x8006F4BCu;
constexpr uint32_t kFinishOpenFileReturnPc = 0x8006F4E4u;
constexpr uint32_t kFinishReadFileReturnPc = 0x8006F4FCu;
constexpr uint32_t kFinishCloseFileReturnPc = 0x8006F504u;

constexpr uint32_t kGuestStackBytes = 72u;

} // namespace

void Spider1AlternateMode::begin() {
  flag_ = true;
  needsInit_ = true;
}

Spider1AlternateMode::Outcome Spider1AlternateMode::step(Core &core) {
  const uint32_t object = core.mem_r32(alternateObjectPointer);
  if (needsInit_) {
    core.mem_w8(object + kUnattributedAlternateObjectByte0E, flag_ ? 1 : 0);
    core.r[4] = object;
    core.r[5] = 0;
    enterGuestCall(core, alternateBegin, kBeginReturnPc);
    needsInit_ = false;
  }

  const GuestStackFrame stack(core, kGuestStackBytes);
  enterGuestCall(core, primaryPreFrame, kStepPreFrameReturnPc);
  enterGuestCall(core, frameBegin, kStepFrameBeginReturnPc);
  enterGuestCall(core, poolRotate, kStepPoolRotateReturnPc);
  const uint32_t vblankAtFrameStart = core.mem_r32(gameVblankCount);

  core.r[4] = alternateEffectX;
  core.r[5] = alternateEffectY;
  enterGuestCall(core, uiEffect, kStepEffectReturnPc);
  core.r[4] = uiDepth;
  enterGuestCall(core, uiSetDepth, kStepSetDepthReturnPc);
  core.r[4] = object;
  // The per-field object update is the menu's update call.
  enterGuestCall(core, uiUpdate, kStepUpdateReturnPc);
  core.r[4] = alternateTextX;
  core.r[5] = alternateTextY;
  core.r[6] = alternateTextLength;
  core.r[7] = 0;
  enterGuestCall(core, uiText, kStepTextReturnPc);
  core.r[4] = alternateQuadX;
  core.r[5] = alternateQuadY;
  core.r[6] = core.mem_r32(uiTable + uiTableQuadAssetEntry);
  core.r[7] = 0;
  core.mem_w32(stack.at(uiQuadSizeWordSlot), uiQuadSizeWord);
  enterGuestCall(core, uiQuad, kStepQuadReturnPc);
  core.r[4] = alternateOverlayX;
  core.r[5] = alternateOverlayY;
  enterGuestCall(core, alternateEffect, kStepOverlayReturnPc);
  enterGuestCall(core, padRead, kStepPadReadReturnPc);

  // A zero first pad byte ends the mode's own flag and clears the ack byte; the exit bytes are read
  // after.
  if (core.mem_r8(padState + padStateByte30) == 0) {
    flag_ = false;
    core.mem_w8(padState + padStateByte31, 0);
  }
  core.r[4] = object;
  enterGuestCall(core, uiFinishFrame, kStepFinishFrameReturnPc);

  // A press ends the mode only while its flag is set, and is consumed (bytes cleared, sound played)
  // first.
  const bool padPressed =
      core.mem_r8(padState + padStateByteE1) != 0 || core.mem_r8(padState + padStateByte31) != 0;
  const Spider1AlternateExit exit = modeAlternateExit(flag_, padPressed);
  if (exit.byPad) {
    core.r[4] = alternateExitSoundId;
    core.r[5] = exitSoundVolume;
    core.r[6] = 0;
    core.mem_w8(padState + padStateByte31, 0);
    core.mem_w8(padState + padStateByteE1, 0);
    enterGuestCall(core, uiSound, kStepSoundReturnPc);
  }

  if (exit.exits) {
    const bool repeats = finish(core);
    stack.restore();
    return repeats ? Outcome::Continue : Outcome::RestartPrimary;
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

bool Spider1AlternateMode::finish(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, oneField);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFinishDrawSyncReturnPc);
  enterGuestCall(core, submitFrame, kFinishSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFinishTrailingDrawSyncReturnPc);
  enterGuestCall(core, outerReset, kFinishOuterResetReturnPc);
  core.r[4] = 0;
  enterGuestCall(core, drawSync, kFinishPostResetDrawSyncReturnPc);
  core.r[4] = padState;
  enterGuestCall(core, padReset, kFinishPadResetReturnPc);

  const uint32_t object = core.mem_r32(alternateObjectPointer);
  core.r[4] = object;
  enterGuestCall(core, alternateRelease, kFinishReleaseReturnPc);

  const bool objectAskedToRepeat = core.mem_r8(object + kUnattributedAlternateObjectByte0E) == 1;
  // Another pass re-reads the file into the caller's stack window, and runs only if the read
  // produced something.
  uint32_t reloadedBytes = 0;
  if (objectAskedToRepeat) {
    core.r[4] = alternateFile;
    core.r[5] = modeObjectReadAlternateFile;
    enterGuestCall(core, modeObjectOpen, kFinishOpenFileReturnPc);
    core.mem_w32(core.r[29] + alternateObjectReadSlot, modeObjectReadAlternateFile);
    core.r[4] = alternateFile;
    core.r[5] = modeObjectReadAlternateFile;
    core.r[6] = core.r[29] + alternateObjectReadSlot;
    core.r[7] = core.r[29] + alternateObjectReadResultSlot;
    enterGuestCall(core, modeObjectRead, kFinishReadFileReturnPc);
    core.r[4] = alternateFile;
    enterGuestCall(core, modeObjectClose, kFinishCloseFileReturnPc);
    reloadedBytes = core.mem_r32(core.r[29] + alternateObjectReadResultSlot);
  }
  if (!modeAlternateRepeats(objectAskedToRepeat, reloadedBytes)) {
    return false;
  }
  flag_ = true;
  needsInit_ = true;
  return true;
}

} // namespace spider::spider1
