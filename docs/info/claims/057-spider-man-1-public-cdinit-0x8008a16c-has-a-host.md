---
id: C057
kind: claim
status: holds
created: 2026-08-27
tags: cd,frame-loop,spiderman1
depends: titles/spiderman1/spider1_platform_facts.h, titles/spiderman1/spider1_frame_driver.cpp#Spider1FrameDriver::initializeCd
---

## Claim

Spider-Man 1 public CdInit 0x8008A16C has a host-synchronous success contract with four fixed callback stores and no observable controller state

## Evidence

The authenticated retail success body writes 0x8008A238/260/288/0 to
0x800B3B14/18/1C7C/1C80, then returns 1. The direct-runtime title override
performs those four stores without the retired `GameConfig` adapter. Its
production-dispatch test checks the stores and return value with `core.cfg == nullptr`.
The 2026-09-12 authenticated Lightrec run crossed CdInit and next stopped at a
stock libcd `VSync(-1)` from return 0x8008D050, with 79,976 translated blocks,
395,713 instructions, and zero fallback blocks. Historical gate log
`gate-boot-20260827-022834` reached STR-player VSync at 0x8002AC8C after
further native CD service coverage.

## What would falsify it

A real launch observes guest state after CdInit that differs from the authenticated success stores/return, reaches controller-reset code from the public boundary, or fails before the next boot phase because a required CdInit side effect is absent.
