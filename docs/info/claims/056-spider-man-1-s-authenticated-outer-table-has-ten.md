---
id: C056
kind: claim
status: holds
created: 2026-08-27
tags: frame-loop,spiderman1
depends: titles/spiderman1/spider1_mode_driver.h#spider1OuterRoute, titles/spiderman1/spider1_mode_driver.cpp#Spider1ModeDriver::step, cmake/spiderman_port.cmake#GAME_SRC
---

## Claim

Spider-Man 1's authenticated outer table has ten selector routes, and the native driver preserves every generated mode super while owning their finite continuation state

## Evidence

SLUS_008.75 RAM at 0x80093C3C contains little-endian targets 8002C558,8002C47C,8002C5F8,8002C59C,8002C59C,8002C500,8002C578,8002C4E4,8002C47C,8002C574. spider1OuterRoute is the production mapping exercised for selectors 0..11 by spider1_mode_transition_test. The Clang spiderman_port link compiles generated gen_func_8002C354, gen_func_8002C174, gen_func_800604CC, gen_func_800160EC, and gen_func_8006F294 alongside Spider1ModeDriver; no generated source was edited. A real launch reached the render seam and correctly aborted at the earlier boot STR player's residual VSync before this outer driver was entered, so behavioral parity is not claimed.

## What would falsify it

Falsified if authenticated SLUS_008.75 table bytes differ, any selector follows another production route, any of the five gen_func supers leaves the linked product, or a product run diverges before/after a native mode transition from the preserved generated route.
