// The host catalog is built from title.json; it must agree with the runtime and, when the
// player's executable is provisioned, with the PS-X EXE header.

#include "spider1_catalog.h"

#include "testutil.h"
#include "title_selection.h"

#include <filesystem>
#include <string_view>

namespace {

void test_the_catalog_lists_spider_man_1_only() {
  const spider::spider1::Spider1Catalog catalog;
  CHECK_EQ(catalog.titles().size(), 1u);
  CHECK(catalog.titles().front().slug == "spiderman1");
  CHECK(catalog.titles().front().serial == "SLUS_008.75");
}

void test_the_catalog_and_the_runtime_share_one_identity() {
  const spider::spider1::Spider1Catalog catalog;
  const psx::host::TitleIdentity &identity = catalog.titles().front();
  const spider::ExecutableIdentity &runtime = catalog.runtime(0).executableIdentity();
  CHECK(identity.serial == runtime.serial);
  CHECK_EQ(identity.fileSize, runtime.fileSize);
  CHECK(identity.sha256 == runtime.sha256);
  CHECK_STREQ(catalog.runtime(0).discEnvVar(), "PSXPORT_SPIDERMAN_DISC");
}

void test_a_provisioned_executable_passes_the_hosts_selection() {
  const spider::spider1::Spider1Catalog catalog;
  const std::filesystem::path executable = SPIDER_TITLE_GUEST_EXE;
  if (!std::filesystem::exists(executable)) {
    return;
  }
  const psx::host::SelectionResult selection =
      psx::host::selectExecutableFile(executable, catalog.titles());
  CHECK_MSG(static_cast<bool>(selection), selection.detail.c_str());
}

} // namespace

int main() {
  RUN(the_catalog_lists_spider_man_1_only);
  RUN(the_catalog_and_the_runtime_share_one_identity);
  RUN(a_provisioned_executable_passes_the_hosts_selection);
  return pt_summary();
}
