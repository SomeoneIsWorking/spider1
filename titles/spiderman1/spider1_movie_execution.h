#pragma once

#include "execution_exit.h"

class Core;

namespace spider::spider1 {

// Resumes the retail STR player through the runtime executor; field/service exits are
// `ExecutionResult`s.
class Spider1MovieExecution {
public:
  psx::cpu::ExecutionResult resume(Core &core) const;
};

} // namespace spider::spider1
