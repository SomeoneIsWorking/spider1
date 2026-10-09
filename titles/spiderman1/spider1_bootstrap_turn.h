#pragma once

#include "execution_exit.h"
#include "guest_execution.h"

class Core;
class Game;

namespace spider::spider1 {

// The retail program run as host turns: the guest executes until a measured boundary, the
// boundary is serviced as one display field, and the guest resumes at the guest's own PC.
class Spider1BootstrapTurn {
public:
  explicit Spider1BootstrapTurn(Game &game);

  // Enter retail main once crt0 is applied; runs to the first boundary.
  void begin(Core &core);
  // Service the pending boundary (one field, one presentation fence), then run to the next.
  // A turn that ended on the cycle budget instead delivers the display field that time covered,
  // then resumes.
  void step(Core &core);

private:
  Game &game_;
  GuestExecution execution_;
  psx::cpu::ExecutionResult result_;
};

} // namespace spider::spider1
