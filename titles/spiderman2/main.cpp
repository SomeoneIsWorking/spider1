#include "enter_electro_runtime.h"
#include "spider_port.h"

int main(int argc, char **argv) {
  static spider::EnterElectroRuntime runtime;
  return spider::runPort(runtime, argc, argv);
}
