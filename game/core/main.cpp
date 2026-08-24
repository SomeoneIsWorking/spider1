// main.cpp — the Spider-Man port's process entry point.
//
// Spider-Man's target-only composition entry point. Shared process boot lives in spider_port.cpp.
//
// Phase 0 (see docs/re-frontier.md): every guest function runs on the recompiled substrate. The
// native boot reproduces crt0 and then SpiderRuntime dispatches the guest's own main().
#include "spider1_runtime.h"
#include "spider_port.h"

int main(int argc, char **argv) {
  static spider::Spider1Runtime runtime;
  return spider::runPort(runtime, argc, argv);
}
