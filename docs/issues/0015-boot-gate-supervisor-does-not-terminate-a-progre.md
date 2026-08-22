---
id: 15
title: Boot gate supervisor does not terminate a progressing reference-path run at its cap
status: investigating
symptom: python3 tools/gate.py boot reaches dem1/l1a1 and keeps emitting frame progress, but neither the supervisor cap nor frame watchdog ends it; gate refuses and reports one process alive immediately after group kill
tags: gate,supervisor,hang,verification
created: 2026-08-20
updated: 2026-08-22
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

### Note (2026-08-21)
Reproduced on 2026-08-21 after the final Clang-format/tidy rebuild: the forced-Cross reference run
advanced through a live mesh `MATCH` at frame 482 and into scene `l1a1`, but `tools/gate.py boot
--seconds 12 --grace 5` still exited 2 because the supervisor reported the progressing child alive
after group termination. No `spiderman_port` process remained when checked immediately afterward.
Log: `scratch/logs/gate-boot-20260821-010831.log`. This is the known gate-lifecycle baseline, not a
mesh-probe failure.

### Note (2026-08-22)

Reproduced unchanged after the `SpiderRuntime` inheritance migration against psxport `7f5d3f13`.
The default 120-second run continued until the gate's 240-second refusal, reached frame 19889 / 6144
submitFrame calls / 10 scene changes, then reported one child alive after process-group signalling.
The exact PID had already exited when checked for scoped cleanup. Re-judging the 93 captured lines
with the same analyzer reports PASS and no game failure pattern; only supervisor lifecycle refused.
Log: `scratch/logs/gate-boot-20260822-141229.log`.

### Note (2026-08-22)
2026-08-22 reproduced twice during the face-builder census. Both 15s/10s progressing dem1 runs refused after the gate grace window and reported one survivor after group signalling; exact-path ps checks immediately afterward found no spiderman_port or gate process. Logs: scratch/logs/gate-boot-20260822-174218.log and gate-boot-20260822-174725.log. The latter captured 20,480 classified calls with zero unknown sites/mismatches before refusal, so the analyzer evidence is useful but the boot gate is still not a passing lifecycle verifier.

### Note (2026-08-22)
On recorded psxport pin ad5cf802, scratch/logs/gate-boot-20260822-181035.log reached dem1 and 16,384 classified face calls with zero census mismatches, then reproduced the same supervisor refusal after the 10-second cap. Exact executable-path ps immediately afterward found no surviving gate or Spider process. A separate first run on this pin aborted earlier in the allocator and is tracked independently as issue 0018 rather than being folded into this lifecycle defect.
