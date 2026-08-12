---
id: I030
kind: instrument
status: trusted
created: 2026-08-12
---

## Instrument

tools/gate.py boot — the run gate: does the built spiderman_port still boot and keep advancing?

## Validated by

SEEN TO FAIL: --selftest judges 16 cases through the same analyse() the gate uses — 1 known-good capture PASSES, 15 mutations caught (11 FAIL, 3 REFUSE, 1 GPU-device-loss STOP). Fires on REAL logs too: check-log scratch/re20/logs/pcleg_final.log -> exit 1 on a genuine [FATAL:error] unimplemented native rendering abort; check-log scratch/logs/gate_newpin.log -> exit 3 on a real 'context is lost'. Two 120s runs of the built binary PASSED (frame 6813/2048 submits/4 scene changes; 6797/2048/4). BLIND to pixels, to the pc_render leg, to audio/input, and to anything past the run's cap; the frame/submit rate floors are load-sensitive on a shared machine. See INST-28 in instruments.md and issue 0014.

## Known failure modes

(none recorded yet)
