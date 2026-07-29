// cd_stream.cpp — drive a continuous CD read (XA / STR) from the point the guest asks for data.
//
// A stream is not a file read. A file read is finite and the port serves it in one call; a stream
// runs until the game stops it and expects sectors delivered as it consumes them.
//
// WHY HERE, measured rather than assumed — two other homes were tried and rejected:
//   * The port's field clock (vblank advance). Wrong: over a boot that reaches the movie the guest
//     makes only 13 VSync calls in total and stops calling it entirely once inside the streaming
//     loop, so a pump hung there never fires when it is needed.
//   * Nowhere at all, relying on the file-read burst. That burst drives a FINITE read to completion
//     and a stream has no end for it to reach; it ran away to its 65536-sector bound and wedged the
//     boot. It is now correctly restricted to games that have not taken over CdRead.
//
// The streaming loop DOES call StGetNext every iteration, which makes this the honest place: supply
// a sector at the moment the guest asks for one and finds none ready.
#include "core.h"
#include "game.h"
#include "cfg.h"
#include "override_registry.h"

// StGetNext(&addr, &header) — 0x80086B10. Returns 0 when it hands back a ready ring slot, non-zero
// when none is ready. Read from its body: it takes the ring slot at DAT_800c1510 + index*0x20 when
// the slot's status short is 2, and marks it 4.
extern void gen_func_80086B10(Core*);

static void spiderman_stgetnext(Core* c) {
  gen_func_80086B10(c);
  // Non-zero means "no sector ready" — exactly when the drive owes the guest one. Pump a sector and
  // answer again. EXACTLY ONE retry: if the stream still has nothing after being pumped, "not ready"
  // is the honest answer and the guest is entitled to keep spinning. Looping here until something
  // appeared would mask a genuinely dry stream and hang inside an override instead of in the game.
  if (c->r[2] != 0) {
    c->game->cd.pumpStream(c, 1);
    gen_func_80086B10(c);
  }
}

void spiderman_install_cd_stream(Game* g) {
  engine_set_override_main(0x80086B10u, spiderman_stgetnext, gen_func_80086B10);
  cfg_logi("cd", "continuous-read pump installed on StGetNext (0x80086B10)");
  (void)g;
}
