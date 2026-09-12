#include "spider_runtime.h"

#include <lucent/log.h>

#include <cstdlib>

namespace spider {

std::string_view SpiderRuntime::serial() const {
  return executableIdentity().serial;
}

void SpiderRuntime::prepareBootstrap(Game &) {}

bool SpiderRuntime::resumeBootstrapBoundary(Core &, const psx::cpu::ExecutionResult &) {
  return false;
}

[[noreturn]] void SpiderRuntime::refuseUnported(std::string_view boundary,
                                                std::string_view frontier) const {
  lucent::error("boot",
                "title serial {} reached unported boundary '{}'; work {} before continuing. "
                "No other title's runtime facts were substituted.",
                serial(),
                boundary,
                frontier);
  std::abort();
}

} // namespace spider
