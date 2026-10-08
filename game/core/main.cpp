#include "product_host.h"
#include "spider1_catalog.h"

#include <lucent/log.h>

#include <cstring>

namespace {

constexpr const char *kProvisioningRoot = "scratch/assets";

bool helpRequested(int argc, char **argv) {
  return argc == 2 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0);
}

void printUsage(const char *program) {
  lucent::info(
      "cli",
      "Usage: {} [executable]\n"
      "With no argument: the title selector, then the chosen Spider-Man title, in this process.\n"
      "With an executable: run exactly that serial-identified executable (maintainer override).\n"
      "Options: -h, --help  Show this help and exit.",
      program);
}

} // namespace

int main(int argc, char **argv) {
  if (helpRequested(argc, argv)) {
    printUsage(argv[0]);
    return 0;
  }
  const spider::spider1::Spider1Catalog catalog;
  psx::host::ProductHost host(catalog, kProvisioningRoot);
  return argc > 1 ? host.runExecutable(argv[1]) : host.runSelector();
}
