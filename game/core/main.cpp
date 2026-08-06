// main.cpp — the Spider-Man port's process entry point.
//
// Installs the game seam (GameConfig + GameHooks + RecompRegistry), brings up the framework's PSX
// hardware backends, loads the retail executable, and enters the native boot. After the install
// nothing here names anything but framework symbols.
//
// Phase 0 (see docs/re-frontier.md): every guest function runs on the recompiled substrate. The
// native boot reproduces crt0 and then the bootInit hook dispatches the guest's own main().
#include "core.h"
#include "game.h"
#include "cfg.h"
#include "fs_util.h"
#include "disc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
void watchdog_init(void);
void mdec_init(void);
void spu_init(void);
}

void load_exe(const char* path, Core* c);      // runtime/recomp/boot.cpp (framework)
void native_boot_run(Core* c);                 // runtime/recomp/native_boot.cpp (framework)
void gte_init(void);
int  selftest_run(const char* path);           // runtime/recomp/selftest.cpp (framework harness)

extern void spiderman_install_game_config();      // game/core/game_config.cpp
extern void spiderman_install_recomp();           // game/core/recomp_register.cpp
extern void spiderman_install_sync_natives(Game*);// game/core/sync_native.cpp
extern void spiderman_install_render_seam(Game*); // game/render/render_seam.cpp

// The retail US executable, as it is named on the disc. There is no separate PSX boot stub to run:
// SYSTEM.CNF boots SLUS_008.75 directly (BOOT=cdrom:\SLUS_008.75;1), unlike Tomba!2's
// SCUS_944.54 -> MAIN.EXE hand-off. So this port skips BootStub entirely and boots the one image.
static const char* kDefaultExe = "scratch/bin/spiderman/SLUS_008.75";
static const char* kDiscExePath = "\\SLUS_008.75";

int main(int argc, char** argv) {
  // Must precede the first Core: Core's ctor snapshots psxport_game_config()/psxport_game_hooks().
  spiderman_install_game_config();
  spiderman_install_recomp();

  const char* path = argc > 1 ? argv[1] : kDefaultExe;

  Game* game = new Game();
  Core* c = &game->core;

  // Self-provision the executable so the binary is runnable straight from a disc image, with no
  // prior ./run.sh step (disc resolution: PSXPORT_SPIDERMAN_DISC, .env, or a *.chd in the cwd).
  if (!Fs::exists(path)) {
    cfg_logw("boot", "%s missing — extracting from disc", path);
    if (!disc_extract_file(&game->disc, kDiscExePath, path)) {
      cfg_loge("boot", "extraction failed: provide a disc (PSXPORT_SPIDERMAN_DISC, .env, "
                       "or a *.chd in the working directory) or run ./run.sh");
      return 1;
    }
  }

  // PSXPORT_SELFTEST=<name>: run the framework's headless selftest harness instead of booting
  // (framework names like mdecpump, plus any game-defined ones via GameHooks::selftestGame).
  // The exit code is the verdict — this is how the MDEC pending-DMA regression runs in CI/dev.
  {
    const char* st = cfg_str("PSXPORT_SELFTEST");
    if (st && *st) return selftest_run(path);
  }

  watchdog_init();            // PSXPORT_WATCHDOG=<sec>: abort + backtrace if a frame stalls
  load_exe(path, c);

  gte_init();                 // GTE (COP2)
  mdec_init();                // MDEC (FMV)
  spu_init();                 // SPU
  game->spu_audio.init();     // SDL audio sink (PSXPORT_NOAUDIO to disable)
  game->gpu.gpu_native_init();// native GPU renderer over the guest's GP0 stream
  game->cd.overridesInit();   // native CD: drive-ready + by-LBA read
  // Hardware-sync HLE, in two halves. initBuiltins() installs the framework's generic handlers at
  // whatever addresses THIS game declares in GameConfig::hle — as of psxport 7c212eb5 it ships none
  // of its own, so it can no longer install another game's addresses over ours. Today that group is
  // all zero (nothing but VSync has been RE'd), so this registers nothing and says so.
  game->platform_hle.initBuiltins();
  // Then this game's own faithfully reimplemented primitives, which have no generic form.
  spiderman_install_sync_natives(game);
  game->pad.overridesInit();  // native controller input
  // The render seam on the engine's own submitFrame (guest FUN_80061308). MUST precede
  // native_boot_run(), which is where PSXPORT_RENDER_PSX is read: the seam sets this port's default
  // leg and an explicit env value has to be able to win over it.
  spiderman_install_render_seam(game);
  c->r[4] = 1; c->r[5] = 0;   // a0/a1 as the BIOS leaves them

  c->hooks->registerOverrides(game);   // Phase 0: no clusters yet, but keep the wiring honest
  native_boot_run(c);
  cfg_logi("boot", "native boot returned");
  return 0;
}
