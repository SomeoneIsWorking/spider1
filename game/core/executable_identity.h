#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace spider {

struct ExecutableIdentity {
  std::string_view serial;
  std::size_t fileSize;
  std::string_view sha256;
};

enum class ExecutableIdentityStatus : std::uint8_t {
  Verified,
  UnsupportedSerial,
  MissingExecutable,
  InvalidExecutable,
  IdentityMismatch,
};

struct ExecutableIdentityResult {
  ExecutableIdentityStatus status;
  std::string detail;

  explicit operator bool() const {
    return status == ExecutableIdentityStatus::Verified;
  }
};

ExecutableIdentityResult verifyExecutable(std::string_view serial,
                                          std::span<const std::uint8_t> bytes,
                                          const ExecutableIdentity &expected);
ExecutableIdentityResult verifyExecutableFile(const std::filesystem::path &path,
                                              const ExecutableIdentity &expected);

} // namespace spider
