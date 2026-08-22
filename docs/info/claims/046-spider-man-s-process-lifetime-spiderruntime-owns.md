---
id: C046
kind: claim
status: holds
created: 2026-08-22
tags: runtime,inheritance,architecture,boot
depends: game/core/spider_runtime.cpp#SpiderRuntime, game/core/main.cpp#main, game/core/game_hooks.cpp
---

## Claim

Spider-Man's process-lifetime SpiderRuntime owns boot dispatch, render policy, and title override installation while the legacy GameHooks bootInit/registerOverrides slots are null; the migration preserves the Phase-0 live path

## Evidence

2026-08-22 against psxport 7f5d3f13: CTest spider_runtime instantiated the shipping SpiderRuntime, installed it into Core, and verified both legacy behavior slots null; full Clang spiderman_port build passed; scratch/logs/gate-boot-20260822-141229.log shows the derived path install VSync/render/CD/module/card/diagnostics, dispatch guest main 0x8002C354, then advance to frame 19889 / 6144 submitFrame calls / 10 scene changes with no failure pattern, and tools/gate.py check-log PASS. The boot wrapper itself refused only because catalogued issue 0015 left the progressing child alive past its cap.

## What would falsify it

SpiderRuntime stops deriving GameRuntime, a boot or override callback becomes non-null in legacy compatibilityHooks, an installer moves out of SpiderRuntime without a typed owner, or a fresh boot check-log fails to reach and advance the same Phase-0 path
