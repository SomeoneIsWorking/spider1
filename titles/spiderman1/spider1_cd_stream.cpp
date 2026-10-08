// The recovered CD-stream service as a native owner of 0x8008C3E0; it never writes the gate word.
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

// Epilogue `v0` for an unclassifiable response: the guest default arm 0x8008C910 prints
// "CDROM: unknown intr" and returns zero at 0x8008C92C.
constexpr std::uint32_t kResultNone = 0u;

// Bound on type re-reads; the guest loop (`bne` at 0x8008C434) has none, but native code must not
// hang. Hitting it is reported.
constexpr std::uint32_t kMaxTypeReads = 8u;

// CD-ROM registers, reached by the guest through its table at 0x800B3DD8.
constexpr std::uint32_t kCdcIndex = 0u;    // 0x1F801800, selected by the register's low two bits
constexpr std::uint32_t kCdcResponse = 1u; // 0x1F801801, bank 1: the response FIFO
constexpr std::uint32_t kCdcIrqFlag = 3u;  // 0x1F801803, bank 1: type in the low three bits

} // namespace

void CdStreamService::install(Core &core) {
  // Sample the gate here; `gateObserved_` is re-read on every call.
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
  // The mask returns in `v0` and the poll loop resumes after its own `jal`.
  auto &runtime = Spider1Runtime::from(*core);
  core->r[2] = runtime.cdStream().service(*core);
}

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

// The five-arm dispatch table at 0x80096670, indexed by `type-1`.
std::uint32_t CdStreamService::armForResponseType(std::uint8_t type) const noexcept {
  for (const auto &arm : cdstream::kDispatchTable) {
    if (arm.responseType == type) {
      return arm.address;
    }
  }
  return cdstream::kUndispatchedType;
}

std::uint32_t CdStreamService::gateObserved(Core &core) const {
  // The gate is a u16 at 0x800B2886 (2 mod 4), so it is read as two bytes.
  const auto lo = core.mem_r8(cdstream::kServiceGateAddress);
  const auto hi = core.mem_r8(cdstream::kServiceGateAddress + 1);
  return static_cast<std::uint32_t>(lo) | (static_cast<std::uint32_t>(hi) << 8);
}

// Guest 0x8008C3E0 in order: select the response FIFO (0x8008C3FC), mask the type with 7
// (0x8008C414), re-read it until stable (0x8008C42C), drain at most eight bytes (0x8008C494).
std::uint32_t CdStreamService::service(Core &core) {
  auto &cdc = core.game->cdc;
  ++services_;
  // Re-read the gate on every call so the report is the guest's latest value.
  gateObserved_ = gateObserved(core);

  // Step 1: select the response FIFO (the framework's `kCdcIndex`).
  core.mem_w8(cdstream::kIndexRegister, cdstream::kSelectResponseFifo);

  // Step 2: read the type twice and act only if stable; bounded, see kMaxTypeReads.
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

  // Step 3: type 0 returns (`beq` to 0x8008C92C).
  if (!typeIsDispatchable(type)) {
    ++noResponse_;
    return kResultNone;
  }

  // Step 4: drain at most eight bytes, zero-filling the tail.
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

  // Step 5: publish the status, then run the arm.
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
  // The guest tests status bit 5 (RSLRRDY): a response byte is available.
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
  // The `andi $s1` at 0x8008C580 runs before dispatch. The triplet at 0x800B3DF0 is written only by
  // the arm (`sb` to `$s2 + 0/1/2`), so it is not cleared here: an arm that writes two bytes leaves
  // the third as the previous response left it.
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
    // Tests the carry, then the drained count against 1 (0x8008C790/0x8008C798); both paths reach
    // 0x8008C7A4 and return 4 (delay slot at 0x8008C808).
    core.mem_w8(cdstream::kStatusTripletAddress + 1, 1);
    core.mem_w8(cdstream::kStatusTripletAddress + 0, 0);
    copyResponse(core, cdstream::kReadyResponseBuffer, response);
    return cdstream::kResultDataReady;
  }
  case cdstream::kArmSectorBufferReady: { // 0x8008C744
    // $v0 = 2 if the carry is zero (0x8008C750), else 5; writes byte 0 and returns 2.
    core.mem_w8(cdstream::kStatusTripletAddress + 0, c != 0 ? 5 : 2);
    copyResponse(core, cdstream::kSyncResponseBuffer, response);
    return cdstream::kResultCommandAcknowledge;
  }
  case cdstream::kArmCommandAcknowledge: { // 0x8008C644
    // Carry non-zero gives 5; otherwise the word at 0x800B3BD8 indexed by the byte at 0x800B3B31
    // selects 3 or 2, all with buffer 0x800C637C; the last path returns 1 (0x8008C6F8).
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
    // Bytes 2 and 1 both become 4; returns 4.
    core.mem_w8(cdstream::kStatusTripletAddress + 2, 4);
    core.mem_w8(cdstream::kStatusTripletAddress + 1, 4);
    copyResponse(core, cdstream::kReadStateResponseBuffer, response);
    copyResponse(core, cdstream::kReadyResponseBuffer, response);
    return cdstream::kResultDataReady;
  }
  case cdstream::kArmError: { // 0x8008C890
    // Bytes 1 and 0 both become 5; returns 6 = 4|2, so a poll loop calls both callbacks, as the
    // guest does.
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
  // Each arm copies eight bytes (`addiu $a0, $zero, 7` down to -1); the guest skips a null
  // destination.
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
