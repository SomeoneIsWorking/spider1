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

## Known failure modes

(none recorded yet)
