---
id: 14
title: No tool drives the built binary — this repo has no run gate, and Tomba2's gate SHAPE cannot be copied
status: open
symptom: every dynamic claim in this repo was measured by hand-launching spiderman_port with env vars; nothing mechanical would notice if boot regressed
tags: verification,gate,tooling
created: 2026-08-12
updated: 2026-08-12
---

MEASURED 2026-08-12. 28 tools in tools/; ZERO match /^gate/; 'grep -ln spiderman_port tools/*.py' returns nothing. The 5 tools that shell out invoke discdump, ghidra, emit.py and cmake — never the port binary. What verification exists is STATIC or POST-HOC: check_reloc_model.py (a shape check with a --selftest that refuses on a missing corpus), check_resume_switch.py, callee_contract.py, ra_classes.py, and the present_*/gp0_*/prim_* analysers that read dumps a run left behind. docs/codemap.md:49 calls check_reloc_model.py a 'gate', which invites exactly the wrong conclusion — it is not a run gate.

THE PREMISE'S IMPLIED FIX IS ONLY HALF RIGHT, and this is the part to remember: Tomba2Engine/tools/gate.py drives the game over the REPL, and THIS PORT NEVER ENTERS THE FRAMEWORK FRAME LOOP THAT SERVICES THE REPL (game/core/game_hooks.cpp:39-42 rec_dispatches the guest main, which never returns; frameUpdate/drawOTag are deliberate abort() fail-fasts). So a REPL-driven gate cannot work here as-is. A gate for this port has to key on its own log lines from a plain capped launch, or the port needs the framework loop first.

BOOT STATUS AT psxport 240d8f9a, so a future regression has a baseline: it boots and boots DEEP — 240s headless run reached rseam frame 14007 with 5632 submitFrame calls and 9 scene changes, clean. Those three counters are the obvious assertions for a gate (frames advanced, submitFrame calls > 0, scene changes > 0) plus 0 recomp-MISS and 0 fail-fast aborts.
