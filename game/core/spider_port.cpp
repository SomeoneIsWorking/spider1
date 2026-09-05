#include "spider_port.h"

#include "core.h"
#include "executable_identity.h"
#include "game.h"
#include "guest_execution.h"
#include "spider_runtime.h"

#include <cstdio>
#include <lucent/log.h>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
void mdec_init(void);
void spu_init(void);
void watchdog_init(void);
}

void gte_init(void);
void load_exe(const char *path, Core *core);

namespace spider {

namespace {

bool isHelpArgument(int argc, char **argv) {
  return argc == 2 && argv[1] &&
         (std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help");
}

void printUsage(const char *program, const SpiderRuntime &runtime) {
  std::printf("Usage: %s [guest-executable]\n", program && *program ? program : "spiderman_port");
  std::printf("Run the %.*s native port with an authenticated extracted PS-X executable.\n",
              static_cast<int>(runtime.serial().size()),
              runtime.serial().data());
  std::printf("With no argument, the executable defaults to %.*s.\n",
              static_cast<int>(runtime.defaultExecutable().size()),
              runtime.defaultExecutable().data());
  std::printf("Options:\n  -h, --help  Show this help and exit.\n");
}

} // namespace

int runPort(SpiderRuntime &runtime, int argc, char **argv) {
  // Help is process metadata, not a game launch. It must remain available in a fresh checkout
  // before executable extraction, identity verification, disc discovery, or framework setup.
  if (isHelpArgument(argc, argv)) {
    printUsage(argv[0], runtime);
    return 0;
  }
  const std::string path = argc > 1 ? argv[1] : std::string(runtime.defaultExecutable());
  const ExecutableIdentityResult identity =
      verifyExecutableFile(path, runtime.executableIdentity());
  if (!identity) {
    lucent::error("boot", "{}", identity.detail);
    return 2;
  }
  lucent::info("boot", "{}", identity.detail);

  // Core snapshots the process-lifetime title runtime. Serial and byte identity are both verified
  // above, before Game construction, so a renamed cache cannot borrow another title's runtime
  // facts.
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  game->disc.env_key = runtime.discEnvironment().data();
  Core *core = &game->core;

  watchdog_init();
  load_exe(path.c_str(), core);
  gte_init();
  mdec_init();
  spu_init();
  game->spu_audio.init();
  game->gpu.gpu_native_init();
  game->pad.overridesInit();
  core->runtime->registerOverrides(*game);
  const GuestProgramImage *program = runtime.guestProgramImage();
  if (!program || !program->crt0Entry) {
    lucent::error("executor", "{} has no authenticated runtime entry", runtime.serial());
    return 3;
  }
  GuestExecution execution(*core);
  return reportExecutionResult(execution.enter(program->crt0Entry), runtime.serial()) ? 0 : 3;
}

} // namespace spider
