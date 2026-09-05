#pragma once

#include "execution_exit.h"

class Core;

namespace spider {

// Resumes the unchanged retail STR player through the runtime executor. The old build-derived
// movie body is deliberately absent; field/service exits are represented by ExecutionResult.
class Spider1MovieExecution {
public:
  psx::cpu::ExecutionResult resume(Core &core) const;
};

} // namespace spider
