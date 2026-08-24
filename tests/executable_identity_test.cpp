#include "executable_identity.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

void writeU32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned byte = 0; byte < 4; ++byte) {
    bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
  }
}

std::vector<std::uint8_t> fixture() {
  std::vector<std::uint8_t> bytes(0x800u);
  constexpr std::array<std::uint8_t, 8> kMagic{'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'};
  std::ranges::copy(kMagic, bytes.begin());
  writeU32(bytes, 0x10u, 0x80010100u);
  writeU32(bytes, 0x18u, 0x80010000u);
  writeU32(bytes, 0x1Cu, 0x1000u);
  writeU32(bytes, 0x30u, 0x801FFFF0u);
  return bytes;
}

} // namespace

int main() {
  const spider::ExecutableIdentity expected{
      .serial = "SLUS_000.01",
      .fileSize = 0x800u,
      .sha256 = "02e23f3624a575943098a80ec71a271d1d192128907fd3bd464a2f54b523239a",
  };

  std::vector<std::uint8_t> bytes = fixture();
  if (!spider::verifyExecutable(expected.serial, bytes, expected)) {
    return 1;
  }
  if (spider::verifyExecutable("SLUS_000.02", bytes, expected).status !=
      spider::ExecutableIdentityStatus::UnsupportedSerial) {
    return 2;
  }
  bytes.back() = 1u;
  if (spider::verifyExecutable(expected.serial, bytes, expected).status !=
      spider::ExecutableIdentityStatus::IdentityMismatch) {
    return 3;
  }
  bytes = fixture();
  bytes.front() = 0u;
  return spider::verifyExecutable(expected.serial, bytes, expected).status ==
                 spider::ExecutableIdentityStatus::InvalidExecutable
             ? 0
             : 4;
}
