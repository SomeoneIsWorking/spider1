// game_hooks.cpp — bounded compatibility callbacks not yet represented by GameRuntime interfaces.
//
// SpiderRuntime owns boot and override orchestration. This table remains only for framework paths
// whose callbacks have not yet migrated to narrow typed owners.
//
// There are exactly two kinds of member here, and the distinction is the point:
//
//   NEUTRAL   — the callback asks "what does the GAME's native code contribute here?", and the
//   honest
//               Phase-0 answer is "nothing, because nothing is owned yet". A neutral body is the
//               CORRECT semantic, not a placeholder. (renderFadeState: mode 0 is the framework's
//               own "no fade applied" — see gpu_vk.cpp
//               dump_to, which only alters pixels for mode 1/2. renderBbFrameReset: no billboard
//               records are kept because no native renderer records any.)
//
//   FAIL-FAST — the callback is only ever called from a framework path this port has NOT yet stood
//   up
//               (the Tomba-shaped native frame loop, PcScheduler stage bodies, the dev-warp/REPL
//               game commands). Being called means the run wandered into an un-RE'd path, and the
//               only correct response is to say so loudly and abort. A silent stub here would let a
//               half-wired path look like it worked, which is exactly the fake-green the porting
//               doc warns about.
//
// Members left NULL are ones the framework only reaches from those same un-stood-up paths AND whose
// signature has no way to fail loudly (they are never called on the Phase-0 boot path).
#include "cfg.h"
#include "core.h"
#include "game_iface.h"
#include "legacy_game_interface.h"
#include "str_skip_oracle.h"
#include <stdlib.h>

// ── neutral ─────────────────────────────────────────────────────────────────────────────────────

static void spiderman_renderFadeState(Core *, FadeState *out) {
  out->mode = 0; // 0 == no fade; the present path leaves pixels untouched
  out->r = out->g = out->b = 0;
}

static void spiderman_renderBbFrameReset(Core *) {
  // No native billboard records are kept — nothing to reset.
}

// ── fail-fast ───────────────────────────────────────────────────────────────────────────────────
static void unstood_up(const char *what) {
  cfg_loge("hooks",
           "%s was called, but this port has not stood that path up yet. "
           "Reaching it means the run entered an un-RE'd framework path — see "
           "docs/re-frontier.md. Refusing to continue with fabricated behaviour.",
           what);
  abort();
}

static void spiderman_frameUpdate(Core *) {
  unstood_up("frameUpdate (native frame loop)");
}
static void spiderman_drawOTag(Core *, uint32_t) {
  unstood_up("drawOTag (native frame loop)");
}
static int spiderman_schedStageBody(Core *, int, void *) {
  unstood_up("schedStageBody (PcScheduler)");
  return 0;
}
static bool spiderman_schedFreshEntry(Core *, int, uint32_t, uint32_t) {
  unstood_up("schedFreshEntry (PcScheduler)");
  return false;
}
static bool spiderman_hasNativeHandlerForEntry(Core *, uint32_t) {
  return false;
} // truthfully: none
static void spiderman_devWarp(Core *, int, int) {
  unstood_up("devWarp");
}
static int spiderman_devAreaCount(Core *) {
  return 0;
} // truthfully: no area index RE'd
static const char *spiderman_devAreaName(Core *, int) {
  return "";
} // "" == no sourced name
static bool spiderman_devWarpAllowed(Core *) {
  return false;
}

// DESIGNATED initialisers, deliberately — every hook binds BY NAME.
//
// This was a positional list, and the framework adding one warp field (upstream
// ff123d81) slid every later entry by one. That break was LOUD only by luck: the adjacent hooks
// happened to have different signatures, so the compiler rejected the conversions. Two neighbours
// with the SAME signature — and this struct is mostly `void (*)(Core*)` — would have compiled
// silently and called the wrong function, which is the kind of defect that surfaces days later as
// inexplicable behaviour with nothing pointing at the cause.
//
// Binding by name removes the whole class: a field inserted anywhere upstream cannot rebind
// anything here, and a field RENAMED or removed becomes a compile error naming the field, which is
// exactly the signal we want. Unlisted fields are value-initialised to null, so the framework's
// "may be null" hooks need no entry at all — the list below is now precisely what this port has
// actually stood up, and reads as that inventory.
//
// C++20 requires designators in declaration order; keep them so when adding one.
static const GameHooks g_spiderman_hooks = {
    .frameUpdate = spiderman_frameUpdate,
    .drawOTag = spiderman_drawOTag,
    .schedFreshEntry = spiderman_schedFreshEntry,
    .hasNativeHandlerForEntry = spiderman_hasNativeHandlerForEntry,
    .renderFadeState = spiderman_renderFadeState,
    .renderBbFrameReset = spiderman_renderBbFrameReset,
    .devWarp = spiderman_devWarp,
    .devAreaCount = spiderman_devAreaCount,
    .devAreaName = spiderman_devAreaName,
    .devWarpAllowed = spiderman_devWarpAllowed,
    .schedStageBody = spiderman_schedStageBody,
    .selftestGame = spiderman_str_skip_selftest,
};

const GameHooks &spider::legacy::compatibilityHooks = g_spiderman_hooks;
