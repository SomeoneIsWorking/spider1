// CD-stream service 0x8008C3E0 recovered from SLUS_008.75; its service gate is never raised.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lucent/log.h>

class Core;
class CdcState;

namespace spider::spider1::cdstream {
// The guest HW table at 0x800B3DD8 holds 0x1F801800..0x1F801803 in order, read by `lw` at
// 0x8008C3E8/0x8008C404/0x8008C480/0x8008C4EC.

inline constexpr std::uint32_t kIndexRegister = 0x1F801800u;        // table +0
inline constexpr std::uint32_t kResponseFifo = 0x1F801801u;         // table +4
inline constexpr std::uint32_t kResponseTypeRegister = 0x1F801803u; // table +12

// `sb $v0, 0($v1)` at 0x8008C3FC selects the response FIFO; `beq $v0, $zero` at 0x8008C474 branches
// on status bit 5 (RSLRRDY), so the guest drains only when a byte is available.
inline constexpr std::uint8_t kSelectResponseFifo = 1u;
inline constexpr std::uint32_t kStatusResponseReadyBit = 0x20u;

// Response type: `andi $v0, $v0, 7` at 0x8008C414; type 0 returns 0 (0x8008C424); `sltiu` at
// 0x8008C620 bounds dispatch to types 1..5.
inline constexpr std::uint8_t kResponseTypeMask = 0x07u;
inline constexpr std::uint8_t kTypeNone = 0u;
inline constexpr std::uint8_t kTypeDataReady = 1u;
inline constexpr std::uint8_t kTypeSectorBufferReady = 2u;
inline constexpr std::uint8_t kTypeCommandAcknowledge = 3u;
inline constexpr std::uint8_t kTypeReadState = 4u;
inline constexpr std::uint8_t kTypeError = 5u;
inline constexpr std::uint8_t kHighestResponseType = 5u;

// Drain bound `slti $v0, $s0, 8` at 0x8008C494, tail zero-filled by the loop at 0x8008C4B4.
inline constexpr std::uint32_t kResponseBytes = 8u;

// Type re-read loop 0x8008C42C..0x8008C454 (`bne` at 0x8008C450): act only on a type seen twice.
inline constexpr std::uint32_t kTypeRestabilityReads = 2u;

// Read by the three dead poll loops; reported, never written.
inline constexpr std::uint32_t kServiceGateAddress = 0x800B2886u;
// Reader `lhu` at 0x8008B904, called from 0x8008CA84, 0x8008CD04, 0x8008D160.
inline constexpr std::uint32_t kServiceGateReader = 0x8008B900u;
// The only writer clears: `sh $zero, 0x2886($at)` at 0x8008BBAC.
inline constexpr std::uint32_t kServiceGateClearer = 0x8008BBACu;

// Returned in `v0`; the poll loops test bit 2 (0x8008CABC) and bit 1 (0x8008CAC4) and exit on zero
// (0x8008CAB8).
inline constexpr std::uint32_t kResultDataReady = 0x04u;
inline constexpr std::uint32_t kResultCommandAcknowledge = 0x02u;
inline constexpr std::uint32_t kResultSectorBufferReady = 0x01u;

// `andi $s1, $v0, 0x001D` at 0x8008C580 keeps response byte 0's low bits for the arms.
inline constexpr std::uint32_t kResponseByteMask = 0x001Du;

// Table at 0x80096670 indexed by `type-1` (`lw` at 0x8008C634). Each arm returns the mask its
// response bytes name; data ready and read state also copy 8 bytes into 0x800C637C..0x800C638C.
//
// The status bytes are a triplet at 0x800B3DF0 (`s2` at 0x8008C994, +1 and +2 at 0x8008C998/C99C).
struct StatusTriplet {
  std::uint8_t sync;      // +0, 0x800B3DF0
  std::uint8_t ready;     // +1, 0x800B3DF1
  std::uint8_t readState; // +2, 0x800B3DF2
};

// The five arms in dispatch-table order; the table index is `responseType - 1`.
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

// The word the command-acknowledge arm tests: `lw` at 0x8008C6A8, `$at = 0x800B0000 + (byte at
// 0x800B3B31) * 4`; non-zero selects the middle path.
inline constexpr std::uint32_t kCommandAcknowledgeWord = 0x800B3BD8u;

inline constexpr std::uint32_t kStatusTripletAddress = 0x800B3DF0u;
inline constexpr std::size_t kStatusTripletSize = 3u;

// Per-command count/status words the service maintains beside the triplet.
inline constexpr std::uint32_t kCommandStatusWord = 0x800B3B20u;    // `sw $v0, 0x3B20($at)`
inline constexpr std::uint32_t kCommandStatusWordAlt = 0x800B3B24u; // `sw $v1, 0x3B24($at)`
inline constexpr std::uint32_t kErrorCount = 0x800B3B28u;           // `sw $v0, 0x3B28($at)`
inline constexpr std::uint32_t kCommandCount = 0x800B3B1Cu;         // `lw $v0, 0x3B1C($v0)`

// Guest RAM response buffers, from the `addiu` immediates in the arms' copy loops.
inline constexpr std::uint32_t kSyncResponseBuffer = 0x800C637Cu;
inline constexpr std::uint32_t kReadyResponseBuffer = 0x800C6384u;
inline constexpr std::uint32_t kReadStateResponseBuffer = 0x800C638Cu;

// The poll loops call whatever is in the slot (by stream time libstr's `StGetNext` consumer):
// a0 = *(u8*)0x800C6384, a1 = 0x800C6384, and for the sync callback a0 = *(u8*)0x800C637C. a1 is a
// guest buffer pointer, so passing 0 hands the consumer a null pointer.
inline constexpr std::uint32_t kReadyCallbackSlot = 0x800B3B18u;
inline constexpr std::uint32_t kSyncCallbackSlot = 0x800B3B14u;

// Polled by four direct `jal` sites in a `do { r = service(); } while (r)` loop, not an interrupt.
inline constexpr std::uint32_t kServiceBody = 0x8008C3E0u;
inline constexpr std::uint32_t kPollSites[4] = {0x8008CAACu, 0x8008CD2Cu, 0x8008D188u, 0x8008DA58u};

// Interrupt-chain head from `sw $a1, 0x1524($at)` at 0x8008DC14 (reads at 0x8008DC40/0x80086B8C).
inline constexpr std::uint32_t kInterruptChainHead = 0x800C1520u;
inline constexpr std::uint32_t kInterruptChainElement = 0x800C1528u;

} // namespace spider::spider1::cdstream

namespace spider::spider1 {

// One owner of guest code 0x8008C3E0: a state machine over one device with per-arm run counts.
class CdStreamService final {
public:
  // Install the native override and sample the gate word once. State is process-lifetime on the
  // title runtime, like `Spider1Widescreen`.
  void install(Core &core);

  // One service call: select the response FIFO, read the type twice and act only if stable, drain
  // up to eight bytes, publish the status, run the arm, and return the guest result mask in `v0`.
  std::uint32_t service(Core &core);

  // The gate is a u16 at an address 2 mod 4, read as two bytes; `Core` is non-const for `mem_r8`.
  [[nodiscard]] std::uint32_t gateObserved(Core &core) const;

  // The recovered pure rules, exposed so a test can assert them without a `Core`.
  [[nodiscard]] std::uint32_t typeIsDispatchable(std::uint8_t type) const noexcept;
  [[nodiscard]] std::uint32_t resultHasDataReady(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint32_t resultHasCommandAcknowledge(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint32_t resultHasSectorBufferReady(std::uint32_t result) const noexcept;
  [[nodiscard]] std::uint8_t carry(std::uint8_t responseByte0) const noexcept;
  [[nodiscard]] std::uint32_t armForResponseType(std::uint8_t type) const noexcept;

  // Report counts with denominators: "0 errors" beside "0 responses" means the service never ran.
  void report() const;

private:
  // One drained controller response; the guest uses eight bytes of a 16-byte stack slot.
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

  // Native-override entry point; a `void (*)(Core *)` reaches the instance through the title
  // runtime.
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
