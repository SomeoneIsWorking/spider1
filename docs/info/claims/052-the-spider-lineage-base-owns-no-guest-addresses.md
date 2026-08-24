---
id: C052
kind: claim
status: holds
created: 2026-08-22
tags: 
depends: game/core/spider_runtime.h#SpiderRuntime, titles/spiderman1/spider1_runtime.cpp#Spider1Runtime::registerOverrides, titles/spiderman1/spider1_runtime.cpp#Spider1Runtime::guestVramIsPicture, titles/spiderman2/enter_electro_runtime.cpp#EnterElectroRuntime::registerOverrides, titles/spiderman2/enter_electro_runtime.cpp#EnterElectroRuntime::guestVramIsPicture, cmake/title_manifest.cmake#spider_configure_target
reconfirmed: 2026-08-22 19:58:25
verified_at: 2026-08-22 19:58:25
---

## Claim

The Spider lineage base owns no guest addresses or title behavior: Spider1Runtime alone contains Spider-Man 1's bounded legacy adapter, compatibility services, overrides, temporal decorator, and measured guest-VRAM picture policy, while EnterElectroRuntime remains direct and temporally neutral and refuses its unmeasured picture policy.

## Evidence

Clean psxport d2266f4b builds both targets together and passes spider_runtime plus enter_electro_runtime tests. The Spider-Man 1 live regression installs 8 HLE services and all title overrides, audits crt0 10/10, dispatches 0x8002C354, reaches dem1/frame 2299/512 submits; Enter Electro reports no title overrides, null legacy views/context, and reaches its independent crt0 refusal. No executable links both generated substrates.

## What would falsify it

SpiderRuntime gains a guest address, render policy, or legacy state; EnterElectroRuntime gains a Spider-Man 1 compatibility view, temporal decorator, or guessed picture-policy answer; Spider1Runtime stops owning its compatibility services and measured picture policy; or either title target links the other title's generated registry.

## Re-confirmed 2026-08-22 19:56:05

Reconfirmed at 2026-08-22 19:56 after clean d2266f4b final gates: both title binaries build, full CTest passes 10/10, Spider-Man 1 reaches dem1/frame 2299/512 submits through Spider1Runtime-owned services, and Enter Electro reaches its independent crt0 refusal with null legacy views/context.

## Re-confirmed 2026-08-22 19:58:25

Final d2266f4b verification: full CTest 10/10, both targets link, live Spider-Man 1 reaches dem1/frame 2299/512 submits, live Enter Electro reaches its direct crt0 refusal, and reciprocal nm checks prove neither binary contains the other title runtime/installer symbols.

## Working-tree verification 2026-08-24

The common base still owns no guest address and now derives `serial()` from each title runtime's
immutable `ExecutableIdentity`, eliminating a duplicate serial authority. Clang build and full CTest
pass 12/12. Reciprocal `nm -C` controls show each binary's own runtime/installer symbols, while
`spiderman_port` contains no Enter Electro runtime/installer and `enter_electro_port` contains no
Spider1Runtime, `spiderman_install_*`, or `legacy::measuredConfig` symbol. No live game run was made
for this verification; the prior live evidence above remains the latest runtime evidence. Landing
operator must run `claim confirm C052` after committing.

## Working-tree verification 2026-08-24 — pure picture policy

Framework `9c2e3f1c` makes `GameRuntime::guestVramIsPicture(const Game&)` a required derived-title
policy. `Spider1Runtime` alone delegates this query to its bounded measured compatibility adapter and
the focused test obtains `true` through the shipping checked query. Enter Electro has not reached
render ownership: its override names EE-02 and aborts, and its focused test traps that abort as the
required opposite answer. The common base owns no default. No live game run was made for this
verification; landing operator must run `claim confirm C052` after committing.

## Working-tree verification 2026-08-24 — current clean framework

Against clean psxport `9c2e3f1c`, Clang built both products and all 12 CTests passed. Reciprocal
`nm -C` controls again found no Enter Electro runtime/installer in `spiderman_port` and no
Spider1Runtime, `spiderman_install_*`, or `legacy::measuredConfig` in `enter_electro_port`; the
corresponding own-title symbols were present in each binary. No game process was launched.
