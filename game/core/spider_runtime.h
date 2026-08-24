#pragma once

#include "executable_identity.h"
#include "game_runtime.h"

#include <string_view>

namespace spider {

// Shared process-lifetime seam for the two Neversoft Spider titles. Executable facts and behavior
// remain virtual title policy: this base deliberately owns no guest address and no generated thunk.
class SpiderRuntime : public GameRuntime {
public:
  std::string_view serial() const;
  virtual std::string_view discEnvironment() const = 0;
  virtual std::string_view defaultExecutable() const = 0;
  virtual const ExecutableIdentity &executableIdentity() const = 0;
  virtual void installRecomp() = 0;

protected:
  [[noreturn]] void refuseUnported(std::string_view boundary, std::string_view frontier) const;
};

} // namespace spider
