---
id: C029
kind: claim
status: holds
created: 2026-08-06
tags: render,frame-loop,drawotag,re-12,re-19,re-20,seam
depends: game/render/render_seam.cpp#RenderSeam::submitFrame, game/core/spider_runtime.cpp#SpiderRuntime::registerOverrides
reconfirmed: 2026-08-22 14:22:52
verified_at: 2026-08-22 14:22:52
---

## Claim

The game's OWN render-submit function FUN_80061308 is the port's reachable drawOTag seam: it is the only game-side caller of libgpu DrawOTag in the whole image, it runs once per engine-rendered frame, and the recomp override table reaches it today — so a native renderer does NOT require the port to own the frame loop.

## Evidence

MEASURED 2026-08-06, headless, 100 s, no input, PSXPORT_FNTRACE on 8 guest addresses (scratch/re12/logs/fntrace1.log). 0x80061308 = 1761 calls, first at frame 2 from ra=80061218, ABI violations 0. Companion counts from the same run: 0x800612B8 (beginFrame) 1765, 0x80061140 (gfx init) 1, 0x80061230 (OT/pool alloc) 1, 0x8002C174 (the gameplay frame driver) 2 (first at frame 2261), 0x800604CC 1 (frame 3714), 0x800160EC and 0x8006F294 NEVER CALLED. fntrace installs a REAL override at each traced address and re-dispatches the body, so those 1761 hits are themselves the proof that an override at 0x80061308 executes — a negative would have printed 'NEVER CALLED', which two of the eight sites did in the same run, so the instrument can produce both answers. STATIC SIDE, Ghidra headless (tools/ghidra_query.py): FUN_80061308 = ResetGraph(1) ; PutDispEnv(ctx+0x5C) ; PutDrawEnv(ctx+0x00) ; DrawOTag(ctx->ot + 0x3FFC). The libgpu leaves are identified by the diagnostic strings libgpu itself emits — DrawOTag 0x80081ED0 ('DrawOTag(%08x)' @0x80095EA0), ClearOTagR 0x80081DC8, PutDispEnv 0x80082000, PutDrawEnv 0x80081F40, DrawSync 0x800819A4, ResetGraph 0x8008173C — not by inference. 'xrefs 0x80081ED0' returns exactly 2 callers image-wide: FUN_80061308 and library-internal FUN_8008A614.

## What would falsify it

Run the port with an override installed at 0x80061308 that does NOT super-call. If the presented picture does not collapse (distinct-colour count per present, tools/present_flicker.py — NOT non-black %, which reads ~99.8% on both classes), then this function is not what puts the guest's geometry on screen and the claim is wrong. Also falsified if a run that reaches a screen this measurement never covered (a loaded level, a pause menu, a cutscene) shows engine-rendered frames while the 0x80061308 counter stays flat.

## Re-confirmed 2026-08-22 14:22:52

2026-08-22 after SpiderRuntime migration: scratch/logs/gate-boot-20260822-141229.log reports render seam installed at 0x80061308 and fired on call 1/frame 2/ra 0x80061218, then advanced to 6144 calls across 10 scene changes with no failure pattern.
