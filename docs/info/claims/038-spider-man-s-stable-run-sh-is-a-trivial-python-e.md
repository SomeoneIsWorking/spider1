---
id: C038
kind: claim
status: holds
created: 2026-08-21
tags:
depends: run.sh, tools/run.py#launch, tools/run.py#port_commands
reconfirmed: 2026-08-21 02:58:33
verified_at: 2026-08-21 02:58:33
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
