---
id: I030
kind: instrument
status: trusted
created: 2026-08-12
---

## Instrument

tools/gate.py boot — the run gate: does the built spiderman_port still boot and keep advancing?

## Validated by

SEEN TO FAIL: --selftest now judges 18 cases — 16 through the same analyse() the real gate uses plus 2 through the real launcher. One known-good capture PASSES; 17 broken variants are caught (12 FAIL, 4 REFUSE, 1 GPU-device-loss STOP). The 2026-08-22 extension feeds the exact `Fps60::rq_capture OVERFLOW` from issue 0017 and requires a named FAIL, not the ambiguous short-run refusal the live pre-fix logs received. Fires on REAL logs too: check-log scratch/re20/logs/pcleg_final.log -> exit 1 on a genuine [FATAL:error] unimplemented native rendering abort; check-log scratch/logs/gate_newpin.log -> exit 3 on a real 'context is lost'; check-log scratch/logs/frame-fence-final.log -> PASS at frame 3880 / 1024 submissions / dem1 -> l1a1 after the frame-fence fix. BLIND to pixels, to the pc_render leg, to audio/input, and to anything past the run's cap; the frame/submit rate floors are load-sensitive on a shared machine. See INST-28 in instruments.md and issues 0014/0017.

## Known failure modes

(none recorded yet)
