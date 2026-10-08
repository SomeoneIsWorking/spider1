#include "spider_port.h"
#include "cfg.h"
#include "dbg_server.h"
#include "field_turn.h"
#include "hw_bind.h"
#include "psx_exe_image.h"
#include "store_observe.h"

#include "core.h"
#include "executable_identity.h"
#include "game.h"
#include "guest_execution.h"
#include "render_mode.h"
#include "spider_runtime.h"

#include <lucent/log.h>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
void mdec_init(void);
void spu_init(void);
void watchdog_init(void);
}

namespace spider {

namespace {

bool isHelpArgument(int argc, char **argv) {
  return argc == 2 && argv[1] &&
         (std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help");
}

void printUsage(const char *program, const SpiderRuntime &runtime) {
  lucent::info(
      "boot", "Usage: {} [guest-executable]", program && *program ? program : "spiderman_port");
  lucent::info("boot",
               "Run the {} native port with an authenticated extracted PS-X executable.",
               runtime.serial());
  lucent::info(
      "boot", "With no argument, the executable defaults to {}.", runtime.defaultExecutable());
  lucent::info("boot", "Options: -h, --help  Show this help and exit.");
}

} // namespace

int runPort(SpiderRuntime &runtime, int argc, char **argv) {
  // Help must work in a fresh checkout, before extraction, identity checks or framework setup.
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

  // Core snapshots the runtime; identity is verified before Game construction.
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
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
  render_path_install(core);
  const GuestProgramImage *program = runtime.guestProgramImage();
  if (!program || !program->crt0Entry) {
    lucent::error("executor", "{} has no authenticated runtime entry", runtime.serial());
    return 3;
  }
  GuestExecution execution(*core);

  // This spine skips native_boot's loop, so the debug endpoint and store observation attach here.
  const int frameCap = game->dbg_server.attach(core, cfg_int("PSXPORT_NATIVE_FRAMES", 0));
  store_observe_attach(*core);
  lucent::info("boot",
               "{} boot spine owns its frame loop: live endpoint {} (frame cap {})",
               runtime.serial(),
               frameCap > 0 ? std::to_string(frameCap) : "uncapped",
               frameCap);

  const psx::frame::FieldTurn fieldTurn;
  auto result = execution.enter(program->crt0Entry);
  for (;;) {
    fieldTurn.beginField(*core);
    if (result.reason != psx::cpu::ExecutionExitReason::BudgetExhausted || result.cycles == 0) {
      break;
    }
    // The turn limit bounds one call, not the program; resume the saved PC.
    result = execution.resumeAt(result.guestPc);
    fieldTurn.endField(*core, 0);
  }
  return reportExecutionResult(*core, result, runtime.serial()) ? 0 : 3;
}

} // namespace spider
