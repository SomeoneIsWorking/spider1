// game_hooks.cpp — the Spider-Man GameHooks vtable: the behaviour the PSX-generic framework calls
// into. At Phase 0 this port owns NO game behaviour natively — every guest function still runs on the
// recompiled substrate — so this table is deliberately small.
//
// There are exactly two kinds of member here, and the distinction is the point:
//
//   NEUTRAL   — the hook asks "what does the GAME's native code contribute here?", and the honest
//               Phase-0 answer is "nothing, because nothing is owned yet". A neutral body is the
//               CORRECT semantic, not a placeholder. (registerOverrides: no clusters exist.
//               renderFadeState: mode 0 is the framework's own "no fade applied" — see gpu_vk.cpp
//               dump_to, which only alters pixels for mode 1/2. renderBbFrameReset: no billboard
//               records are kept because no native renderer records any.)
//
//   FAIL-FAST — the hook is only ever called from a framework path this port has NOT yet stood up
//               (the Tomba-shaped native frame loop, PcScheduler stage bodies, the dev-warp/REPL
//               game commands). Being called means the run wandered into an un-RE'd path, and the
//               only correct response is to say so loudly and abort. A silent stub here would let a
//               half-wired path look like it worked, which is exactly the fake-green the porting doc
//               warns about.
//
// Members left NULL are ones the framework only reaches from those same un-stood-up paths AND whose
// signature has no way to fail loudly (they are never called on the Phase-0 boot path).
#include "core.h"
#include "game_iface.h"
#include "cfg.h"
#include <stdlib.h>

// ── boot ────────────────────────────────────────────────────────────────────────────────────────
// The framework's crt0_setup (native_boot.cpp) has already reproduced Spider-Man's crt0 prologue
// (BSS-zero, sp/gp, heap globals, InitHeap) — see game_config.cpp for the instruction-level mapping.
// bootInit is what runs after it. Tomba!2 transcribes its guest boot prologue here because it owns
// that code natively; this port owns none of it, so the honest Phase-0 body is the whole of it:
// dispatch the guest's own main() and let the substrate run the game.
//
// This is Phase 0 of docs/re-frontier.md ("everything on substrate"), NOT a stopgap standing in for
// a native boot — there is no native boot to stand in for yet.
static void spiderman_bootInit(Core* c) {
  cfg_logi("boot", "Phase 0: dispatching guest main() 0x%08X on the recompiled substrate", c->cfg->gameMain);
  rec_dispatch(c, c->cfg->gameMain);
}

// ── neutral ─────────────────────────────────────────────────────────────────────────────────────
extern void spiderman_install_diag_overrides(Game*);   // game/core/diag_overrides.cpp

static void spiderman_registerOverrides(Game* g) {
  // No native BEHAVIOUR overrides exist yet — this port owns no game function. The only thing
  // installed here is diagnostic, and only when its channel is on: a wrapper that logs and then
  // super-calls the original body, so a run with it enabled executes what a run without it does.
  spiderman_install_diag_overrides(g);
}

static void spiderman_renderFadeState(Core*, FadeState* out) {
  out->mode = 0;                      // 0 == no fade; the present path leaves pixels untouched
  out->r = out->g = out->b = 0;
}

static void spiderman_renderBbFrameReset(Core*) {
  // No native billboard records are kept — nothing to reset.
}

// ── fail-fast ───────────────────────────────────────────────────────────────────────────────────
static void unstood_up(const char* what) {
  cfg_loge("hooks", "%s was called, but this port has not stood that path up yet. "
                    "Reaching it means the run entered an un-RE'd framework path — see "
                    "docs/re-frontier.md. Refusing to continue with fabricated behaviour.", what);
  abort();
}

static void spiderman_frameUpdate(Core*)            { unstood_up("frameUpdate (native frame loop)"); }
static void spiderman_drawOTag(Core*, uint32_t)     { unstood_up("drawOTag (native frame loop)"); }
static int  spiderman_schedStageBody(Core*, int, void*) { unstood_up("schedStageBody (PcScheduler)"); return 0; }
static bool spiderman_schedFreshEntry(Core*, int, uint32_t, uint32_t) { unstood_up("schedFreshEntry (PcScheduler)"); return false; }
static bool spiderman_hasNativeHandlerForEntry(Core*, uint32_t) { return false; }   // truthfully: none
static void spiderman_devWarpAreaLoad(Core*)        { unstood_up("devWarpAreaLoad (dev warp)"); }
static int  spiderman_devAreaCount(Core*)           { return 0; }   // truthfully: no area index RE'd
static const char* spiderman_devAreaName(Core*, int){ return ""; }  // "" == no sourced name
static bool spiderman_devWarpAllowed(Core*)         { return false; }

static const GameHooks g_spiderman_hooks = {
    /* ctxCreate                */ nullptr,   // no per-Core game aggregate yet
    /* ctxDestroy               */ nullptr,
    /* frameUpdate              */ spiderman_frameUpdate,
    /* drawOTag                 */ spiderman_drawOTag,
    /* musicCoordTick           */ nullptr,
    /* cdDialogToneActive       */ nullptr,
    /* cdMusicFadeIn            */ nullptr,
    /* audioMixFrame            */ nullptr,   // no native music engine — SPU PCM passes through
    /* audioNowPlayingName      */ nullptr,
    /* audioSoundTestPlay       */ nullptr,
    /* bootInit                 */ spiderman_bootInit,
    /* schedFreshEntry          */ spiderman_schedFreshEntry,
    /* hasNativeHandlerForEntry */ spiderman_hasNativeHandlerForEntry,
    /* registerOverrides        */ spiderman_registerOverrides,
    /* renderFadeState          */ spiderman_renderFadeState,
    /* replBehaviorName         */ nullptr,
    /* replCamTeleport          */ nullptr,
    /* replCamTeleportOff       */ nullptr,
    /* renderBbFrameReset       */ spiderman_renderBbFrameReset,
    /* replCommand              */ nullptr,
    /* devWarpAreaLoad          */ spiderman_devWarpAreaLoad,
    /* devAreaCount             */ spiderman_devAreaCount,
    /* devAreaName              */ spiderman_devAreaName,
    /* devWarpAllowed           */ spiderman_devWarpAllowed,
    /* schedStageBody           */ spiderman_schedStageBody,
    /* schedRng                 */ nullptr,
    /* fps60WorldPass           */ nullptr,
    /* fps60BbSwapPrev          */ nullptr,
    /* selftestGame             */ nullptr,
};

const GameHooks* spiderman_game_hooks() { return &g_spiderman_hooks; }
