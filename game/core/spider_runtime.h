#pragma once

#include "executable_identity.h"
#include "execution_exit.h"
#include "game_runtime.h"

#include <string_view>

namespace spider {

// Process-lifetime seam for the two Neversoft Spider titles; owns no guest address.
class SpiderRuntime : public GameRuntime {
public:
  std::string_view serial() const;
  virtual std::string_view discEnvironment() const = 0;
  virtual std::string_view defaultExecutable() const = 0;
  virtual const ExecutableIdentity &executableIdentity() const = 0;
  virtual void prepareBootstrap(Game &game);
  virtual bool resumeBootstrapBoundary(Core &core, const psx::cpu::ExecutionResult &result);

protected:
  [[noreturn]] void refuseUnported(std::string_view boundary, std::string_view frontier) const;
};

} // namespace spider
