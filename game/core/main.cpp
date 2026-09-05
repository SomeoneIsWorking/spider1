// main.cpp — the Spider-Man port's process entry point.
//
// Spider-Man's target-only composition entry point. Shared process boot lives in spider_port.cpp.
//
// The shared process owner authenticates the original executable and enters its crt0 through
// psxport's per-Core runtime executor.
#include "spider1_runtime.h"
#include "spider_port.h"

int main(int argc, char **argv) {
  static spider::Spider1Runtime runtime;
  return spider::runPort(runtime, argc, argv);
}
