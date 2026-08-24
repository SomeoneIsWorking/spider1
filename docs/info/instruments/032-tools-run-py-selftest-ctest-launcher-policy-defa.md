---
id: I032
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/run.py --selftest / CTest launcher_policy — default launcher policy without building or launching

## Validated by

2026-08-21: CTest launcher_policy passed 10 positive/refusal cases; the real ./run.sh wrapper was also run with PATH containing only python3 and dirname, and returned exit 1 with '[run] error: cmake not found', proving the instrument distinguishes a refusal from PASS.

2026-08-24: the expanded 27-case selftest was fed three answers that must fail: a temporary launcher
shim using bare `python3` instead of frozen uv, and a dependency probe with every prerequisite except
`glslc`, plus a duplicate `--prepare-only`. It rejected all three, including the exact Fedora
`sudo dnf install glslc` command. Positive checks assert both selected products, locked Python,
compiler-and-policy-keyed `scratch/build/player/` paths, the framework-owned submodule-sync working
directory, `BUILD_TESTING=OFF`, `PSXPORT_BUILD_TESTS=OFF`, and no CTest command. The exact
direct frozen preparation route then built only `spiderman_port` in an isolated tree with no CTest
metadata or test target and stopped before game launch.

## Known failure modes

(none recorded yet)
