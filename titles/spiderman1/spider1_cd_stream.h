// spider1_cd_stream.h — Spider-Man 1's CD-stream service, recovered from SLUS_008.75's bytes.
//
// WHY THIS FILE IS THE ROOT CAUSE AND NOT A COMMENT ABOUT ONE. The black-picture frontier
// (docs/issues/0024, 0025, 0026) recorded the guest's routine `0x8008C3E0` as "the CD-ROM
// interrupt handler" and named an unowned callback as the reason. Both are wrong in a way that
// pointed the fix at the wrong subsystem, and the correction is structural, not cosmetic:
//
//   `0x8008C3E0` is POLLED, not interrupted. tools/re_cd_stream.py measures exactly four direct
//   `jal` sites (0x8008CAAC, 0x8008CD2C, 0x8008D188, 0x8008DA58) and ZERO `lui+addiu`
//   materialisations, ZERO stored pointers and ZERO jumps through a materialised address across
//   all 186,880 text words. Nothing routes an IRQ to it. Its own body contains no `jalr` and no
//   branch back into itself. Every one of the four sites is a `do { r = service(); } while (r)`
//   poll loop, and the loop's continuation is decided by bits 4 and 2 of the returned mask.
//
// So the title cannot learn that a sector arrived from these sites, because the only code that
// would tell it is a poll, and three of the four polls are behind a gate word that is ZERO in the
// image:
//
//   0x8008B900  lui $v0,0x800B ; lhu $v0,0x2886($v0) ; jr $ra
//
// Three of the four call sites call 0x8008B900 and do `beq $v0,$zero, <skip the whole service>`
// -- MEASURED, see the correction below.
//
// CORRECTION 2026-09-29, FROM AN INDEPENDENT CENSUS OF THE WRITERS. The first draft of this header
// claimed the gate word "IS the black screen, in one gate word", and named FOUR gated poll loops.
// Both are wrong, and the second error is the one that would have shipped a regression:
//
//   1. There are THREE `jal 0x8008B900` sites in the whole image, not four: 0x8008CA84, 0x8008CD04
//      and 0x8008D160. Each is followed 8 bytes later by `beq $v0,$zero` whose target is PAST the
//      service call it skips (0x8008CA8C -> 0x8008CB34, skipping the `jal` at 0x8008CAAC; the other
//      two skip 0x8008CD2C and 0x8008D188 the same way). That is a CLOSED argument rather than a
//      scan that failed to find a fourth reader: three gated sites each gate one of the first
//      three service call sites, so the fourth, 0x8008DA58, is gated by nothing and DOES run in
//      retail. Exactly one of the four sites reaches the service.
//   2. NOTHING IN THE IMAGE EVER SETS THE GATE. A census of every store whose byte range covers
//      0x800B2886 -- direct `sh`/`sw`/`sb` at $at-relative offsets 0x2880..0x2888, stores through
//      a register materialised by any lui/addiu/ori chain, and stores through a register of any
//      other provenance -- returns EXACTLY ONE instruction: the clearer at 0x8008BBAC. That one is
//      the DELAY SLOT of `jal 0x80091620` at 0x8008BBA8, so it is a field clear performed by a call
//      the CD driver makes while initialising, not a service. There is no setter.
//      0x800B2886 is also not code: 0x800B2870..0x800B287C is the ASCII "gi Re served" and
//      0x800B2884 onward is a zeroed DATA region that is filled at run time.
//
//   CONSEQUENCE, and it is the whole point of the correction: the three gated poll loops are DEAD
// CODE IN RETAIL. The gate is a flag nothing raises, so the guest never services through them, and
// writing a non-zero value into 0x800B2886 would not "open a gate the title closed" -- it would
// fabricate guest state the retail image never has and would additionally enable three poll loops
// retail never executes. This is the DEAD-TAP failure this workspace has already paid for twice
// (the `is3d` counter, the VSync(0) census): a word that reads zero and looks like a cause, read by
// a scan that cannot see that nothing writes it.
//
// SO THIS OWNER DOES NOT WRITE THE GATE, and that is a decision rather than an omission. The real
// frontier is already measured and is not here: `spider1_cd_initialization.cpp` and
// `spider1_platform_facts.h` record that the guest's CD service reaches the title through the BIOS
// HwCD hardware event class 0xF0000003, and that `Hle::deliverEvent` has no arm for it because the
// BIOS's own CD-ROM interrupt handler is ROM code this port does not have (docs/issues/0026). This
// owner makes the ONE site that retail does execute readable in C++; it does not pretend to be the
// missing BIOS handler.
//
// WHAT THIS FILE OWNS. Not the interrupt (there is none to own) and not the callback delivery
// (the framework's `cd_ready_delivery.cpp` already owns that seam). It owns the RECOVERED
// SERVICE BEHAVIOUR: the register reads, the response drain, the five-arm dispatch, the status
// bytes, and the result mask. Those are title facts read from the executable, and the framework
// cannot know them.
//
// WHAT IS NOT CLAIMED. That opening the gate makes a picture. `docs/issues/0025`'s refusal stands
// until pixels say otherwise, and this header exists so that when they do, the thing that
// changed is a name.
//
// AND, MEASURED, THAT THE OVERRIDE IS REACHED AT ALL. A real-disc headless run on 2026-09-28 with
// `PSXPORT_DEBUG=cd,cdstream` produced `[cd] CdRead 1 sector(s) x 2048 bytes from LBA 16 ...` and
// four more native read lines -- the POSITIVE CONTROL proving the channel plumbing was live in the
// same process, the same run -- and ZERO `[cdstream]` lines. The override is installed (the
// `lucent::info("cd", ...)` line above names the address) and it is NEVER ENTERED, because every
// caller of 0x8008C3E0 is a libcd routine this port already replaces natively: the framework's
// `cd_override.cpp` owns 0x8008C1EC as "the single synchronous read primitive" and its own header
// says it "bypasses the whole FUN_8008c960/c5d8/cafc/ac34 command+IRQ machinery for data", and the
// title additionally owns CdRead 0x80089ECC, CdReadSync 0x8008A068 and CD_cw 0x8008CE8C through
// `spider1_platform_facts.h`. So the guest's own poll loops do not run in this product. That is a
// fact about the callers, not about the recovered service, and it is the next thing to settle.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lucent/log.h>

class Core;
class CdcState;

namespace spider::spider1::cdstream {

// ---- the device side, through the guest's own hardware-address table -----------------------
//
// Measured from the image: the guest's HW table at 0x800B3DD8 holds 0x1F801800, 0x1F801801,
// 0x1F801802, 0x1F801803 in that order (0x800B3DD8..0x800B3DE4, read as four `lw` at
// 0x8008C3E8/0x8008C404/0x8008C480/0x8008C4EC). So the four CD-ROM registers the service touches
// are the ones the guest named, and the service's register accesses are:

inline constexpr std::uint32_t kIndexRegister = 0x1F801800u;        // table +0
inline constexpr std::uint32_t kResponseFifo = 0x1F801801u;         // table +4
inline constexpr std::uint32_t kResponseTypeRegister = 0x1F801803u; // table +12

// The `sb $v0, 0($v1)` at 0x8008C3FC writes 1 to the index register before the service reads the
// response, and the `beq $v0, $zero` at 0x8008C474 then branches on the SAME register's bit 5.
// In the framework's `cdc_read` that is case 0, the status register, and bit 5 is RSLRRDY
// (`kCdcStatResponseReady` in cd_ready_delivery.cpp's own list). So the guest selects the response
// FIFO, reads the status, and only drains if a response byte is available.
inline constexpr std::uint8_t kSelectResponseFifo = 1u;
inline constexpr std::uint32_t kStatusResponseReadyBit = 0x20u;

// The response type is the low three bits of bank 1 of the interrupt-flag register:
// `andi $v0, $v0, 7` at 0x8008C414. `beq $v0, $zero, 0x8008C92C` at 0x8008C424 returns 0 for
// type 0, and `sltiu $v0, $v1, 5` at 0x8008C620 bounds the dispatch to types 1..5.
inline constexpr std::uint8_t kResponseTypeMask = 0x07u;
inline constexpr std::uint8_t kTypeNone = 0u;
inline constexpr std::uint8_t kTypeDataReady = 1u;
inline constexpr std::uint8_t kTypeSectorBufferReady = 2u;
inline constexpr std::uint8_t kTypeCommandAcknowledge = 3u;
inline constexpr std::uint8_t kTypeReadState = 4u;
inline constexpr std::uint8_t kTypeError = 5u;
inline constexpr std::uint8_t kHighestResponseType = 5u;

// The drain: `slti $v0, $s0, 8` at 0x8008C494, zero-filling the tail with the
// `sb $zero, 0($v0)` loop at 0x8008C4B4. Eight bytes, which is a whole response.
inline constexpr std::uint32_t kResponseBytes = 8u;

// The response-type re-read loop at 0x8008C42C..0x8008C454: read the type byte, read it again,
// and if it changed, read it again. `bne $v1, $v0, 0x8008C434`. The service does not act on a type
// it did not see twice.
inline constexpr std::uint32_t kTypeRestabilityReads = 2u;

// ---- the gate ---------------------------------------------------------------------------

// The u16 read by the three DEAD poll loops before touching the CD-ROM. It is reported, never
// written: see the correction at the top of this file for why writing it would be fabrication.
inline constexpr std::uint32_t kServiceGateAddress = 0x800B2886u;
// The reader, `lhu $v0, 0x2886($v0)` at 0x8008B904, and the THREE (not four) call sites that do
// `jal 0x8008B900` + `beq $v0, $zero, <skip>`: 0x8008CA84, 0x8008CD04, 0x8008D160.
inline constexpr std::uint32_t kServiceGateReader = 0x8008B900u;
// Its ONLY writer in the whole image, and it clears: `sh $zero, 0x2886($at)` at 0x8008BBAC.
// There is no setter.
inline constexpr std::uint32_t kServiceGateClearer = 0x8008BBACu;

// ---- the result mask ---------------------------------------------------------------------

// Returned in `v0`. The poll loops test bit 2 and bit 1:
//   0x8008CABC  andi $v0, $s0, 0x0004 ; beq $v0, $zero, <next>
//   0x8008CAC4  andi $v0, $s0, 0x0002 ; beq $v0, $zero, <next>
// and `beq $s0, $zero, 0x8008CB24` at 0x8008CAB8 exits the loop on zero.
inline constexpr std::uint32_t kResultDataReady = 0x04u;
inline constexpr std::uint32_t kResultCommandAcknowledge = 0x02u;
inline constexpr std::uint32_t kResultSectorBufferReady = 0x01u;

// The per-response carry: `andi $s1, $v0, 0x001D` at 0x8008C580 keeps response byte 0's low bits
// as the `s1` the five arms branch on.
inline constexpr std::uint32_t kResponseByteMask = 0x001Du;

// ---- the five dispatch arms ---------------------------------------------------------------
//
// Table at 0x80096670, indexed by `type-1`, loaded by `lw $v0, 0x6670($at)` at 0x8008C634 with
// `at = 0x80090000 + (type-1)*4`. The five table words, read in index order, are:
//
//   [0] 0x8008C790  data ready
//   [1] 0x8008C744  sector buffer ready
//   [2] 0x8008C644  command acknowledge
//   [3] 0x8008C810  read state
//   [4] 0x8008C890  error
//
// (The first draft of tools/re_cd_stream.py listed these in a different order and the tool went
// red on its own author. The index is `type-1`, which is why.)

// The arms' observable effects, all of them guest RAM writes:
//
//  data ready (0x8008C790)
//      `sb $v1, 1($v0)` with $v0 = 0x800B3DF0 writes the SYNC status byte
//      `sb $zero, 0($v0)` at 0x8008C7F8 clears the READY status byte
//      copies 8 bytes into BOTH 0x800C6384 and 0x800C637C
//      `addiu $v0, $zero, 4` in the delay slot at 0x8008C808 -> returns 4
//  sector buffer ready (0x8008C744)
//      `sb $v0, 0x800B3DF0` writes 2 or 5 depending on `s1`
//      copies 8 bytes into 0x800C637C
//      `addiu $v0, $zero, 2` at 0x8008C78C -> returns 2
//  command acknowledge (0x8008C644)
//      writes 5, 3 or 2 to 0x800B3DF0 by three paths
//      copies 8 bytes into 0x800C637C
//      returns 5, 1 or 2
//  read state (0x8008C810)
//      `sb $v1, 2($v0)` writes 4 to 0x800B3DF2, then `sb $v1, 1($v0)` to 0x800B3DF1
//      copies 8 bytes into 0x800C638C AND 0x800C6384
//      `addiu $v0, $zero, 4` at 0x8008C88C -> returns 4
//  error (0x8008C890)
//      `sb $v1, 1($v0)` writes 5 to 0x800B3DF1, then `sb $v1, 0($v0)` to 0x800B3DF0
//      copies 8 bytes into 0x800C637C AND 0x800C6384
//      `addiu $v0, $zero, 6` at 0x8008C90C -> returns 6
//
// The three status bytes are a three-byte structure at 0x800B3DF0: `addiu $s5, $s2, 1` at
// 0x8008C998 and `addiu $s6, $s2, 2` at 0x8008C99C, with `s2 = 0x800B3DF0` at 0x8008C994.
struct StatusTriplet {
  std::uint8_t sync;      // +0, 0x800B3DF0
  std::uint8_t ready;     // +1, 0x800B3DF1
  std::uint8_t readState; // +2, 0x800B3DF2
};

// The five arms as a named table, in the order the words are READ from 0x80096670, which is index
// `responseType - 1`. Named rather than five loose constants because the index arithmetic is
// exactly the thing the recovery tool's own selftest caught being wrong on the first draft.
enum : std::uint32_t {
  kArmDataReady = 0x8008C790u,
  kArmSectorBufferReady = 0x8008C744u,
  kArmCommandAcknowledge = 0x8008C644u,
  kArmReadState = 0x8008C810u,
  kArmError = 0x8008C890u,
  kUndispatchedType = 0u,
};

struct DispatchArm {
  std::uint8_t responseType; // 1..5, the type that selects this arm
  std::uint32_t address;     // the table word's value, i.e. the arm's guest PC
};

inline constexpr std::array<DispatchArm, 5> kDispatchTable{{
    {1, kArmDataReady},
    {2, kArmSectorBufferReady},
    {3, kArmCommandAcknowledge},
    {4, kArmReadState},
    {5, kArmError},
}};

// The guest word the command-acknowledge arm loads and tests: `lw $v0, 0x3BD8($at)` at 0x8008C6A8
// with `$at = 0x800B0000 + (byte at 0x800B3B31) * 4`. Non-zero selects the arm's middle path.
inline constexpr std::uint32_t kCommandAcknowledgeWord = 0x800B3BD8u;

inline constexpr std::uint32_t kStatusTripletAddress = 0x800B3DF0u;
inline constexpr std::size_t kStatusTripletSize = 3u;

// The two per-command count/status words the service maintains alongside the triplet.
inline constexpr std::uint32_t kCommandStatusWord = 0x800B3B20u;    // `sw $v0, 0x3B20($at)`
inline constexpr std::uint32_t kCommandStatusWordAlt = 0x800B3B24u; // `sw $v1, 0x3B24($at)`
inline constexpr std::uint32_t kErrorCount = 0x800B3B28u;           // `sw $v0, 0x3B28($at)`
inline constexpr std::uint32_t kCommandCount = 0x800B3B1Cu;         // `lw $v0, 0x3B1C($v0)`

// The three guest RAM response buffers, from the `addiu` immediates in the arms' copy loops.
inline constexpr std::uint32_t kSyncResponseBuffer = 0x800C637Cu;
inline constexpr std::uint32_t kReadyResponseBuffer = 0x800C6384u;
inline constexpr std::uint32_t kReadStateResponseBuffer = 0x800C638Cu;

// ---- the two callback slots and the ABI the poll loops actually build ------------------------
//
// CORRECTION 2026-09-28 to `cd_ready_delivery.cpp`'s own comment, which says the installed
// callbacks "read NEITHER argument" and therefore passes `a0 = 1, a1 = 0`. That is true of
// 0x8008A260 and of libstr's 0x800860B4. It is FALSE of what the poll loops pass, because the
// loops do not call a known function -- they call whatever is IN the slot, and by stream time that
// is libstr's `StGetNext` consumer. The recovered call shape at all four sites is:
//
//   a0 = *(u8*)0x800C6384   (`lbu $a0, 0($s4)`, with $s4 = 0x800C6384 at 0x8008C988)
//   a1 = 0x800C6384         (`addiu $a1, $a1, 25476` in the `jalr` delay slot)
//
// and for the sibling sync callback, a0 = *(u8*)0x800C637C with a1 = 0x800C637C. **a0 is a value
// read from guest RAM and a1 is a POINTER to a guest buffer.** Passing 0 for a1 hands the consumer
// a null pointer, and the framework's own comment already names the reason it cannot know: "the
// layout declares the slot and not the guest's private buffer". That admission is the defect.
inline constexpr std::uint32_t kReadyCallbackSlot = 0x800B3B18u;
inline constexpr std::uint32_t kSyncCallbackSlot = 0x800B3B14u;

// ---- the call sites ------------------------------------------------------------------------

inline constexpr std::uint32_t kServiceBody = 0x8008C3E0u;
inline constexpr std::uint32_t kPollSites[4] = {0x8008CAACu, 0x8008CD2Cu, 0x8008D188u, 0x8008DA58u};

// The guest's interrupt-chain head, from `sw $a1, 0x1524($at)` at 0x8008DC14 and the reads at
// 0x8008DC40/0x80086B8C. Issue 0026 recorded 0x800C1528 as the element; 0x800C1520 is the head
// pointer the element is linked from.
inline constexpr std::uint32_t kInterruptChainHead = 0x800C1520u;
inline constexpr std::uint32_t kInterruptChainElement = 0x800C1528u;

} // namespace spider::spider1::cdstream

namespace spider::spider1 {

// The recovered CD-stream service, as one cohesive owner of guest code 0x8008C3E0.
//
// It is a class and not four free functions because the recovered behaviour is a STATE MACHINE
// over one device: a type read that must be stable, a bounded drain, a five-arm dispatch, and a
// status triplet that each arm writes. Splitting it would put the state in globals and make the
// count denominators unattributable, which is the failure this repository's diagnostics keep
// catching.
class CdStreamService final {
public:
  // Install the native override for the guest's service body and sample the gate word once, at the
  // instant the override goes in. The state is process-lifetime and lives on the title runtime,
  // which is the same honest precedent `Spider1Widescreen` sets on that class: this service
  // outlives every frame and is not a per-frame cadence concern.
  void install(Core &core);

  // One service call: select the response FIFO, read the type twice and act only if stable,
  // drain up to eight bytes, publish the recovered status, run the recovered arm, and return the
  // guest's own result mask in `v0`.
  //
  // The return value is the guest's ABI: 0 for "nothing happened", and otherwise bits 2 and 1 that
  // the guest's own poll loops test. Returning anything else would make those loops misread the
  // service.
  std::uint32_t service(Core &core);

  // The guest's gate word, read as two bytes because it is a u16 at an address 2 mod 4. Reported,
  // never written: see the correction at the top of this file for why writing it would fabricate
  // guest state. `Core` is taken by non-const reference because the framework's `mem_r8` is a
  // non-const reader; the method itself is const because it does not touch its own counters.
  [[nodiscard]] std::uint32_t gateObserved(Core &core) const;

  // The recovered pure rules, exposed so a test can assert them without a `Core`. Each is the
  // exact branch the guest's own code takes, and each has a matching byte assertion in
  // tools/re_cd_stream.py.
  [[nodiscard]] std::uint32_t typeIsDispatchable(std::uint8_t type) const noexcept;
  [[nodiscard]] std::uint32_t resultHasDataReady(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint32_t resultHasCommandAcknowledge(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint32_t resultHasSectorBufferReady(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint8_t carry(std::uint8_t responseByte0) const noexcept;
  [[nodiscard]] std::uint32_t armForResponseType(std::uint8_t type) const noexcept;

  // Report the counts with their denominators, because "0 errors" beside "0 responses" means the
  // service never ran and not that the title is healthy.
  void report() const;

private:
  // One drained controller response. The guest's is a 16-byte stack slot of which it uses eight.
  struct Response {
    std::array<std::uint8_t, cdstream::kResponseBytes> bytes{};
    std::uint32_t drained = 0;
    std::uint8_t carry = 0;
  };

  std::uint8_t readResponseType(CdcState &cdc) const;
  std::uint8_t readResponseByte(CdcState &cdc) const;
  bool responseByteAvailable(CdcState &cdc) const;
  void publishStatus(Core &core, std::uint8_t type, const Response &response);
  std::uint32_t runArm(Core &core, std::uint8_t type, const Response &response);
  void copyResponse(Core &core, std::uint32_t destination, const Response &response) const;

  // The native-override entry point. A `void (*)(Core *)` cannot carry the owner, so it reaches
  // the instance through the title runtime the same way `Spider1StreamDriver::from` reaches its
  // own per-Core owner: the Core knows its runtime, and the runtime owns the state.
  static void serviceGuest(Core *core);

  std::uint32_t services_ = 0;
  std::uint32_t noResponse_ = 0;
  std::uint32_t errors_ = 0;
  std::uint32_t undispatched_ = 0;
  std::uint32_t unstableTypeReads_ = 0;
  std::array<std::uint32_t, 6> byType_{};
  std::uint32_t gateObserved_ = 0;
};

} // namespace spider::spider1
