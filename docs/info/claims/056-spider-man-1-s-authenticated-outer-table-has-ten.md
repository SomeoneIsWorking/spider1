---
id: C056
kind: claim
status: holds
created: 2026-08-27
tags: frame-loop,spiderman1
depends: titles/spiderman1/spider1_mode_driver.h#spider1OuterRoute,titles/spiderman1/spider1_mode_driver.cpp#Spider1ModeDriver::step,CMakeLists.txt
---

## Claim

Spider-Man 1's authenticated outer table has ten selector routes, and the native driver models their finite continuation state behind the dynamic guest-execution boundary

## Evidence

SLUS_008.75 RAM at 0x80093C3C contains little-endian targets 8002C558,8002C47C,8002C5F8,8002C59C,8002C59C,8002C500,8002C578,8002C4E4,8002C47C,8002C574. `spider1OuterRoute` is the production mapping exercised for selectors 0..11 by `spider1_mode_transition_test`. The native owner compiles against `GuestExecution::callOriginal`, so retail bodies remain dynamically executed rather than linked as generated host functions. The owner is deliberately not attached to the player until authenticated Lightrec execution reaches this lifecycle; behavioral parity is not claimed.

## What would falsify it

Falsified if authenticated SLUS_008.75 table bytes differ, any selector follows another production route, or a product run diverges before or after a native mode transition from ordinary Lightrec execution.
