#pragma once

#include "allocator_audit.h"

class Core;

namespace spider {

struct SpiderContext {
  AllocatorAudit allocatorAudit;
};

SpiderContext &context(Core &core);

} // namespace spider
