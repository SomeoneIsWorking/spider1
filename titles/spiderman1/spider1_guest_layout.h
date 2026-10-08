// Named SLUS_008.75 guest addresses and offsets; kUnattributed names have unknown meaning.
#pragma once

#include <cstdint>

namespace spider::spider1 {

// Outer mode

// Outer mode's asynchronous-load state; non-zero makes the display repeat the previous image.
inline constexpr uint32_t asyncModeState = 0x800B5464u;
// Title field counter, advanced by the display-field owner.
inline constexpr uint32_t gameVblankCount = 0x800B5468u;
// Mode number the outer cycle re-enters with.
inline constexpr uint32_t outerCaseValue = 0x800B486Cu;
// Cleared on scratch mode entry; kept for the two selectors in `modePreservesOuterClear`.
inline constexpr uint32_t outerClear = 0x800B53D4u;
// Outer cycle index and table; a record's last word is the next mode object pointer.
inline constexpr uint32_t outerCycleIndex = 0x800B4F30u;
inline constexpr uint32_t outerCycleTable = 0x80098B14u;
inline constexpr uint32_t outerCycleRecordStride = 184u;
inline constexpr uint32_t outerCycleRecordObjectOffset = 180u;
// File object the outer cycle opens, reads and closes; read takes handle in r6, block in r7.
inline constexpr uint32_t modeObject = 0x800B4FD0u;
// Mode argument block and the level name buffer beside it.
inline constexpr uint32_t modeArgument = 0x800A5688u;
inline constexpr uint32_t levelName = 0x800A568Cu;
// Outer argument block in the guest stack frame; the guest's use of the two flags is unknown.
inline constexpr uint32_t outerArgumentFirstSlot = 16u;
inline constexpr uint32_t outerArgumentSecondSlot = 20u;
inline constexpr uint32_t outerArgumentReadResultSlot = 24u;
// Level lookup keys; the second key's hit picks which visit counter is bumped.
inline constexpr uint32_t levelLookupKey = 0x800B4FD8u;
inline constexpr uint32_t levelAltKey = 0x800B4FE0u;
// Set when the level route commits a lookup; selects alternate over primary mode.
inline constexpr uint32_t levelAlternateFlag = 0x800B4F40u;
// Level object flag word; the alternate bit being set selects the alternate mode.
inline constexpr uint32_t levelObjectFlagWordOffset = 8u;
inline constexpr uint32_t levelObjectAlternateBit = 2u;
// Lookup index of `max` means the name is not in the table.
inline constexpr uint32_t levelLookupMissing = UINT32_MAX;
// 0 just before a frame's present, 1 once the field service has run for it.
inline constexpr uint32_t frameHandshake = 0x800B5474u;

// Primary mode

// Primary state: non-zero while the loop runs, and the outer jump table selector once it stops.
inline constexpr uint32_t primaryModeState = 0x800B4F34u;
// Cleared by the primary teardown before its body.
inline constexpr uint32_t primaryTeardownFlag = 0x800B54A0u;
// Cleared by the primary exit before the outer route is chosen.
inline constexpr uint32_t primaryPostClear = 0x800B56F8u;
// Non-zero runs the optional init pass with `primaryOptionalInitArgument`.
inline constexpr uint32_t primaryOptionalInitFlag = 0x800B5778u;
inline constexpr uint32_t primaryOptionalInitArgument = 0x12A6CC58u;
// Halfword the teardown sets when finished, and the exit object the frame body tests.
inline constexpr uint32_t primaryReady = 0x800B58D0u;
inline constexpr uint32_t primaryExitObject = 0x800B5268u;

// Selectors the port writes; request-exit is also the jump table's menu route.
inline constexpr uint32_t primarySelectorRequestExit = 2u;
// Selector that makes the primary teardown take its special exit.
inline constexpr uint32_t primarySelectorSpecialExit = 3u;

// Words `initializePrimary` clears once each around the primary reset; role unknown.
inline constexpr uint32_t kUnattributedWord4F38 = 0x800B4F38u;
inline constexpr uint32_t kUnattributedWord5690 = 0x800B5690u;
inline constexpr uint32_t kUnattributedWord5694 = 0x800B5694u;
inline constexpr uint32_t kUnattributedWord4FEC = 0x800B4FECu;
inline constexpr uint32_t kUnattributedWord5004 = 0x800B5004u;

// Mode argument block and level visit counters

// Byte 13 of the mode argument block, cleared by ResetThenPrimary before preparing the primary
// mode.
inline constexpr uint32_t modeArgumentResultByte = 13u;
// Per-key visit counters past `levelName`; they saturate at 255, and the bumping route sets
// `levelAlternateFlag`.
inline constexpr uint32_t levelVisitCounterFirstKey = 81u;
inline constexpr uint32_t levelVisitCounterSecondKey = 82u;

// Display

// Current draw buffer and the first display buffer record; `displayBuffer0` is the upper half.
inline constexpr uint32_t currentDrawBuffer = 0x800B54A8u;
inline constexpr uint32_t displayBuffer0 = 0x8009A6E4u;
// VSync(0) clock: pointer to the horizontal counter, its rebased baseline, last VSync field, and
// the GPU status word the VSync tail reads.
inline constexpr uint32_t horizontalCounterPointer = 0x800B0FA4u;
inline constexpr uint32_t horizontalCounterBaseline = 0x800B0FA8u;
inline constexpr uint32_t lastVsyncField = 0x800B0FACu;
inline constexpr uint32_t gpuStatusWord = 0x800B0FA0u;
// Two 256-line halves scanned as one 512x240 image.
inline constexpr uint32_t displayWidth = 512u;
inline constexpr uint32_t displayHeight = 240u;
inline constexpr uint32_t displayBufferHeight = 256u;
// Draw/display swap bytes in the 0x8009A6E4 record: byte 18 cleared before a mode's first field,
// set after its last; byte 90 the opposite.
inline constexpr uint32_t displayBufferTableByte18 = 24u;
inline constexpr uint32_t displayBufferTableByte90 = 144u;

// Word read from the draw buffer and passed to the OT relink as its length.
inline constexpr uint32_t drawBufferOtLengthOffset = 112u;
// Argument the OT relink is always given.
inline constexpr uint32_t renderArgument = 0x1000u;

// Guest stack argument blocks, addressed by slot offset within a `GuestStackFrame` window.

// Wipe source/destination rectangle: x, y, width, height halfwords.
inline constexpr uint32_t transitionRectSlot = 16u;
inline constexpr uint32_t transitionRectX = 0u;
inline constexpr uint32_t transitionRectY = 2u;
inline constexpr uint32_t transitionRectWidth = 4u;
inline constexpr uint32_t transitionRectHeight = 6u;
// Wipe per-row transfer descriptor; row count in the last halfword.
inline constexpr uint32_t transitionRowSlot = 40u;
inline constexpr uint32_t transitionRowRect = 0u;
inline constexpr uint32_t transitionRowY = 2u;
inline constexpr uint32_t transitionRowWidth = 4u;
inline constexpr uint32_t transitionRowCount = 6u;
// Wipe scratch for one four-row group.
inline constexpr uint32_t transitionScratchBytes = 4096u;
inline constexpr uint32_t transitionRowGroupBytes = 1024u;
inline constexpr uint32_t transitionRowGroupRows = 4u;
inline constexpr uint32_t transitionRowWords = 256u;

// Menu construction block: width, height, and a third unread word.
inline constexpr uint32_t menuConstructSlot = 16u;
inline constexpr uint32_t menuConstructWidthSlot = 0u;
inline constexpr uint32_t menuConstructHeightSlot = 4u;
inline constexpr uint32_t kUnattributedMenuConstructWord3 = 8u;
inline constexpr uint32_t kUnattributedMenuConstructWord3Value = 16u;
// Menu background copy rectangle and the four button words the input read fills.
inline constexpr uint32_t menuRectSlot = 32u;
inline constexpr uint32_t menuRectX = 0u;
inline constexpr uint32_t menuRectY = 2u;
inline constexpr uint32_t menuRectWidth = 4u;
inline constexpr uint32_t menuRectHeight = 6u;
inline constexpr uint32_t menuInputSlot = 40u;
inline constexpr uint32_t menuInputButtonsSlot = 0u;
inline constexpr uint32_t menuInputButtonsHeldSlot = 4u;
inline constexpr uint32_t menuInputButtonsNewSlot = 8u;
inline constexpr uint32_t menuInputButtonsReleasedSlot = 12u;
// Size word given to both the menu's and the alternate mode's quad call.
inline constexpr uint32_t uiQuadSizeWordSlot = 16u;
inline constexpr uint32_t uiQuadSizeWord = 4096u;
// Alternate mode object-read block: mode word written, size reported.
inline constexpr uint32_t alternateObjectReadSlot = 24u;
inline constexpr uint32_t alternateObjectReadResultSlot = 28u;

// Objects owned by the modes

// Menu object bytes written once at build: 0x18 set to 1, 0x0B to 0; guest names unknown.
inline constexpr uint32_t kUnattributedMenuObjectByte0B = 11u;
inline constexpr uint32_t kUnattributedMenuObjectByte18 = 24u;
// Menu vtable slots: +8 is a signed 16-bit offset to the exit call's argument, +12 the function
// entered.
inline constexpr uint32_t menuVtableTargetOffsetSlot = 8u;
inline constexpr uint32_t menuVtableEntrySlot = 12u;
inline constexpr uint32_t menuVtableCallArgument = 3u;
// Alternate mode object pointer and the file it re-opens when the repeat byte asks.
inline constexpr uint32_t alternateObjectPointer = 0x800B49A0u;
inline constexpr uint32_t alternateFile = 0x800B57E8u;
// Alternate object repeat byte: set from the mode flag before begin, tested for 1 after release.
inline constexpr uint32_t kUnattributedAlternateObjectByte0E = 14u;

// Pad state block

// Pad buffer; the 0x30/0xE0 bytes request a mode exit when non-zero and the 0x31/0xE1 bytes are
// cleared by the same branch.
inline constexpr uint32_t padState = 0x800A4DF4u;
inline constexpr uint32_t padStateByte30 = 48u;
inline constexpr uint32_t padStateByte31 = 49u;
inline constexpr uint32_t padStateByteE0 = 224u;
inline constexpr uint32_t padStateByteE1 = 225u;

// UI script table

// Table entries passed as arguments to UI calls; named for the consuming call.
inline constexpr uint32_t uiTable = 0x80097760u;
inline constexpr uint32_t uiTableQuadAssetRoot = 0u;
inline constexpr uint32_t uiTableQuadAssetEntry = 416u;
inline constexpr uint32_t uiTableBindEntryFirst = 808u;
inline constexpr uint32_t uiTableBindEntrySecond = 820u;

// Asynchronous mode state values

// Published to `asyncModeState` as each mode finishes.
inline constexpr uint32_t asyncModeStateAfterWipe = 0x001E005Au;
inline constexpr uint32_t asyncModeStateAwaitingMenu = 10u;
inline constexpr uint32_t asyncModeStateAfterMenu = 80u;
// Outer cycle `case` values written before re-entering a transition.
inline constexpr uint32_t outerRestartCaseValue = 5u;
inline constexpr uint32_t outerTransitionCaseValue = 20u;

// Invalid-selector wait

// Setup argument and the field timeout before the release is declared missing.
inline constexpr uint32_t invalidWaitArgument = 0x80093C28u;
inline constexpr uint32_t invalidInputWaitFields = 240u;
inline constexpr uint32_t invalidInputTimeoutFields = 1800u;

// Guest service entry points; return addresses are the `jal` return addresses the guest recorded.

inline constexpr uint32_t beginOuterMode = 0x8006BE28u;
inline constexpr uint32_t modeObjectOpen = 0x8001B990u;
inline constexpr uint32_t modeObjectRead = 0x8001BEC4u;
inline constexpr uint32_t modeObjectClose = 0x8001BDCCu;
inline constexpr uint32_t modePrepare = 0x8001895Cu;
inline constexpr uint32_t outerReset = 0x80017920u;
inline constexpr uint32_t resourceBegin = 0x8005DAE0u;
inline constexpr uint32_t resourceFind = 0x80058CE4u;
inline constexpr uint32_t resourceTransform = 0x80047DF8u;
inline constexpr uint32_t resourceFinish = 0x8005AC00u;
inline constexpr uint32_t resourceBind = 0x8006F948u;
inline constexpr uint32_t levelLookup = 0x8005F1D4u;
inline constexpr uint32_t levelIndex = 0x80018898u;
inline constexpr uint32_t levelCommit = 0x8006F0D4u;
inline constexpr uint32_t levelObject = 0x80018800u;
inline constexpr uint32_t invalidWaitSetup = 0x80014D54u;

inline constexpr uint32_t primaryPrepare = 0x80047478u;
inline constexpr uint32_t primaryReset = 0x8006AF28u;
inline constexpr uint32_t primaryOptionalInit = 0x8005E77Cu;
inline constexpr uint32_t primaryTableInit = 0x80060C3Cu;
inline constexpr uint32_t primaryPreFrame = 0x8002BBCCu;
inline constexpr uint32_t frameBegin = 0x800612B8u;
inline constexpr uint32_t poolRotate = 0x8002BB9Cu;
inline constexpr uint32_t logic = 0x8002BBD4u;
inline constexpr uint32_t audioState = 0x80062CE0u;
inline constexpr uint32_t renderWalk = 0x8002BD5Cu;
inline constexpr uint32_t renderTail = 0x8002B184u;
inline constexpr uint32_t otRelink = 0x8007F930u;
inline constexpr uint32_t drawSync = 0x800819A4u;
inline constexpr uint32_t fieldService = 0x8005E234u;
inline constexpr uint32_t submitFrame = 0x80061308u;
inline constexpr uint32_t exitTest = 0x80059664u;
inline constexpr uint32_t primaryTeardown = 0x800610C8u;
inline constexpr uint32_t primaryAudioTeardown = 0x80069D3Cu;
inline constexpr uint32_t primarySpecialExit = 0x80018900u;
inline constexpr uint32_t primaryFinalize = 0x80063D80u;
inline constexpr uint32_t primaryRelease = 0x8002B88Cu;

inline constexpr uint32_t transitionCopyImage = 0x80081D10u;
inline constexpr uint32_t transitionStoreRow = 0x80081CB0u;
inline constexpr uint32_t transitionLoadRow = 0x80081C50u;
inline constexpr uint32_t guestAllocate = 0x800651C8u;
inline constexpr uint32_t guestRelease = 0x800654E8u;

inline constexpr uint32_t padRead = 0x8006B514u;
inline constexpr uint32_t padReset = 0x8006AFECu;
inline constexpr uint32_t uiSetDepth = 0x80019524u;
inline constexpr uint32_t uiAllocate = 0x8002BAB4u;
inline constexpr uint32_t uiConstruct = 0x80016424u;
inline constexpr uint32_t uiBind = 0x80016B30u;
inline constexpr uint32_t uiUpdate = 0x80016CA4u;
inline constexpr uint32_t uiText = 0x8001957Cu;
inline constexpr uint32_t uiQuad = 0x80019A90u;
inline constexpr uint32_t uiEffect = 0x8006D61Cu;
inline constexpr uint32_t uiReadInput = 0x80017E14u;
inline constexpr uint32_t uiFinishFrame = 0x800174CCu;
inline constexpr uint32_t uiSound = 0x80063770u;
inline constexpr uint32_t uiExitTest = 0x80016C64u;

// Alternate mode entries; its object update is `uiUpdate`, shared with the menu.
inline constexpr uint32_t alternateBegin = 0x800166A0u;
inline constexpr uint32_t alternateEffect = 0x8006D320u;
inline constexpr uint32_t alternateRelease = 0x800165FCu;

// Literal arguments the modes pass

// Primary warm-up table init: element count and byte stride.
inline constexpr uint32_t primaryTableInitCount = 32u;
inline constexpr uint32_t primaryTableInitStride = 8u;
// Resource ids searched by the ResourcePrimary route, in search order.
inline constexpr uint32_t resourceFindFirstId = 4128u;
inline constexpr uint32_t resourceFindSecondId = 756u;
// UI depth set by the menu and alternate modes.
inline constexpr uint32_t uiDepth = 256u;
// Menu allocation bytes and construction width and height.
inline constexpr uint32_t menuAllocateBytes = 1160u;
inline constexpr uint32_t menuConstructWidth = 256u;
inline constexpr uint32_t menuConstructHeight = 112u;
// Menu per-frame script geometry.
inline constexpr uint32_t menuTextX = 127u;
inline constexpr uint32_t menuTextY = 25u;
inline constexpr uint32_t menuTextLength = 33u;
inline constexpr uint32_t menuQuadX = 256u;
inline constexpr uint32_t menuQuadY = 64u;
inline constexpr uint32_t menuEffectX = 460u;
inline constexpr uint32_t menuEffectY = 173u;
inline constexpr uint32_t menuIntroEffectFrames = 2u;
// Exit sound id and volume.
inline constexpr uint32_t menuExitSoundId = 21u;
inline constexpr uint32_t alternateExitSoundId = 31u;
inline constexpr uint32_t exitSoundVolume = 8192u;
// Alternate mode per-frame script geometry.
inline constexpr uint32_t alternateEffectX = 800u;
inline constexpr uint32_t alternateEffectY = 173u;
inline constexpr uint32_t alternateOverlayX = 800u;
inline constexpr uint32_t alternateOverlayY = 460u;
inline constexpr uint32_t alternateQuadX = 256u;
inline constexpr uint32_t alternateQuadY = 60u;
inline constexpr uint32_t alternateTextX = 77u;
inline constexpr uint32_t alternateTextY = 83u;
inline constexpr uint32_t alternateTextLength = 105u;
// Mode-object open and read modes; field wait count.
inline constexpr uint32_t modeObjectOpenMode = 1u;
inline constexpr uint32_t modeObjectReadFrame = 0u;
inline constexpr uint32_t modeObjectReadAlternateFile = 1u;
inline constexpr uint32_t oneField = 1u;
// Outer-mode number ResourcePrimary enters its loader with; other routes pass 0.
inline constexpr uint32_t resourceBeginModeNumber = 2u;
// 3D wipe channel arithmetic: new value is a share of the sum of the three old channels, darkening
// toward red.
inline constexpr uint32_t transitionChannelMask = 31u;
inline constexpr uint32_t transitionDimNumerator = 341u;
inline constexpr uint32_t transitionDimShift = 11u;
inline constexpr uint32_t transitionStrongNumerator = 1365u;
inline constexpr uint32_t transitionStrongShift = 12u;
inline constexpr uint32_t transitionSignBit = 0x8000u;

// Pre-main boot

// Field-callback registration entry and the guest field wait.
inline constexpr uint32_t vsyncCallbackRegistration = 0x8008B8CCu;
inline constexpr uint32_t guestFieldWait = 0x8005E748u;
// Stock CD sync body, served by the host's complete/ready owner.
inline constexpr uint32_t innerCdSync = 0x8008C944u;
// Stock GPU DMA timeout arm; answered from the title field counter instead of guest libetc.
inline constexpr uint32_t gpuDmaTimeoutStart = 0x80083C60u;
inline constexpr uint32_t gpuDmaTimeoutDeadline = 0x800B0F64u;
inline constexpr uint32_t gpuDmaTimeoutPollCount = 0x800B0F68u;
inline constexpr uint32_t gpuDmaTimeoutFieldBudget = 240u;
// Instructions the retail timeout body would have run, for the executor budget.
inline constexpr uint32_t gpuDmaTimeoutRetailInstructions = 13u;
// C entry, game init, and the return address inside game init of the post-logo wait's pad service.
inline constexpr uint32_t libcEntry = 0x80087444u;
inline constexpr uint32_t gameInit = 0x8006BF9Cu;
inline constexpr uint32_t bootTailPadReturn = 0x8006C304u;
// Retail movie player entry.
inline constexpr uint32_t moviePlayer = 0x8002AA0Cu;
// NTSC field rate in milliHz.
inline constexpr unsigned ntscFieldRateMilliHz = 59940u;

// Boot prefix stack frame; callee-saved registers spill into its top, highest first.
inline constexpr uint32_t bootPrefixFrameBytes = 72u;
// Return address crt0's `jal main` at 0x80087438 leaves in $ra.
inline constexpr uint32_t crt0MainReturn = 0x80087440u;

// crt0 values restored by the prefix: $gp, $s8 stack base, and $s7 (purpose unknown).
inline constexpr uint32_t crt0GlobalPointer = 0x800B0000u;
inline constexpr uint32_t crt0StackBase = 0x800A0000u;
inline constexpr uint32_t kUnattributedCrt0Register23 = 0x80090000u;

} // namespace spider::spider1
