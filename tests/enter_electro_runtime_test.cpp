#include "enter_electro_runtime.h"

#include "game.h"
#include "game_iface.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>

void enter_electro_install_recomp() {}

namespace {

[[noreturn]] void exitOnAbort(int) {
  std::_Exit(97);
}

bool refusesUnverifiedPicturePolicy(const Game &game) {
  const pid_t child = fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    std::signal(SIGABRT, exitOnAbort);
    (void)game_guest_vram_is_picture(game);
    std::_Exit(0);
  }

  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 97;
}

} // namespace

int main() {
  static_assert(std::is_base_of_v<GameRuntime, spider::SpiderRuntime>);
  static_assert(std::is_base_of_v<spider::SpiderRuntime, spider::EnterElectroRuntime>);
  static_assert(!std::is_base_of_v<LegacyGameRuntimeAdapter, spider::EnterElectroRuntime>);

  spider::EnterElectroRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  const GuestProgramImage *image = core.guestProgramImage;
  if (core.runtime != &runtime || core.cfg != nullptr || core.hooks != nullptr ||
      core.gameCtx != nullptr || !image) {
    std::fprintf(stderr, "Enter Electro borrowed a legacy title seam\n");
    return 1;
  }
  if (runtime.serial() != "SLUS_013.78" || runtime.executableIdentity().fileSize != 786432u ||
      runtime.executableIdentity().sha256 !=
          "dbe6c3f32337fe0fa7085519c728a75abf5d007b45ea0ba58178bcf84b72908a" ||
      image->bss.begin != 0x800C2AF4u || image->bss.end != 0x800CF0DCu ||
      image->stackTopWordAddress != 0x800C0D10u || image->stackReserveWordAddress != 0x800C0D0Cu ||
      image->heapBase != 0x800CF0DCu || image->heapSizeStoreAddress != 0x800BED00u ||
      image->heapBaseStoreAddress != 0x800BECFCu || image->globalPointer != 0x800C1764u ||
      image->libcInitEntry != 0x800988B0u || image->gameMainEntry != 0x80031F54u ||
      image->crt0Entry != 0x80093C68u || image->residentText.begin != 0x00010000u ||
      image->residentText.end != 0x000C2AF4u || !image->stackBias.declared ||
      image->stackBias.bytes != -8 || !refusesUnverifiedPicturePolicy(*game)) {
    std::fprintf(stderr, "Enter Electro executable facts differ from the measured crt0 group\n");
    return 1;
  }

  std::puts("Enter Electro: direct derived runtime, 8/8 crt0 facts, no legacy title seam");
  return 0;
}
