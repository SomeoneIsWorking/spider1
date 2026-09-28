// spider1_guest_layout.h — the ONE owner of every named SLUS_008.75 guest fact the title's mode
// and frame drivers touch.
//
// WHY THIS IS A HEADER AND NOT A BLOCK OF CONSTANTS AT THE TOP OF EACH DRIVER. The two drivers read
// well over ninety distinct guest words, table entries, and record offsets. Two lists of them
// drift, and a list of bare addresses is a list of bare addresses: a reader cannot tell which word
// is the mode selector, which is a frame handshake, and which is a number nobody has looked at. So
// each fact is declared ONCE, here, at a name that says what it is, and both drivers include this
// header instead of redeclaring anything.
//
// THE NAMING CONTRACT, which is the whole point of the file:
//
//   * A name asserts only what the executable or the driving code already shows. Where a word's
//     role is evident from the value's use, the name states that role ("the field the guest clears
//     before its primary reset").
//   * Where the role is genuinely NOT known, the name says so — `kUnattributed…` — and the comment
//     says exactly what IS known (where it is read or written, what value, what it is passed to)
//     and that the guest's own name for it has not been read out of the image. An honest "we do not
//     know what this is" name is what makes a black box readable, and it is the first step to
//     finding out. A confident invented name is worse than a hex literal, because it will be
//     trusted.
//
// NO ADDRESS IN THIS FILE APPLIES TO ENTER ELECTRO. These are SLUS_008.75 facts.
#pragma once

#include <cstdint>

namespace spider::spider1 {

// ---------------------------------------------------------------------------------------------
// Outer mode: the persistent selector and the arguments the outer function reads back
// ---------------------------------------------------------------------------------------------

// The word the outer mode function sets while its asynchronous load runs, and every mode waits on
// before it may proceed. Each mode clears or sets it on entry and exit; a non-zero value means the
// display keeps repeating the previous image instead of advancing.
inline constexpr uint32_t asyncModeState = 0x800B5464u;
// The title's own field counter, incremented by the display-field owner. Every "did a field pass
// during this frame?" question in the mode driver compares against a value sampled at frame start.
inline constexpr uint32_t gameVblankCount = 0x800B5468u;
// The outer function's `case` value: the mode number the outer cycle re-enters with.
inline constexpr uint32_t outerCaseValue = 0x800B486Cu;
// Cleared when a mode is entered from scratch and preserved for the two selectors that keep the
// outer screen; see `spider1ModePreservesOuterClear`.
inline constexpr uint32_t outerClear = 0x800B53D4u;
// The retail index into the outer cycle table, 0-based, and the table itself. One record is
// `outerCycleRecordStride` bytes and its last word is the mode object pointer the next cycle opens.
inline constexpr uint32_t outerCycleIndex = 0x800B4F30u;
inline constexpr uint32_t outerCycleTable = 0x80098B14u;
inline constexpr uint32_t outerCycleRecordStride = 184u;
inline constexpr uint32_t outerCycleRecordObjectOffset = 180u;
// The file object the outer cycle opens, reads arguments from, and closes. The read call takes the
// object's handle in `r6` and a caller-supplied block in `r7`.
inline constexpr uint32_t modeObject = 0x800B4FD0u;
// The mode argument block the prepare call clears or fills, and the level name buffer beside it.
inline constexpr uint32_t modeArgument = 0x800A5688u;
inline constexpr uint32_t levelName = 0x800A568Cu;
// The outer cycle's argument block, as built in the guest's own stack frame: the two whole-word
// flags the port hands to the outer function, and the word its object read reports into. The flags
// are booleans the port sets on exactly one route each; the guest's use for each of the two is not
// read out of the image.
inline constexpr uint32_t outerArgumentFirstSlot = 16u;
inline constexpr uint32_t outerArgumentSecondSlot = 20u;
inline constexpr uint32_t outerArgumentReadResultSlot = 24u;
// The two keys the level route looks a name up by. The first miss decides the alternate route; the
// second key's hit decides which of the two visit counters the route bumps.
inline constexpr uint32_t levelLookupKey = 0x800B4FD8u;
inline constexpr uint32_t levelAltKey = 0x800B4FE0u;
// Set by the level route when it commits a lookup, and read to choose between the alternate mode
// and the primary mode.
inline constexpr uint32_t levelAlternateFlag = 0x800B4F40u;
// The level object the level route resolves the name to, its flag word, and the bit in that word
// whose being SET sends the title to the alternate mode instead of the primary. The word's other
// bits are not read by this port, and its field name is not read out of the image.
inline constexpr uint32_t levelObjectFlagWordOffset = 8u;
inline constexpr uint32_t levelObjectAlternateBit = 2u;
// The lookup the level route makes when its first key missed: an index of `max` means the name is
// not in the table at all, which is not an error and is not treated as one.
inline constexpr uint32_t levelLookupMissing = UINT32_MAX;
// Set to 0 immediately before a frame's present and to 1 once the field service has run for it, so
// the frame body and the display field agree on which fence is outstanding.
inline constexpr uint32_t frameHandshake = 0x800B5474u;

// ---------------------------------------------------------------------------------------------
// Primary mode: the selector the primary loop writes to request its own exit
// ---------------------------------------------------------------------------------------------

// The primary mode's own state word. It is both "this loop is still running" (non-zero) and the
// selector the outer jump table is indexed by once it stops, so one word names the live state and
// the exit request.
inline constexpr uint32_t primaryModeState = 0x800B4F34u;
// Cleared by the primary teardown before its body runs.
inline constexpr uint32_t primaryTeardownFlag = 0x800B54A0u;
// Cleared by the primary exit before the outer route is chosen.
inline constexpr uint32_t primaryPostClear = 0x800B56F8u;
// The primary optional-init gate: non-zero means the primary reset runs the optional init pass with
// `primaryOptionalInitArgument`, and the argument it is given.
inline constexpr uint32_t primaryOptionalInitFlag = 0x800B5778u;
inline constexpr uint32_t primaryOptionalInitArgument = 0x12A6CC58u;
// The halfword the primary teardown publishes when it has finished, and the exit object the frame
// body tests to decide whether the primary loop is done.
inline constexpr uint32_t primaryReady = 0x800B58D0u;
inline constexpr uint32_t primaryExitObject = 0x800B5268u;

// PRIMARY SELECTOR VALUES. Two of them are written by the port rather than only read, so they are
// named: `primarySelectorRequestExit` is what the frame body writes when the exit object reports
// the loop is finished, and it is also the selector the jump table maps to the menu route.
inline constexpr uint32_t primarySelectorRequestExit = 2u;
// The selector whose value makes the primary teardown take its special exit before finalizing.
inline constexpr uint32_t primarySelectorSpecialExit = 3u;

// THE FIVE WORDS `initializePrimary` CLEARS AND NOTHING ELSE IN THIS PORT TOUCHES. What is known:
// the primary reset is called between the first write and the last, each is a whole word, and each
// is cleared to zero exactly once per primary initialization. What is NOT known: the guest's own
// name for any of them, and whether the retail body reads them, stores counters into them, or
// treats them as flags. They are named honestly rather than confidently.
inline constexpr uint32_t kUnattributedWord4F38 = 0x800B4F38u;
inline constexpr uint32_t kUnattributedWord5690 = 0x800B5690u;
inline constexpr uint32_t kUnattributedWord5694 = 0x800B5694u;
inline constexpr uint32_t kUnattributedWord4FEC = 0x800B4FECu;
inline constexpr uint32_t kUnattributedWord5004 = 0x800B5004u;

// ---------------------------------------------------------------------------------------------
// The mode argument block, and the level visit counters inside the level name buffer
// ---------------------------------------------------------------------------------------------

// Byte 13 of the mode argument block. The ResetThenPrimary route clears it before preparing the
// primary mode; the block's own field names are not read out of the image.
inline constexpr uint32_t modeArgumentResultByte = 13u;
// The two visit counters that follow the level name: one slot per lookup key, at `index` bytes past
// `levelName`. A counter saturates at 255 rather than wrapping, and the route that bumped it sets
// `levelAlternateFlag` and re-enters the outer cycle instead of loading a mode.
inline constexpr uint32_t levelVisitCounterFirstKey = 81u;
inline constexpr uint32_t levelVisitCounterSecondKey = 82u;

// ---------------------------------------------------------------------------------------------
// Display: the two buffers, the field-height split, and the unknown table they share
// ---------------------------------------------------------------------------------------------

// The draw buffer currently being filled, and the first display buffer record. A record's identity
// is what decides which half of the 512x240 frame a transition or menu wipe starts from: the
// buffer equal to `displayBuffer0` is the upper half.
inline constexpr uint32_t currentDrawBuffer = 0x800B54A8u;
inline constexpr uint32_t displayBuffer0 = 0x8009A6E4u;
// The display field clock VSync(0) is defined in terms of. The title holds a POINTER to its
// horizontal counter rather than the counter itself, a baseline the previous VSync rebased, and the
// field the most recent VSync completed. The GPU status word the retail VSync tail reads on its way
// out is here too, because the READ is part of that tail even though nothing here uses its value.
inline constexpr uint32_t horizontalCounterPointer = 0x800B0FA4u;
inline constexpr uint32_t horizontalCounterBaseline = 0x800B0FA8u;
inline constexpr uint32_t lastVsyncField = 0x800B0FACu;
inline constexpr uint32_t gpuStatusWord = 0x800B0FA0u;
// The frame this title renders into, and the vertical offset of the second buffer below it. The
// display is two 256-line halves scanned as one 512x240 image.
inline constexpr uint32_t displayWidth = 512u;
inline constexpr uint32_t displayHeight = 240u;
inline constexpr uint32_t displayBufferHeight = 256u;
// THE TWO BYTES WRITTEN WITH 0 AND 1 AROUND EVERY TRANSITION, MENU, AND ALTERNATE FRAME. What is
// known: `displayBufferTableByte18` is cleared before a mode's first field and set after its last,
// and `displayBufferTableByte90` is written with the opposite value at the same two points, and the
// pair is a draw/display swap. What is NOT known: the record's layout — 0x8009A6E4 is a static-data
// address, not a libgpu environment block, and no field name for either byte has been read out of
// the image.
inline constexpr uint32_t displayBufferTableByte18 = 24u;
inline constexpr uint32_t displayBufferTableByte90 = 144u;

// The one word the frame body reads out of the current draw buffer and passes to the ordering-table
// relink as its length. The draw buffer record's own field at `drawBufferOtLengthOffset` has not
// been named from the image; what is known is the role: it is the OT walk length argument.
inline constexpr uint32_t drawBufferOtLengthOffset = 112u;
// The argument the ordering-table relink is always given.
inline constexpr uint32_t renderArgument = 0x1000u;

// ---------------------------------------------------------------------------------------------
// Guest stack argument blocks
// ---------------------------------------------------------------------------------------------
//
// Each mode builds its outgoing argument block in a `GuestStackFrame` window and addresses it by
// the slot offset below. The slots are named per block; the block's own guest field names are not
// read out of the image.

// The wipe's source/destination rectangle: x, y, width, height as four halfwords.
inline constexpr uint32_t transitionRectSlot = 16u;
inline constexpr uint32_t transitionRectX = 0u;
inline constexpr uint32_t transitionRectY = 2u;
inline constexpr uint32_t transitionRectWidth = 4u;
inline constexpr uint32_t transitionRectHeight = 6u;
// The wipe's per-row transfer descriptor: the rectangle it re-reads each row, the row's y, its
// width, and the row count in the last halfword.
inline constexpr uint32_t transitionRowSlot = 40u;
inline constexpr uint32_t transitionRowRect = 0u;
inline constexpr uint32_t transitionRowY = 2u;
inline constexpr uint32_t transitionRowWidth = 4u;
inline constexpr uint32_t transitionRowCount = 6u;
// The scratch the wipe allocates to hold one four-row group.
inline constexpr uint32_t transitionScratchBytes = 4096u;
inline constexpr uint32_t transitionRowGroupBytes = 1024u;
inline constexpr uint32_t transitionRowGroupRows = 4u;
inline constexpr uint32_t transitionRowWords = 256u;

// The menu object's own construction block: width, height, and a third word the port does not
// otherwise read.
inline constexpr uint32_t menuConstructSlot = 16u;
inline constexpr uint32_t menuConstructWidthSlot = 0u;
inline constexpr uint32_t menuConstructHeightSlot = 4u;
inline constexpr uint32_t kUnattributedMenuConstructWord3 = 8u;
inline constexpr uint32_t kUnattributedMenuConstructWord3Value = 16u;
// The menu's background copy rectangle, and the four button words the input read fills.
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
// The word BOTH the menu's and the alternate mode's quad call is given as its size, in the first
// slot of their outgoing block. Two modes, one slot, one value: the port had it written twice under
// two different names.
inline constexpr uint32_t uiQuadSizeWordSlot = 16u;
inline constexpr uint32_t uiQuadSizeWord = 4096u;
// The alternate mode's object-read block: the mode word it writes, and the size the read reports.
inline constexpr uint32_t alternateObjectReadSlot = 24u;
inline constexpr uint32_t alternateObjectReadResultSlot = 28u;

// ---------------------------------------------------------------------------------------------
// Objects the modes own: the menu object, the alternate object, and the menu vtable
// ---------------------------------------------------------------------------------------------

// TWO MENU OBJECT BYTES, both written once when the menu is built. The guest's field names for them
// are not read out of the image; what is known is that the second is set to 1 and the first to 0 at
// the same point, which is the shape of a visibility pair.
inline constexpr uint32_t kUnattributedMenuObjectByte0B = 11u;
inline constexpr uint32_t kUnattributedMenuObjectByte18 = 24u;
// The two vtable slots the menu's final call uses: slot `+8` is a SIGNED 16-bit offset from the
// object to the address the exit call receives, and slot `+12` is the function pointer that call
// enters. Both are read out of the object's first word, so they are vtable slots, not object
// fields.
inline constexpr uint32_t menuVtableTargetOffsetSlot = 8u;
inline constexpr uint32_t menuVtableEntrySlot = 12u;
inline constexpr uint32_t menuVtableCallArgument = 3u;
// The object pointer the alternate mode drives, and the file it re-opens when its repeat byte says
// the object asked for another pass.
inline constexpr uint32_t alternateObjectPointer = 0x800B49A0u;
inline constexpr uint32_t alternateFile = 0x800B57E8u;
// THE ALTERNATE OBJECT'S REPEAT BYTE. What is known: it is set from the mode's own flag before the
// object is begun, tested for 1 after the mode releases it, and a re-read of the file with a
// non-zero size repeats the mode. What is NOT known: the guest's name for the byte.
inline constexpr uint32_t kUnattributedAlternateObjectByte0E = 14u;

// ---------------------------------------------------------------------------------------------
// The pad state block
// ---------------------------------------------------------------------------------------------

// The pad buffer the pad read and pad reset calls operate on. `padStateByte30` and `padStateByteE0`
// are tested for NON-ZERO to request a mode's exit, and `padStateByte31` and `padStateByteE1` are
// cleared by the very same branch — so each pair is a latched press and its acknowledgement. The
// guest's own field names for the four bytes are NOT read out of the image, and this port does not
// claim to know which buttons they are.
inline constexpr uint32_t padState = 0x800A4DF4u;
inline constexpr uint32_t padStateByte30 = 48u;
inline constexpr uint32_t padStateByte31 = 49u;
inline constexpr uint32_t padStateByteE0 = 224u;
inline constexpr uint32_t padStateByteE1 = 225u;
// The menu exit test reads the first of the four input words and a non-zero result ends the menu.

// ---------------------------------------------------------------------------------------------
// The UI script table
// ---------------------------------------------------------------------------------------------

// The table the UI calls index into. Four ENTRIES are read; each is a whole word passed as an
// argument to a UI call, and none of the four entries' own meaning has been read out of the image —
// they are named for the call that consumes them, not for what they describe.
inline constexpr uint32_t uiTable = 0x80097760u;
inline constexpr uint32_t uiTableQuadAssetRoot = 0u;
inline constexpr uint32_t uiTableQuadAssetEntry = 416u;
inline constexpr uint32_t uiTableBindEntryFirst = 808u;
inline constexpr uint32_t uiTableBindEntrySecond = 820u;

// ---------------------------------------------------------------------------------------------
// The asynchronous mode state values the port publishes
// ---------------------------------------------------------------------------------------------

// Written to `asyncModeState` as each mode finishes, and the value every mode waits to become zero.
inline constexpr uint32_t asyncModeStateAfterWipe = 0x001E005Au;
inline constexpr uint32_t asyncModeStateAwaitingMenu = 10u;
inline constexpr uint32_t asyncModeStateAfterMenu = 80u;
// The outer cycle's `case` values the port writes before re-entering a transition.
inline constexpr uint32_t outerRestartCaseValue = 5u;
inline constexpr uint32_t outerTransitionCaseValue = 20u;

// ---------------------------------------------------------------------------------------------
// The invalid-selector wait
// ---------------------------------------------------------------------------------------------

// The argument the invalid-selector input setup is given, and the fields it will wait before
// declaring the release never came.
inline constexpr uint32_t invalidWaitArgument = 0x80093C28u;
inline constexpr uint32_t invalidInputWaitFields = 240u;
inline constexpr uint32_t invalidInputTimeoutFields = 1800u;

// ---------------------------------------------------------------------------------------------
// Guest service entry points
// ---------------------------------------------------------------------------------------------
//
// The two argument-passing conventions, and the authenticated `jal` return address the native
// driver resumes at. Every return address below is the address the GUEST itself recorded in `r[31]`
// when it called the entry, read out of the image; none of them is invented by this port.

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

// The alternate mode's own three entries. Its per-field object update is NOT a fourth entry: the
// address the alternate mode gives to update its object is the same `uiUpdate` the menu uses, and
// the two names the port used for it were one function written down twice.
inline constexpr uint32_t alternateBegin = 0x800166A0u;
inline constexpr uint32_t alternateEffect = 0x8006D320u;
inline constexpr uint32_t alternateRelease = 0x800165FCu;

// ---------------------------------------------------------------------------------------------
// Literal arguments the modes pass
// ---------------------------------------------------------------------------------------------
//
// Every value a mode hands a guest call that is NOT a guest address, a field offset, or a mode
// selector. They are declared once here, at the name, so a reader can see that `256` in one mode
// and `256` in another are two different things.

// The primary warm-up's table initialization: an element count and a stride in bytes.
inline constexpr uint32_t primaryTableInitCount = 32u;
inline constexpr uint32_t primaryTableInitStride = 8u;
// The two resource ids the ResourcePrimary route looks up. The resource id space is not decoded, so
// these are named for the order they are searched in and nothing more.
inline constexpr uint32_t resourceFindFirstId = 4128u;
inline constexpr uint32_t resourceFindSecondId = 756u;
// The UI depth the menu and alternate modes set.
inline constexpr uint32_t uiDepth = 256u;
// The menu object: the bytes it is allocated with, and the width and height it is constructed with.
inline constexpr uint32_t menuAllocateBytes = 1160u;
inline constexpr uint32_t menuConstructWidth = 256u;
inline constexpr uint32_t menuConstructHeight = 112u;
// The menu's per-frame script geometry.
inline constexpr uint32_t menuTextX = 127u;
inline constexpr uint32_t menuTextY = 25u;
inline constexpr uint32_t menuTextLength = 33u;
inline constexpr uint32_t menuQuadX = 256u;
inline constexpr uint32_t menuQuadY = 64u;
inline constexpr uint32_t menuEffectX = 460u;
inline constexpr uint32_t menuEffectY = 173u;
inline constexpr uint32_t menuIntroEffectFrames = 2u;
// The sound both modes play on exit: an id and a volume.
inline constexpr uint32_t menuExitSoundId = 21u;
inline constexpr uint32_t alternateExitSoundId = 31u;
inline constexpr uint32_t exitSoundVolume = 8192u;
// The alternate mode's per-frame script geometry.
inline constexpr uint32_t alternateEffectX = 800u;
inline constexpr uint32_t alternateEffectY = 173u;
inline constexpr uint32_t alternateOverlayX = 800u;
inline constexpr uint32_t alternateOverlayY = 460u;
inline constexpr uint32_t alternateQuadX = 256u;
inline constexpr uint32_t alternateQuadY = 60u;
inline constexpr uint32_t alternateTextX = 77u;
inline constexpr uint32_t alternateTextY = 83u;
inline constexpr uint32_t alternateTextLength = 105u;
// The mode-object read mode, and the number of fields the field waits ask for.
inline constexpr uint32_t modeObjectOpenMode = 1u;
inline constexpr uint32_t modeObjectReadFrame = 0u;
inline constexpr uint32_t modeObjectReadAlternateFile = 1u;
inline constexpr uint32_t oneField = 1u;
// The outer-mode number the ResourcePrimary route enters its resource loader with, against the 0
// every other route's outer entry is given.
inline constexpr uint32_t resourceBeginModeNumber = 2u;
// The 3D wipe's own per-channel arithmetic, as it is in the guest body: each channel's new value is
// a share of the sum of the three old ones, and the two shares differ so the image darkens toward
// the red end of the spectrum.
inline constexpr uint32_t transitionChannelMask = 31u;
inline constexpr uint32_t transitionDimNumerator = 341u;
inline constexpr uint32_t transitionDimShift = 11u;
inline constexpr uint32_t transitionStrongNumerator = 1365u;
inline constexpr uint32_t transitionStrongShift = 12u;
inline constexpr uint32_t transitionSignBit = 0x8000u;

// ---------------------------------------------------------------------------------------------
// The pre-main boot: the fields, the services, and the crt0 register values
// ---------------------------------------------------------------------------------------------

// The entry that publishes the title's display-field callback, and the field wait the mode driver
// calls to ask for fields in the first place.
inline constexpr uint32_t vsyncCallbackRegistration = 0x8008B8CCu;
inline constexpr uint32_t guestFieldWait = 0x8005E748u;
// The directly-called stock CD sync body the host serves through the same complete/ready owner as
// the public wrapper.
inline constexpr uint32_t innerCdSync = 0x8008C944u;
// The stock GPU DMA timeout arm. Its retail body asks the title field count how long it may take,
// and this port answers from the title's own counter instead of transferring cadence back to guest
// libetc.
inline constexpr uint32_t gpuDmaTimeoutStart = 0x80083C60u;
inline constexpr uint32_t gpuDmaTimeoutDeadline = 0x800B0F64u;
inline constexpr uint32_t gpuDmaTimeoutPollCount = 0x800B0F68u;
inline constexpr uint32_t gpuDmaTimeoutFieldBudget = 240u;
// The instructions the retail timeout body would have executed, accounted so the executor's budget
// stays honest about work the host did instead.
inline constexpr uint32_t gpuDmaTimeoutRetailInstructions = 13u;
// The stock C entry point and the game-init function the finite prefix enters, and the return
// address inside game init whose pad service is the authenticated post-logo wait.
inline constexpr uint32_t libcEntry = 0x80087444u;
inline constexpr uint32_t gameInit = 0x8006BF9Cu;
inline constexpr uint32_t bootTailPadReturn = 0x8006C304u;
// The retail movie player entry the title reaches through the scoped-original runtime boundary.
inline constexpr uint32_t moviePlayer = 0x8002AA0Cu;
// The display field rate the boot fiber's elapsed-time host turn is paced at, in milliHz. NTSC is
// 59.94 fields per second and this is that number exactly rather than a rounded 60.
inline constexpr unsigned ntscFieldRateMilliHz = 59940u;

// The finite prefix's own guest stack frame. The retail main frame's callee-saved registers are
// spilled into the top of it, highest register first, and `r[29]` is lowered by this much first.
inline constexpr uint32_t bootPrefixFrameBytes = 72u;
// The register value the title's C runtime sets up and the prefix restores before handing over to
// the native mode driver. `$gp` is the small-data base and `$s8` the stack base, both from the o32
// ABI, so those two are named. The third is `$s7`, and nothing in this port says what it is for.
inline constexpr uint32_t crt0GlobalPointer = 0x800B0000u;
inline constexpr uint32_t crt0StackBase = 0x800A0000u;
inline constexpr uint32_t kUnattributedCrt0Register23 = 0x80090000u;

} // namespace spider::spider1
