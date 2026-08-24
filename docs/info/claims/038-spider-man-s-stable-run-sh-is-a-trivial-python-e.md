---
id: C038
kind: claim
status: falsified
created: 2026-08-21
tags:
depends: run.sh, tools/run.py#launch, tools/run.py#port_commands
reconfirmed: 2026-08-21 14:15:27
verified_at: 2026-08-21 14:15:27
falsified_on: 2026-08-24
---

## Claim

Spider-Man's stable ./run.sh is a trivial Python exec wrapper; the Python launcher configures C++ with Clang and builds the required spiderman_port project target by default.

## Evidence

2026-08-21: run.sh contains only the shebang and exec of tools/run.py; CMake CTest launcher_policy passed 10 policy cases; a real wrapper run in a PATH without cmake refused with exit 1 and the named missing tool; CTest cpp_policy passed alongside it; cmake --build build --target spiderman_port -j16 completed with CMAKE_CXX_COMPILER_ID=Clang.

## What would falsify it

Falsified if run.sh regains nontrivial policy, the zero-special-mode launcher stops building/launching spiderman_port, a non-Clang C++ configure is accepted, or the real wrapper no longer discriminates a missing required tool.

## Re-confirmed 2026-08-21 02:04:04

2026-08-21 landing verification: after recording psxport eb2465b2c77f5a88f409e431703c618c92820b05, CMake configured with CMAKE_CXX_COMPILER_ID=Clang, spiderman_port built, CTest launcher_policy and cpp_policy passed, and tools/psxport_sync.py --check confirmed the build metadata and pin both name eb2465b2.

## Re-confirmed 2026-08-21 02:21:54

2026-08-21 final landing verification: after recording psxport be3815038e0df525a1b59d03f33f1b616b5d7c9f, CMake configured with CMAKE_CXX_COMPILER_ID=Clang, spiderman_port built, CTest launcher_policy and cpp_policy passed, and tools/psxport_sync.py --check confirmed the build metadata and pin both name be381503.

## Re-confirmed 2026-08-21 02:58:33

2026-08-21 clean-framework verification after recording psxport 2b5ef7b5522f3b879b69315acd11a037ca7a78bb: CMake reports CMAKE_CXX_COMPILER_ID=Clang, spiderman_port built, CTest launcher_policy and cpp_policy passed, and tools/psxport_sync.py --check confirmed build metadata and pin both name 2b5ef7b5.

## Re-confirmed 2026-08-21 11:40:39

Final 2026-08-21 verification after recording psxport 692b9b20e3d4a6194452522060fd2657c2235f40: CMake reports that exact framework SHA with CMAKE_CXX_COMPILER_ID=Clang, spiderman_port built, all four CTests passed, explicit cpp_policy checked format 33/33 and clang-tidy 21/21, and tools/psxport_sync.py --check confirmed build metadata and pin both name 692b9b20.

## Re-confirmed 2026-08-21 14:15:27

Final 2026-08-21 verification at psxport 3418a79b624765614f3f198dc1e89632e1e650f0: a fresh CMake configure selected Clang 22.1.8, spiderman_port built, all six CTests passed, cpp_policy checked format 39/39 and clang-tidy 25/25, and psxport_sync --check matched build metadata to the pin. The real zero-argument ./run.sh route rebuilt the required target, selected PSXPORT_RENDER_PATH=gte from its Default layer, and reached dem1 then l1a1 before the separately catalogued FPS60 queue-overflow boundary.

## FALSIFIED 2026-08-24

The 2026-08-24 player-launcher contract explicitly permits compatible GCC, Clang, and AppleClang and forbids compiler identity whitelists/blacklists. The shipping launcher now checks only that CC/CXX resolve to executables; Clang remains the separate maintainer verification toolchain.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
