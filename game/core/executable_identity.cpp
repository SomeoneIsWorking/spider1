#include "executable_identity.h"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

namespace spider {
namespace {

ExecutableIdentityResult mismatch(const ExecutableIdentity &expected, std::string detail) {
  return {ExecutableIdentityStatus::IdentityMismatch,
          std::string(expected.serial) + " identity mismatch: " + std::move(detail)};
}

std::string sha256(std::span<const std::uint8_t> bytes) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digestSize = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &digestSize, EVP_sha256(), nullptr) !=
          1 ||
      digestSize != 32u) {
    return {};
  }

  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (unsigned int index = 0; index < digestSize; ++index) {
    stream << std::setw(2) << static_cast<unsigned int>(digest[index]);
  }
  return stream.str();
}

} // namespace

ExecutableIdentityResult verifyExecutable(std::string_view serial,
                                          std::span<const std::uint8_t> bytes,
                                          const ExecutableIdentity &expected) {
  if (serial != expected.serial) {
    return {ExecutableIdentityStatus::UnsupportedSerial,
            "target serial " + std::string(expected.serial) + " refuses executable serial " +
                std::string(serial)};
  }
  if (bytes.size() != expected.fileSize) {
    return mismatch(expected,
                    "expected " + std::to_string(expected.fileSize) + " bytes, got " +
                        std::to_string(bytes.size()));
  }

  constexpr std::array<std::uint8_t, 8> kPsxExeMagic{'P', 'S', '-', 'X', ' ', 'E', 'X', 'E'};
  if (bytes.size() < 0x800u || !std::ranges::equal(kPsxExeMagic, bytes.first(8))) {
    return {ExecutableIdentityStatus::InvalidExecutable,
            std::string(expected.serial) + " is not a PS-X EXE"};
  }

  const std::string digest = sha256(bytes);
  if (digest.empty()) {
    return {ExecutableIdentityStatus::InvalidExecutable,
            std::string(expected.serial) + " SHA-256 calculation failed"};
  }
  if (digest != expected.sha256) {
    return mismatch(expected,
                    "SHA-256 expected " + std::string(expected.sha256) + ", got " + digest);
  }
  return {ExecutableIdentityStatus::Verified,
          std::string(expected.serial) + " executable identity verified (SHA-256 " + digest + ")"};
}

ExecutableIdentityResult verifyExecutableFile(const std::filesystem::path &path,
                                              const ExecutableIdentity &expected) {
  const std::string serial = path.filename().string();
  if (serial != expected.serial) {
    return {ExecutableIdentityStatus::UnsupportedSerial,
            "target serial " + std::string(expected.serial) + " refuses executable serial " +
                serial};
  }

  std::error_code sizeError;
  const std::uintmax_t size = std::filesystem::file_size(path, sizeError);
  if (sizeError) {
    return {ExecutableIdentityStatus::MissingExecutable,
            "cannot read " + path.string() + ": " + sizeError.message()};
  }
  if (size != expected.fileSize) {
    return mismatch(expected,
                    "expected " + std::to_string(expected.fileSize) + " bytes, got " +
                        std::to_string(size));
  }

  std::vector<std::uint8_t> bytes(expected.fileSize);
  std::ifstream input(path, std::ios::binary);
  if (!input.read(reinterpret_cast<char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()))) {
    return {ExecutableIdentityStatus::MissingExecutable, "cannot read " + path.string()};
  }
  return verifyExecutable(serial, bytes, expected);
}

} // namespace spider
