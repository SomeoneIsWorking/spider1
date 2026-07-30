---
id: C011
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
reconfirmed: 2026-07-30
---

## Claim

The 0x080252D4 fault is NOT caused by the recompiler's emission design for the 0x8002A338 coroutine complex. Six independent emission designs produce it byte-identically; the common factor is that each one lets the decode loop actually RUN. The cause lies inside what the decoder does, not in how its control flow is rendered.

## Evidence

Attempts 9, 12, 13, 19, 21 and 22 die with identical registers (ra=0x03FF03FF fp=0x03FF0FFF s0=0x03FF07FF s1=0x47FF03FF, v0=0x080251F4, last-fn-entered=0x800654E8) across: rec_dispatch resume (9/12), attempt 13's flat emission, attempt 19's runtime resume switch, 21 (+ 0x8002A5F4 demoted), and 22 (+ jal rendering made consistent, collision gate GREEN). Two pre-registered falsifiers fired: demoting 0x8002A5F4 changed nothing (C010), and closing all 25 resume-switch collisions changed nothing. Host backtrace at the fault is fully unwound -- gen_func_8006BF9C <- gen_func_8002C354 <- native_boot_run <- main -- so the decoder has RETURNED and a later dispatch faults on an already-garbaged register file. The 0x3FF values are the decoder's own EOB/escape constant (0x8002A474 / 0x8002A694), so the register file is being filled with bitstream data rather than with stack garbage.

## What would falsify it

If an entry/exit contract check on gen_func_8002A338 (snapshot sp + s0-s7 at entry, compare at the real 'default: return') shows the decoder itself failing to preserve them, the corruption IS inside the rendered control flow after all and this claim is wrong. Also falsified if some seventh emission design makes the fault move or disappear.

## Re-confirmed 2026-07-30

Pre-registered falsifier RUN and it did NOT fire. Contract check installed on gen_func_8002A338 (snapshot sp + s0-s7 at entry, compare after the super-call, report on first violation AND on the clean case so silence is never the only signal): 'call #1: CONTRACT OK -- sp and s0-s7 all preserved'. The decoder returns with every callee-saved register and sp intact, so it does not corrupt the register file, and the emission of its control flow is exonerated. Measured on the attempt-22 build (6 demotions, collision gate GREEN).
