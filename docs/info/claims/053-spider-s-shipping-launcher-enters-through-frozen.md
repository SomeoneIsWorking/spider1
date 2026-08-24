---
id: C053
kind: claim
status: holds
created: 2026-08-24
tags:
depends: run.sh, bootstrap.py, tools/run.py#player_build_root, tools/run.py#discdump_commands, tools/run.py#port_commands, tools/run.py#launch, tools/launcher_dependencies.py#require_native_dependencies
reconfirmed: 2026-08-24 23:28:59
verified_at: 2026-08-24 23:28:59
---

## Claim

Spider's shipping launcher enters through frozen uv, propagates its interpreter, does not classify compiler identity, builds only the selected product in a compiler-and-policy-keyed test-disabled player tree, and emits platform package commands for missing native dependencies.

## Evidence

2026-08-24: `uv run --frozen python -B tools/run.py --selftest` passed 27 positive/refusal checks without configuring, building, running CTest, or launching either game. The suite uses fake GCC/G++ paths and injected dependency probes, asserts locked `Python3_EXECUTABLE`, compiler-and-policy-keyed `scratch/build/player/` paths, `BUILD_TESTING=OFF`, the framework-owned submodule-sync working directory, `PSXPORT_BUILD_TESTS=OFF` for the standalone framework tool build, selected product targets, and the absence of any CTest command; it rejects a deliberately broken non-uv shim, duplicate preparation option, and missing glslc, and checks the exact Fedora DNF command.

The exact direct frozen preparation route, `CC=clang CXX=clang++ uv run --frozen python -B bootstrap.py --prepare-only`, then authenticated the real Spider-Man disc/executable, regenerated MAIN plus all 30 runtime modules, and built only `spiderman_port` under the compiler-and-policy-keyed `scratch/build/player/` tree. The framework tool tree records `PSXPORT_BUILD_TESTS=OFF`, the title tree records `BUILD_TESTING=OFF`, neither project-owned tree has a `CTestTestfile.cmake`, and the title target inventory has no test/CTest/policy target. Both record `CMAKE_CXX_COMPILER_ID=Clang`. The command stopped with "Spider-Man is built and ready" before process launch. The final repetition used framework revision `9c2e3f1c +dirty` because a separate syscall milestone began changing the shared framework during this gate; repeat after that work lands before claiming a clean pinned-tree verification.

## What would falsify it

Any change to run.sh, bootstrap.py, pyproject.toml, uv.lock, tools/run.py#port_commands, tools/run.py#launch, or tools/launcher_dependencies.py; or a selftest that no longer detects a non-frozen shim, interpreter escape, compiler identity filter, test-enabled product configure, or wrong package command.

## Re-confirmed 2026-08-24 23:28:59

The 27-case direct selftest passed in this tree. The isolated direct preparation first passed against
clean psxport `9c2e3f1c`; after the final test-disable correction, it passed against the same revision
plus concurrent uncommitted framework syscall work. `run.sh` itself was not invoked, and no game
process was launched. A clean framework landing is this claim's remaining re-verification condition.
