#include "spider1_catalog.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace spider::spider1 {
namespace {

constexpr std::array<psx::host::TitleIdentity, 1> kIdentities{{
    psx::host::TitleIdentity{
        .displayName = SPIDER_TITLE_LABEL,
        .serial = SPIDER_TITLE_SERIAL,
        .slug = SPIDER_TITLE_ID,
        .fileSize = SPIDER_TITLE_EXECUTABLE_SIZE,
        .sha256 = SPIDER_TITLE_EXECUTABLE_SHA256,
        .entry = SPIDER_TITLE_HEADER_ENTRY,
        .globalPointer = SPIDER_TITLE_HEADER_GP,
        .textAddress = SPIDER_TITLE_HEADER_TEXT_ADDRESS,
        .textSize = SPIDER_TITLE_HEADER_TEXT_SIZE,
        .stackAddress = SPIDER_TITLE_HEADER_STACK_ADDRESS,
        .stackOffset = SPIDER_TITLE_HEADER_STACK_OFFSET,
    },
}};

} // namespace

std::string_view Spider1Catalog::productName() const {
  return "Spider-Man";
}

std::span<const psx::host::TitleIdentity> Spider1Catalog::titles() const {
  return kIdentities;
}

Spider1Runtime &Spider1Catalog::runtime(std::size_t index) const {
  if (index >= kIdentities.size()) {
    lucent::error("spider1-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
  return runtime_;
}

} // namespace spider::spider1
