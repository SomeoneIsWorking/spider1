---
id: C057
kind: claim
status: holds
created: 2026-08-27
tags: cd,frame-loop,spiderman1
depends: game/core/game_config.cpp, titles/spiderman1/spider1_frame_driver.cpp#Spider1FrameDriver::initializeCd
---

## Claim

Spider-Man 1 public CdInit 0x8008A16C has a host-synchronous success contract with four fixed callback stores and no observable controller state

## Evidence

Authenticated generated body writes 0x8008A238/260/288/0 to 0x800B3B14/18/1C7C/1C80 then returns 1. The title override calls Cd::hleInit with those configured values. Real gate log gate-boot-20260827-022834 advances beyond the prior 0x8008C944 VSync(-1) timeout and reaches STR-player VSync at 0x8002AC8C.

## What would falsify it

A real launch observes guest state after CdInit that differs from the authenticated success stores/return, reaches controller-reset code from the public boundary, or fails before the next boot phase because a required CdInit side effect is absent.
