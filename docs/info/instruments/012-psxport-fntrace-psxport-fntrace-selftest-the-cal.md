---
id: I012
kind: instrument
status: trusted
created: 2026-08-05
---

## Instrument

PSXPORT_FNTRACE / PSXPORT_FNTRACE_SELFTEST — the callee-saved ABI check around a traced guest function

## Validated by

BOTH WAYS, on the same two control leaves, in this port. NEGATIVE: over a normal run 0x8008735C (setRECT, 4 calls) and 0x8008710C (the guest printf, 49 calls) report 0 ABI violations while 0x8002A338 and 0x8002B430 report violations in the same run — so it is not blanket-reporting. POSITIVE: with PSXPORT_FNTRACE_SELFTEST=1 (which xors s0 after the call) those same two leaves DO report 'VIOLATES THE ABI, s0 8009A6E4 -> 25AC0341' — so it can say the other thing on the very functions it was silent about. Logs: scratch/logs/re07b_trace1.log, re07b_selftest.log. This settles a doubt recorded in issue 0004: the ABI violation it reported on 0x8002A338 was suspected to be manufactured by fntrace's own uninstall/re-dispatch/reinstall dance. It was NOT — the violation was the RE-16 recompiler defect, and fixing that took the same counter to 0 over 423 calls. Note fntrace_init() was only wired into this port on 2026-08-05 (game/core/game_hooks.cpp), so any PRE-2026-08-05 fntrace conclusion in these docs rests on a tracer that never ran. Two live caveats, both in the tool's own header: it CLAIMS THE OVERRIDE SLOT (do not trace an address whose native override does real work), and it counts the OUTER call only for a recursive function

## Known failure modes

(none recorded yet)
