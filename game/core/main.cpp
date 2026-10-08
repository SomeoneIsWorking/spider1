#include "spider1_runtime.h"
#include "spider_port.h"

int main(int argc, char **argv) {
  spider::spider1::Spider1Runtime runtime;
  return spider::runPort(runtime, argc, argv);
}
