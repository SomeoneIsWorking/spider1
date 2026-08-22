---
id: C025
kind: claim
status: holds
created: 2026-08-06
tags: render,drawOTag,native-frame-loop,phase-0,re-12
depends: external/psxport/runtime/recomp/native_boot.cpp, game/core/spider_runtime.cpp#SpiderRuntime::bootInit, game/core/game_hooks.cpp
reconfirmed: 2026-08-22 19:16:21
verified_at: 2026-08-22 19:16:21
---

## Claim

GameHooks::drawOTag is UNREACHABLE in this port at Phase 0 — the native frame loop is never entered, so a native-renderer skeleton implemented on that hook is dead code

## Evidence

hooks->drawOTag has exactly 2 call sites in all of psxport (external/psxport/runtime/recomp/native_boot.cpp:173 and :194), both inside native_step_frame, which only runs from game_main's frame loop at native_boot.cpp:452. SpiderRuntime::bootInit (game/core/spider_runtime.cpp) rec_dispatches the guest's own main() 0x8002C354, which never returns, so game_init never returns and the loop below it never starts. INSTRUMENT: the unconditional lucent::info at native_boot.cpp:295, 'entering native frame loop'. NEGATIVE: 0 occurrences in a 230s windowed run reaching present 13757 (scratch/logs/g8/ovload_census.log), while its sibling lucent::info from the same file, 'entering native crt0 (PC-driven)', DID print (scratch/logs/g8/base_psx.log:30) together with '[boot] Phase 0: dispatching guest main() 0x8002C354' — so the sink is live. POSITIVE CONTROL, the other class: the same line IS present in Tomba2Engine runs (grep -h 'entering native frame loop' ../Tomba2Engine/scratch/logs/*.log). Reconfirmed after the runtime migration by scratch/logs/gate-boot-20260822-141229.log: 6144 submitFrame calls and no native-frame-loop entry.

## What would falsify it

any new call site of hooks->drawOTag outside native_step_frame, OR SpiderRuntime::bootInit ceasing to rec_dispatch the guest main() and returning instead (i.e. RE-12 landing a native frame loop). Re-run: grep -rn 'hooks->drawOTag' external/psxport and grep 'entering native frame loop' in a fresh windowed run log.

## Re-confirmed 2026-08-22 14:22:52

2026-08-22 after SpiderRuntime migration: scratch/logs/gate-boot-20260822-141229.log dispatches guest main from the derived runtime, advances through 6144 submitFrame calls and 10 scene changes, and contains no 'entering native frame loop'; CTest spider_runtime verifies the legacy drawOTag table remains compatibility-only with boot/override slots null.

## Re-confirmed 2026-08-22 19:16:21

Post-commit af8a3c0 authoritative Clang CTest passes 8/8; SpiderRuntime boot remains the guest frame loop and GameHooks drawOTag remains unreachable.
