---
id: 14
title: No tool drives the built binary — this repo has no run gate, and Tomba2's gate SHAPE cannot be copied
status: resolved
symptom: every dynamic claim in this repo was measured by hand-launching spiderman_port with env vars; nothing mechanical would notice if boot regressed
tags: verification,gate,tooling
created: 2026-08-12
updated: 2026-08-12
---

MEASURED 2026-08-12. 28 tools in tools/; ZERO match /^gate/; 'grep -ln spiderman_port tools/*.py' returned nothing. The tools that spawned processes invoked discdump, Ghidra, the retired offline translator, and CMake — never the port binary. Verification was static or post-hoc: a relocation shape check, resume-switch and ABI analysers, and presentation/GP0/primitive analysers that read dumps a run left behind. The then-current codemap called a shape check a 'gate', which invited exactly the wrong conclusion — it was not a run gate.

THE PREMISE'S IMPLIED FIX IS ONLY HALF RIGHT, and this is the part to remember: Tomba2Engine/tools/gate.py drives the game over the REPL, and THIS PORT NEVER ENTERS THE FRAMEWORK FRAME LOOP THAT SERVICES THE REPL (game/core/game_hooks.cpp:39-42 rec_dispatches the guest main, which never returns; frameUpdate/drawOTag are deliberate abort() fail-fasts). So a REPL-driven gate cannot work here as-is. A gate for this port has to key on its own log lines from a plain capped launch, or the port needs the framework loop first.

BOOT STATUS AT psxport 240d8f9a, so a future regression has a baseline: it boots and boots DEEP — 240s headless run reached rseam frame 14007 with 5632 submitFrame calls and 9 scene changes, clean. Those three counters are the obvious assertions for a gate (frames advanced, submitFrame calls > 0, scene changes > 0) plus 0 recomp-MISS and 0 fail-fast aborts.

RESOLVED 2026-08-12 — `tools/gate.py`. `python3 tools/gate.py boot` launches the already-built
`scratch/bin/spiderman_port` headless, capped (never `./run.sh`), and keys on
the port's own log lines, as this issue said it would have to. Seven assertions: `[boot] loaded …:
entry 0x…`, `[boot] Phase 0: dispatching guest main()`, `[rseam] render seam installed at 0x…`,
`[rseam] submitFrame override REACHED` (the line the port's own install message names as its
proof-of-fire), ADVANCE between the first and last periodic `[rseam] submitFrame calls=… frame=…`
line, frame/submit floors at 35% of the baseline RATE (never an absolute end frame — the scene order
is not even stable run to run: the baseline reached `l1a1` first, the 2026-08-12 gate runs reached
`dem1` first), and ≥2 scene changes into printable scene names. Refusals exit 2; GPU device loss
exits 3 as a session-wide STOP.

MEASURED on the gate itself: two 120s runs PASSED (frame 6813 / 2048 submits / 4 scene changes and
frame 6797 / 2048 / 4 — 56 frames/s against the baseline's 58), and `check-log` re-confirms the
recorded baseline log mechanically (frame 14007, 5632 submits, 9 scene changes). `--selftest` judges
16 cases through the SAME analyser: 1 known-good capture PASSES, 15 broken variants are caught
(11 FAIL, 3 REFUSE, 1 device-loss STOP).

TWO CALIBRATION FINDINGS worth keeping, both caught by running the gate against REAL logs rather
than reasoning about it:
1. The sibling gate's `/\babort\b/` failure pattern MATCHES this port's healthy startup banner
   ("… then aborts at the next scene naming it. That abort is the correct result"), so copying that
   list would have failed every green run. Patterns here are anchored to how a failure PRINTS —
   lucent renders an error as `[<channel>:error]`, hence `[FATAL:error]`; `[watchdog] STUCK` is
   case-sensitive and tag-anchored because the ordinary timeout-kill line says "where it was stuck".
2. ORDERING: the first version REFUSED (exit 2, "nothing proven") over
   `scratch/re20/logs/pcleg_final.log` — a REAL pc_render-leg abort at submitFrame call #2 — because
   the "too few progress lines to speak about ADVANCE" refusal returned ahead of the failure-pattern
   check. An abort is a FAILURE however early it happened; only an otherwise-clean short run is a
   refusal. Selftest case 14 now pins that shape.

NOT covered by this gate, stated so a pass is not overread: pixels, the pc_render leg (the default
leg is psx_render, the reference; pc_render has no display-list producer and aborts by design),
audio, input, and anything past the ~2 minutes it runs. The frame/submit floors are LOAD-SENSITIVE —
several agents share this machine — so a failure on those alone, with everything else green, is the
one verdict to re-run before believing.

AUDITED 2026-08-12, SECOND PASS — the gate was re-verified by running it, and TWO REAL DEFECTS were
found in the part nothing covered: the LAUNCHER's hang branch. `--selftest` had 16 cases and all 16
judged captured TEXT through `analyse()`; the launcher itself had exactly one case (missing binary).
1. The hang refusal wrote only `e.stdout` to its log — and the port writes **100% of its output to
   stderr** (measured: 92 stderr lines / 0 stdout lines from a 12s capped run). The refusal for the
   single failure that most needs evidence pointed at an EMPTY file.
2. `subprocess.run(timeout=)` kills only the DIRECT child, so the hang path killed a
   wrapper and left `spiderman_port` running, reparented — measured with a stand-in: **2 orphans per
   hang**. A GPU-holding orphan is what the next gate run contends with while this one reports a tidy
   refusal.
Both fixed: the launch takes its own session (`start_new_session=True`), a hang `killpg`s the group and
the refusal states processes-signalled / still-alive / lines-captured. Selftest case 17 drives the real
`cmd_boot` into that branch and was run against BOTH the pre-fix and post-fix launcher — it FAILS on the
old one (log 0 lines, marker absent, heartbeat still advancing) and PASSES on the new one.
THE AUDIT'S OWN CHECK LIED TWICE FIRST, which is the more transferable finding: the survivor test
scanned `ps` for the stand-in's NAME (the grandchild's argv is `sleep 600`), then for a marker inside
`sh -c 'sleep 600 # MARK'` — which dash EXEC-OPTIMISES into bare `sleep 600`, erasing it. Both versions
printed "surviving=0" against a launcher that provably leaked. Survival is now detected by an advancing
HEARTBEAT mtime, with `grandchild spawned=…` printed as the denominator so "nothing survived" cannot be
read off a grandchild that never started.
