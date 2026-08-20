---
id: 15
title: Boot gate supervisor does not terminate a progressing reference-path run at its cap
status: investigating
symptom: python3 tools/gate.py boot reaches dem1/l1a1 and keeps emitting frame progress, but neither the supervisor cap nor frame watchdog ends it; gate refuses and reports one process alive immediately after group kill
tags: gate,supervisor,hang,verification
created: 2026-08-20
updated: 2026-08-20
---

## Evidence

On 2026-08-20, a 12-second reference-path boot with the mesh probe reached the render seam and live
mesh calls, then exited 2 after 15 seconds. The gate reported that neither cap ended the process and
one process remained immediately after process-group signalling. The captured evidence is
`scratch/logs/gate-boot-20260820-223032.log`. An exact executable-path process listing immediately
afterward found no gate or port process, so the reported survivor self-exited before manual PID
cleanup. The longer `scratch/logs/gate-boot-20260820-221812.log` reached scene `l1a1` and reproduced
the same refusal.

## Root cause

Unknown. This issue records a verifier defect, not a game-rendering conclusion. Do not call these
gate runs passing until the supervisor/child shutdown contract is diagnosed and the same tool
demonstrates both a capped clean exit and an intentional-hang refusal.
