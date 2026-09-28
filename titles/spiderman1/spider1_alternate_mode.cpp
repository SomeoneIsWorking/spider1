#include "spider1_alternate_mode.h"

#include "spider1_guest_call.h"
#include "spider1_guest_layout.h"
#include "spider1_guest_stack_frame.h"
#include "spider1_mode_decisions.h"
#include "spider1_mode_frame_boundary.h"

#include "core.h"

namespace spider {
namespace {

// The authenticated `jal` return addresses for this mode's own call sites, in call order.
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
  const uint32_t object = core.mem_r32(spider1::alternateObjectPointer);
  if (needsInit_) {
    core.mem_w8(object + spider1::kUnattributedAlternateObjectByte0E, flag_ ? 1 : 0);
    core.r[4] = object;
    core.r[5] = 0;
    spider1CallGuest(core, spider1::alternateBegin, kBeginReturnPc);
    needsInit_ = false;
  }

  const GuestStackFrame stack(core, kGuestStackBytes);
  spider1CallGuest(core, spider1::primaryPreFrame, kStepPreFrameReturnPc);
  spider1CallGuest(core, spider1::frameBegin, kStepFrameBeginReturnPc);
  spider1CallGuest(core, spider1::poolRotate, kStepPoolRotateReturnPc);
  const uint32_t vblankAtFrameStart = core.mem_r32(spider1::gameVblankCount);

  core.r[4] = spider1::alternateEffectX;
  core.r[5] = spider1::alternateEffectY;
  spider1CallGuest(core, spider1::uiEffect, kStepEffectReturnPc);
  core.r[4] = spider1::uiDepth;
  spider1CallGuest(core, spider1::uiSetDepth, kStepSetDepthReturnPc);
  core.r[4] = object;
  // The alternate mode's per-field object update is the MENU's update call: one guest function, two
  // modes, and the port used to give it two names.
  spider1CallGuest(core, spider1::uiUpdate, kStepUpdateReturnPc);
  core.r[4] = spider1::alternateTextX;
  core.r[5] = spider1::alternateTextY;
  core.r[6] = spider1::alternateTextLength;
  core.r[7] = 0;
  spider1CallGuest(core, spider1::uiText, kStepTextReturnPc);
  core.r[4] = spider1::alternateQuadX;
  core.r[5] = spider1::alternateQuadY;
  core.r[6] = core.mem_r32(spider1::uiTable + spider1::uiTableQuadAssetEntry);
  core.r[7] = 0;
  core.mem_w32(stack.at(spider1::uiQuadSizeWordSlot), spider1::uiQuadSizeWord);
  spider1CallGuest(core, spider1::uiQuad, kStepQuadReturnPc);
  core.r[4] = spider1::alternateOverlayX;
  core.r[5] = spider1::alternateOverlayY;
  spider1CallGuest(core, spider1::alternateEffect, kStepOverlayReturnPc);
  spider1CallGuest(core, spider1::padRead, kStepPadReadReturnPc);

  // The first pad byte decides the MODE'S OWN FLAG, not an exit: reading it as zero means the
  // attract screen has run out of input, and the acknowledgement byte beside it is cleared with it.
  // That write happens BEFORE the exit bytes are read below, which is why the read cannot be
  // hoisted above this block.
  if (core.mem_r8(spider1::padState + spider1::padStateByte30) == 0) {
    flag_ = false;
    core.mem_w8(spider1::padState + spider1::padStateByte31, 0);
  }
  core.r[4] = object;
  spider1CallGuest(core, spider1::uiFinishFrame, kStepFinishFrameReturnPc);

  // A press ends the attract screen only while its own flag is still set, and the press is CONSUMED
  // before the mode acts on it: both pad bytes are cleared and the sound is played first, so the
  // press that ends this pass cannot also end the pass that follows it.
  const bool padPressed = core.mem_r8(spider1::padState + spider1::padStateByteE1) != 0 ||
                          core.mem_r8(spider1::padState + spider1::padStateByte31) != 0;
  const Spider1AlternateExit exit = spider1ModeAlternateExit(flag_, padPressed);
  if (exit.byPad) {
    core.r[4] = spider1::alternateExitSoundId;
    core.r[5] = spider1::exitSoundVolume;
    core.r[6] = 0;
    core.mem_w8(spider1::padState + spider1::padStateByte31, 0);
    core.mem_w8(spider1::padState + spider1::padStateByteE1, 0);
    spider1CallGuest(core, spider1::uiSound, kStepSoundReturnPc);
  }

  if (exit.exits) {
    const bool repeats = finish(core);
    stack.restore();
    return repeats ? Outcome::Continue : Outcome::RestartPrimary;
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

bool Spider1AlternateMode::finish(Core &core) {
  core.r[4] = 1;
  host_.waitFields(core, spider1::oneField);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFinishDrawSyncReturnPc);
  spider1CallGuest(core, spider1::submitFrame, kFinishSubmitReturnPc);
  host_.commitSubmittedFrame(core);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFinishTrailingDrawSyncReturnPc);
  spider1CallGuest(core, spider1::outerReset, kFinishOuterResetReturnPc);
  core.r[4] = 0;
  spider1CallGuest(core, spider1::drawSync, kFinishPostResetDrawSyncReturnPc);
  core.r[4] = spider1::padState;
  spider1CallGuest(core, spider1::padReset, kFinishPadResetReturnPc);

  const uint32_t object = core.mem_r32(spider1::alternateObjectPointer);
  core.r[4] = object;
  spider1CallGuest(core, spider1::alternateRelease, kFinishReleaseReturnPc);

  const bool objectAskedToRepeat =
      core.mem_r8(object + spider1::kUnattributedAlternateObjectByte0E) == 1;
  // The object asked for another pass, so its file is re-read — and the pass only happens if the
  // read actually produced something. Reading it into the caller's own stack window is what the
  // retail code does, and the result word is this window's, not a host allocation's.
  uint32_t reloadedBytes = 0;
  if (objectAskedToRepeat) {
    core.r[4] = spider1::alternateFile;
    core.r[5] = spider1::modeObjectReadAlternateFile;
    spider1CallGuest(core, spider1::modeObjectOpen, kFinishOpenFileReturnPc);
    core.mem_w32(core.r[29] + spider1::alternateObjectReadSlot,
                 spider1::modeObjectReadAlternateFile);
    core.r[4] = spider1::alternateFile;
    core.r[5] = spider1::modeObjectReadAlternateFile;
    core.r[6] = core.r[29] + spider1::alternateObjectReadSlot;
    core.r[7] = core.r[29] + spider1::alternateObjectReadResultSlot;
    spider1CallGuest(core, spider1::modeObjectRead, kFinishReadFileReturnPc);
    core.r[4] = spider1::alternateFile;
    spider1CallGuest(core, spider1::modeObjectClose, kFinishCloseFileReturnPc);
    reloadedBytes = core.mem_r32(core.r[29] + spider1::alternateObjectReadResultSlot);
  }
  if (!spider1ModeAlternateRepeats(objectAskedToRepeat, reloadedBytes)) {
    return false;
  }
  flag_ = true;
  needsInit_ = true;
  return true;
}

} // namespace spider
