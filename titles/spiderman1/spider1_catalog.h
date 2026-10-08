#pragma once

#include "spider1_runtime.h"
#include "title_catalog.h"

#include <span>

namespace spider::spider1 {

// Spider-Man 1 as the multi-title host's catalog. Spider-Man 2 joins it when its first game-owned
// call is ported (EE-02).
class Spider1Catalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  Spider1Runtime &runtime(std::size_t index) const override;

private:
  // The host asks for a mutable runtime from a const catalog; the runtime is the process's one.
  mutable Spider1Runtime runtime_;
};

} // namespace spider::spider1
