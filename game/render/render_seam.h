// render_seam.h — RenderSeam: where this port takes ownership of the picture.
//
// THE SEAM IS THE GUEST'S OWN submitFrame (FUN_80061308), NOT GameHooks::drawOTag.
//
// The framework's `GameHooks::drawOTag` has exactly two call sites, both inside
// `native_step_frame`, and this port never enters that loop — `spiderman_bootInit` dispatches the
// guest's own `main()` (0x8002C354), which never returns (claim C025, still holds). Implementing
// that hook here would be dead code, so it stays a fail-fast stub.
//
// The reachable seam is the engine's own frame-submit function. From the Ghidra decompile
// (docs/re-frontier.md RE-19, claim C029):
//
//     FUN_80061308:  ResetGraph(1)
//                    PutDispEnv(db + 0x5C)
//                    PutDrawEnv(db + 0x00)
//                    DrawOTag(db->ot + 0x3FFC)
//
// and `DrawOTag` (libgpu 0x80081ED0, identified by the string libgpu itself prints, not inferred)
// has EXACTLY TWO callers image-wide: this function and a library-internal one. So this is the ONE
// game-side OT submit, and at its entry the frame's display list is complete and the current
// double-buffer context names the OT and both envs — precisely the state Tomba!2's
// `Engine::drawOTag` is called with. It is a MAIN-module address, so the recomp override table
// reaches it today, with no framework change and without the port owning the frame loop.
//
// MEASURED, headless, 100 s, no input (scratch/re12/logs/fntrace1.log): 1761 calls, first at frame
// 2, 0 ABI violations — while two other traced addresses in the same run reported NEVER CALLED, so
// that instrument could produce the other answer.
//
// TWO LEGS, and today the reference leg is the DEFAULT — see render_seam.cpp for why that is the
// honest state of this port and not a fallback.
#pragma once
#include <stdint.h>

class Game; // external/psxport/runtime/recomp/game.h

// Install the override on the engine's submitFrame and set this port's default render leg.
// Call from main() BEFORE native_boot_run(), which is where PSXPORT_RENDER_PSX is read — so an
// explicit env setting still wins over the default set here.
void spiderman_install_render_seam(Game *g);
