#include "spider_context.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spider {

SpiderContext &context(Core &core) {
  if (!core.gameCtx) {
    lucent::error("spider", "SpiderRuntime context is absent");
    std::abort();
  }
  return *static_cast<SpiderContext *>(core.gameCtx);
}

} // namespace spider
