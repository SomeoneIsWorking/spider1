// spider1_cd_stream.cpp — the recovered CD-stream service, as a native owner of 0x8008C3E0.
//
// THE SHAPE OF THE BUG, restated because it decides what this class is for.
//
//   Spider-Man 1 has no CD-ROM interrupt handler. `0x8008C3E0` -- the routine the workspace map
//   called one -- is reached by exactly four direct `jal` sites, all of which are POLL loops
//   (`do { r = 0x8008C3E0(); } while (r)`), and by nothing else in the image. Three of the four
//   first call `0x8008B900`, which reads the u16 at `0x800B2886`, and skip the entire service when
//   it is zero. It is zero -- and an exhaustive writer census finds NO SETTER, only the one
//   clearing `sh` at 0x8008BBAC, so those three loops are dead code in retail and the gate is not
//   a cause this port may "open". The fourth site, 0x8008DA58, is ungated and DOES run.
//
//   So this owner replaces the one site retail actually executes. It is a readability and
//   debuggability win on a measured contract, and it is explicitly NOT the fix for the black
//   picture: that frontier is the missing BIOS HwCD hardware-event delivery recorded in
//   spider1_cd_initialization.cpp and docs/issues/0026.
//
// WHY A NATIVE OVERRIDE IS THE RIGHT SHAPE AND NOT A BAND-AID. The service body is title code with
// a measured contract: which registers it reads, that it reads the response type TWICE and acts
// only on a stable value, that it drains at most eight bytes, that it dispatches on type-1 through
// a five-entry table, and that it writes a three-byte status triplet. All of that is recovered from
// the image in `spider1_cd_stream.h` and checked against those same bytes. Owning it means the
// service is READABLE and DEBUGGABLE in C++ rather than an opaque translated block, which is the
// whole point of the decompile work order (psxport issue 0135): recover the code on the path of a
// visible defect that has no native owner, then own it.
//
// WHAT THIS DOES NOT DO, deliberately.
//   * It does not fabricate a sector. Every byte this returns was drained from the controller's
//     response FIFO. An empty FIFO yields `kResultNone`, exactly as the guest's own code would.
//   * It does not write the gate word. The gate is a flag NOTHING in the image ever sets, so the
//     three loops behind it are dead code in retail; writing it would fabricate guest state and
//     enable loops retail never runs. See the correction at the top of spider1_cd_stream.h. The
//     service runs only where the guest itself calls it, and the gate word is left alone and
//     reported by `gateObserved()`.
//   * It does not call the ready callback. The framework's `cd_ready_delivery.cpp` owns that seam
//     and this must not double-deliver. This owner produces the recovered RESULT; the framework
//     delivers it.

#include "spider1_cd_stream.h"

#include "spider1_platform_facts.h"
#include "spider1_runtime.h"

#include "cdc_state.h"
#include "core.h"
#include "game.h"
#include "native_execution.h"

#include <lucent/log.h>

namespace spider::spider1 {
namespace {

// The recovered epilogue's `v0` for a response the service could not classify. The guest's own
// default arm is 0x8008C910: it prints "CDROM: unknown intr" / "(%d)" and falls into
// 0x8008C92C, which is `addu $v0, $zero, $zero`. So the guest returns ZERO for an unclassifiable
// response, and so do we -- a port that returned a non-zero mask here would make the guest's poll
// loop spin on a response the guest itself treats as "nothing happened".
constexpr std::uint32_t kResultNone = 0u;

// How many times the service re-reads the response type before acting. The guest's loop is
// unbounded in principle (`bne $v1, $v0, 0x8008C434` with no counter), so a bound is REQUIRED
// here: an unbounded loop in native code over a device read is a hang, and a port that can hang
// where the guest merely spins is a regression. The bound is generous relative to what the
// controller does -- a response type does not change within eight consecutive reads of the same
// register -- and hitting it is REPORTED, never silently absorbed.
constexpr std::uint32_t kMaxTypeReads = 8u;

// The framework's CD-ROM registers, named here once so the class reads as the recovered service
// rather than as a pile of magic addresses. The guest reaches them through its own hardware-address
// table at 0x800B3DD8, whose four words are 0x1F801800, 0x1F801801, 0x1F801802, 0x1F801803.
constexpr std::uint32_t kCdcIndex = 0u;    // 0x1F801800, selected by the register's low two bits
constexpr std::uint32_t kCdcResponse = 1u; // 0x1F801801, bank 1: the response FIFO
constexpr std::uint32_t kCdcIrqFlag = 3u;  // 0x1F801803, bank 1: type in the low three bits

} // namespace

// ---- installation -------------------------------------------------------------------------

void CdStreamService::install(Core &core) {
  // Sample the gate once, here, rather than reporting a constant. `gateObserved_` is written from
  // this read and re-read on every service call, so the number in the report is an OBSERVATION of
  // the guest's own word and not a literal that happens to agree with it.
  gateObserved_ = gateObserved(core);
  installNativeOverride(core, cdstream::kServiceBody, "Spider CD stream service", serviceGuest);
  lucent::info(
      "cd",
      "Spider-Man 1 installed the recovered CD service 0x{:08X}. Guest gate 0x{:08X} = {} at "
      "install. The override replaces the routine only: the caller is still the guest's own poll "
      "loop, which still performs the result-mask test and still makes the callback call.",
      cdstream::kServiceBody,
      cdstream::kServiceGateAddress,
      gateObserved_);
}

void CdStreamService::serviceGuest(Core *core) {
  // The guest's ABI: the result mask comes back in `v0`, and the poll loop resumes at the
  // instruction after its own `jal`. `v0` is written explicitly so the contract does not depend on
  // the override seam happening to leave the register alone.
  auto &runtime = spider::Spider1Runtime::from(*core);
  core->r[2] = runtime.cdStream().service(*core);
}

// ---- the pure recovered rules, testable without a Core -------------------------------------

std::uint32_t CdStreamService::typeIsDispatchable(std::uint8_t type) const noexcept {
  // `beq $v0, $zero, 0x8008C92C` for type 0, and `sltiu $v0, $v1, 5` over `type-1` for the rest.
  return type >= cdstream::kTypeDataReady && type <= cdstream::kHighestResponseType ? 1u : 0u;
}

std::uint32_t CdStreamService::resultHasDataReady(std::uint32_t result) const noexcept {
  // `andi $v0, $s0, 0x0004` at every poll site.
  return (result & cdstream::kResultDataReady) != 0u ? 1u : 0u;
}

std::uint32_t CdStreamService::resultHasCommandAcknowledge(std::uint32_t result) const noexcept {
  // `andi $v0, $s0, 0x0002` at every poll site.
  return (result & cdstream::kResultCommandAcknowledge) != 0u ? 1u : 0u;
}

std::uint32_t CdStreamService::resultHasSectorBufferReady(std::uint32_t result) const noexcept {
  // Bit 0, returned by the command-acknowledge arm's `adiu $v0, $zero, 1` at 0x8008C6F8.
  return (result & cdstream::kResultSectorBufferReady) != 0u ? 1u : 0u;
}

std::uint8_t CdStreamService::carry(std::uint8_t responseByte0) const noexcept {
  // `andi $s1, $v0, 0x001D` at 0x8008C580.
  return static_cast<std::uint8_t>(responseByte0 & cdstream::kResponseByteMask);
}

// The five-arm dispatch, as the recovered table read at 0x80096670. The arms are indexed by
// `type-1`, which is the correction the tool caught on its own first draft.
std::uint32_t CdStreamService::armForResponseType(std::uint8_t type) const noexcept {
  for (const auto &arm : cdstream::kDispatchTable) {
    if (arm.responseType == type) {
      return arm.address;
    }
  }
  return cdstream::kUndispatchedType;
}

std::uint32_t CdStreamService::gateObserved(Core &core) const {
  // The gate is a u16 at 0x800B2886, which is 2 mod 4, so it is read as two bytes rather than as
  // a word's low half. That distinction is the same one the recovery tool's selftest caught.
  const auto lo = core.mem_r8(cdstream::kServiceGateAddress);
  const auto hi = core.mem_r8(cdstream::kServiceGateAddress + 1);
  return static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 8);
}

// ---- the recovered service body -----------------------------------------------------------

// The guest's 0x8008C3E0, in order, with the byte evidence for each step:
//
//   0x8008C3FC  sb $v0,0($v1)                  select the response FIFO in the index register
//   0x8008C414  andi $v0,$v0,7                 response type = interrupt flag & 7
//   0x8008C424  beq $v0,$zero,0x8008C92C       type 0 -> return 0
//   0x8008C42C  j   0x8008C444                  enter the re-read loop
//   0x8008C450  bne $v1,$v0,0x8008C434         if the type changed, read it again
//   0x8008C474  beq $v0,$zero,0x8008C4A0       if no response byte is ready, skip the drain
//   0x8008C488  lbu $v0,0($v0)                 drain one byte
//   0x8008C494  slti $v0,$s0,8                 at most eight
//   0x8008C4B4  sb   $zero,0($v0)              zero-fill the tail
//   0x8008C580  andi $s1,$v0,0x001D            carry = response byte 0 & 0x1D
//   0x8008C634  lw   $v0,0x6670($at)           dispatch table[responseType - 1]
//
// Every one of those was asserted against the authenticated image bytes.
std::uint32_t CdStreamService::service(Core &core) {
  auto &cdc = core.game->cdc;
  ++services_;
  // Re-read the guest's own gate word on every call, so the reported value is the last one the
  // guest actually had rather than the one it had when the override was installed.
  gateObserved_ = gateObserved(core);

  // Step 1: select the response FIFO. The guest writes the index register through its own hardware
  // address table, which is the same register the framework models as `kCdcIndex`.
  core.mem_w8(cdstream::kIndexRegister, cdstream::kSelectResponseFifo);

  // Step 2: the response type, read TWICE and acted on only if stable. See kMaxTypeReads for why
  // the loop is bounded here when the guest's is not.
  std::uint8_t type = readResponseType(cdc);
  std::uint8_t confirm = readResponseType(cdc);
  std::uint32_t reads = cdstream::kTypeRestabilityReads;
  while (confirm != type && reads < kMaxTypeReads) {
    type = confirm;
    confirm = readResponseType(cdc);
    ++reads;
  }
  if (confirm != type) {
    ++unstableTypeReads_;
    lucent::debug("cdstream",
                  "CD service: response type still changing after {} reads (last 0x{:02X} vs "
                  "0x{:02X}) -- treating the response as unclassifiable, as the guest's own "
                  "0x8008C910 arm does",
                  reads,
                  type,
                  confirm);
    return kResultNone;
  }

  // Step 3: the guest's own `beq $v0, $zero, 0x8008C92C` for type 0.
  if (!typeIsDispatchable(type)) {
    ++noResponse_;
    return kResultNone;
  }

  // Step 4: drain the response, at most eight bytes, zero-filling the tail.
  Response response{};
  std::uint32_t drained = 0;
  while (drained < cdstream::kResponseBytes && responseByteAvailable(cdc)) {
    response.bytes[drained] = readResponseByte(cdc);
    ++drained;
  }
  if (drained == 0) {
    ++noResponse_;
    return kResultNone;
  }
  response.drained = drained;

  // Step 5: publish the recovered status, then run the recovered arm.
  publishStatus(core, type, response);

  const std::uint32_t result = runArm(core, type, response);
  ++byType_[type & 0x7u];
  lucent::debug("cdstream",
                "CD service: response type {} -> result 0x{:02X} ({} bytes, carry 0x{:02X}, arm "
                "0x{:08X})",
                type,
                result,
                drained,
                response.carry,
                armForResponseType(type));
  return result;
}

std::uint8_t CdStreamService::readResponseType(CdcState &cdc) const {
  const int bank = cdc.index;
  cdc_write(&cdc, kCdcIndex, cdstream::kSelectResponseFifo);
  const auto raw = static_cast<std::uint8_t>(cdc_read(&cdc, kCdcIrqFlag));
  cdc_write(&cdc, kCdcIndex, static_cast<std::uint8_t>(bank));
  return static_cast<std::uint8_t>(raw & cdstream::kResponseTypeMask);
}

bool CdStreamService::responseByteAvailable(CdcState &cdc) const {
  // The guest reads the index register itself and tests bit 5, which in the framework's `cdc_read`
  // is the STATUS register's RSLRRDY -- a response byte is available.
  const int bank = cdc.index;
  cdc_write(&cdc, kCdcIndex, cdstream::kSelectResponseFifo);
  const auto status = cdc_read(&cdc, kCdcIndex);
  cdc_write(&cdc, kCdcIndex, static_cast<std::uint8_t>(bank));
  return (status & cdstream::kStatusResponseReadyBit) != 0u;
}

std::uint8_t CdStreamService::readResponseByte(CdcState &cdc) const {
  const int bank = cdc.index;
  cdc_write(&cdc, kCdcIndex, cdstream::kSelectResponseFifo);
  const auto value = static_cast<std::uint8_t>(cdc_read(&cdc, kCdcResponse));
  cdc_write(&cdc, kCdcIndex, static_cast<std::uint8_t>(bank));
  return value;
}

void CdStreamService::publishStatus(Core &core, std::uint8_t, const Response &) {
  // The guest's `andi $s1, $v0, 0x001D` at 0x8008C580 runs on response byte 0 BEFORE the dispatch,
  // and the trip three bytes at 0x800B3DF0 are written ONLY by the arm that handles the type --
  // each arm begins by `sb`ing into `$s2 + 0/1/2`. So the triplet is not cleared here: clearing
  // it would be a write the guest's code does not make, and an arm that writes only bytes 0 and
  // 1 must leave byte 2 as the previous response left it. That asymmetry is observable in the
  // read-state and error arms, which each write two of the three bytes and never touch the third.
}

std::uint32_t CdStreamService::runArm(Core &core, std::uint8_t type, const Response &response) {
  const std::uint32_t arm = armForResponseType(type);
  if (arm == cdstream::kUndispatchedType) {
    ++undispatched_;
    return kResultNone;
  }
  const std::uint8_t c = response.carry;

  switch (arm) {
  case cdstream::kArmDataReady: { // 0x8008C790
    // `beq $s1, $zero, 0x8008C7A4` then `bne $s0, 1, 0x8008C7A4` at 0x8008C790/0x8008C798: the
    // arm tests the carry FIRST and, when it is set, tests the drained count against 1. Both
    // paths converge on 0x8008C7A4, which is the arm's only real work, and its return value is
    // the `addiu $v0, $zero, 4` in the delay slot of the `j 0x8008C930` at 0x8008C808. So the
    // return is 4 on BOTH paths and the carry does not change it.
    core.mem_w8(cdstream::kStatusTripletAddress + 1, 1);
    core.mem_w8(cdstream::kStatusTripletAddress + 0, 0);
    copyResponse(core, cdstream::kReadyResponseBuffer, response);
    return cdstream::kResultDataReady;
  }
  case cdstream::kArmSectorBufferReady: { // 0x8008C744
    // `beq $s1,$zero, 0x8008C750` with $v0 = 2, else $v0 = 5. Writes byte 0 of the triplet and
    // returns 2 either way.
    core.mem_w8(cdstream::kStatusTripletAddress + 0, c != 0 ? 5 : 2);
    copyResponse(core, cdstream::kSyncResponseBuffer, response);
    return cdstream::kResultCommandAcknowledge;
  }
  case cdstream::kArmCommandAcknowledge: { // 0x8008C644
    // Three paths, all distinguished by the carry and a guest word the arm loads
    // (`lw $v0, 0x3BD8($at)`, indexed by the byte at 0x800B3B31):
    //   carry != 0                    -> 5, buffer at 0x800C637C
    //   carry == 0 and word != 0       -> 3, buffer at 0x800C637C
    //   carry == 0 and word == 0       -> 2, buffer at 0x800C637C
    // and the third path returns 1 in place of 2 (`addiu $v0,$zero,1` at 0x8008C6F8).
    const std::uint8_t word = core.mem_r8(cdstream::kCommandAcknowledgeWord);
    if (c != 0) {
      core.mem_w8(cdstream::kStatusTripletAddress + 0, 5);
      copyResponse(core, cdstream::kSyncResponseBuffer, response);
      return cdstream::kResultDataReady | cdstream::kResultCommandAcknowledge;
    }
    if (word != 0) {
      core.mem_w8(cdstream::kStatusTripletAddress + 0, 3);
      copyResponse(core, cdstream::kSyncResponseBuffer, response);
      return cdstream::kResultDataReady;
    }
    core.mem_w8(cdstream::kStatusTripletAddress + 0, 2);
    copyResponse(core, cdstream::kSyncResponseBuffer, response);
    return cdstream::kResultSectorBufferReady | cdstream::kResultCommandAcknowledge;
  }
  case cdstream::kArmReadState: { // 0x8008C810
    // `sb $v1, 2($v0)` with $v1 = 4, then `lbu $v1, 2($v0)` and `sb $v1, 1($v0)`. So byte 2 and
    // byte 1 both become 4, and the arm returns 4.
    core.mem_w8(cdstream::kStatusTripletAddress + 2, 4);
    core.mem_w8(cdstream::kStatusTripletAddress + 1, 4);
    copyResponse(core, cdstream::kReadStateResponseBuffer, response);
    copyResponse(core, cdstream::kReadyResponseBuffer, response);
    return cdstream::kResultDataReady;
  }
  case cdstream::kArmError: { // 0x8008C890
    // `sb $v1, 1($v0)` with $v1 = 5, then `lbu $v1, 1($v0)` and `sb $v1, 0($v0)`. Both bytes
    // become 5, and the arm returns 6 -- which is 4 | 2, so a poll loop reads an error as BOTH
    // "data ready" and "command acknowledge" and therefore calls BOTH callbacks. That is the
    // guest's own encoding and it is preserved exactly.
    core.mem_w8(cdstream::kStatusTripletAddress + 1, 5);
    core.mem_w8(cdstream::kStatusTripletAddress + 0, 5);
    copyResponse(core, cdstream::kSyncResponseBuffer, response);
    copyResponse(core, cdstream::kReadyResponseBuffer, response);
    ++errors_;
    return cdstream::kResultDataReady | cdstream::kResultCommandAcknowledge;
  }
  default:
    ++undispatched_;
    return kResultNone;
  }
}

void CdStreamService::copyResponse(Core &core, std::uint32_t destination, const Response &r) const {
  // Every arm's copy loop is the same eight bytes: `addiu $a0, $zero, 7` down to a0 = -1, reading
  // the source with `lbu` and writing with `sb`, advancing both. The guest also checks the
  // destination for zero before copying (`beq $v1, $zero, <skip>`), which is the null-pointer
  // guard on a caller-supplied buffer. Reproduced rather than assumed.
  if (destination == 0) {
    return;
  }
  for (std::uint32_t i = 0; i < cdstream::kResponseBytes; ++i) {
    core.mem_w8(destination + i, r.bytes[i]);
  }
}

void CdStreamService::report() const {
  lucent::info(
      "cdstream",
      "Spider-Man 1 recovered CD service: {} calls (of which {} with no response), responses by "
      "type 1..5 = {} {} {} {} {}, {} errors, {} unclassifiable, {} unstable type reads. Guest "
      "gate "
      "0x{:08X} = {} (the guest's own poll would skip the service)",
      services_,
      noResponse_,
      byType_[1],
      byType_[2],
      byType_[3],
      byType_[4],
      byType_[5],
      errors_,
      undispatched_,
      unstableTypeReads_,
      cdstream::kServiceGateAddress,
      gateObserved_);
}

} // namespace spider::spider1
