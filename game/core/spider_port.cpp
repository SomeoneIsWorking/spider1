#include "spider_port.h"

#include "cfg.h"
#include "core.h"
#include "executable_identity.h"
#include "game.h"
#include "spider_runtime.h"

#include <cstdio>
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
void native_boot_run(Core *core);
int selftest_run(const char *path);

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
  // above, before either this install or Game construction, so a renamed cache cannot borrow the
  // other title's generated substrate, guest addresses, or compatibility views.
  psxport_install_game(runtime);
  runtime.installRecomp();

  auto game = std::make_unique<Game>();
  game->disc.env_key = runtime.discEnvironment().data();
  Core *core = &game->core;

  const char *selftest = cfg_str("PSXPORT_SELFTEST");
  if (selftest && *selftest) {
    return selftest_run(path.c_str());
  }

  watchdog_init();
  load_exe(path.c_str(), core);
  gte_init();
  mdec_init();
  spu_init();
  game->spu_audio.init();
  game->gpu.gpu_native_init();
  game->pad.overridesInit();
  core->r[4] = 1;
  core->r[5] = 0;

  core->runtime->registerOverrides(*game);
  native_boot_run(core);
  lucent::info("boot", "native boot returned");
  return 0;
}

} // namespace spider
