---
id: C010
kind: claim
status: falsified
created: 2026-07-30
tags: RE-16
falsified_on: 2026-07-30
---

## Claim

Retired hypothesis (false): 0x8002A5F4 cannot be emitted as a host callee because its four callers'
links lie inside its overlapping discovered extent, causing its resume switch to consume those links
instead of returning. It was also incorrectly named as the cause of the 0x080252D4 fault.

## Correction 2026-08-25

An exact executable-CFG walk disproves the absolute callee-contract claim, not merely its association
with the old fault. From entry 0x8002A5F4, every path joins 0x8002A478 and reaches the computed return
at 0x8002A460. The current emitter's switch there contains only the helper's two reachable in-body
resume links, 0x8002A708 and 0x8002A774. The four external caller links
0x8002A77C/784/78C/794 take the default dispatch-and-return path, so they are not consumed as local
continuations. The helper cannot reach the enclosing decoder's save-and-suspend tail at 0x8002A7F4;
that tail is downstream of the separate 0x8002A424 continuation.

The broad all-label resume switch measured on attempt 19 was an obsolete emitter defect. Its address
collision did not establish that 0x8002A5F4 itself had to be demoted, and the current emitted CFG is
the bounded falsifier that separates those two claims. `tools/callee_contract.py` no longer carries
0x8002A5F4 as an expected violation. See issue 0018 for the remaining intermittent VLC overrun.

## Evidence

Static, on the emitted attempt-19 artifact, reproduced by an independently written scan: 28 addresses across 7 bodies have a 'case 0x..u:' in a 'switch (c->r[31])' that is ALSO a host-call link ('c->r[31] = 0x..u;' followed by a func_..(c) call). gen_func_8002A5F4 carries 0x8002A77C/784/78C/794 -- exactly the links emitted at L_8002A774 in gen_func_8002A338. Denominators: 102 files, 30 switch sites, 48 distinct case addrs, 16946 link sites / 13354 distinct link addrs. Independently, over the executable: 0x8002A5F4 has 4/4 jal call sites whose site+8 falls inside its extent [0x8002A5F4,0x8002A840). Mechanism: the callee's fast path reaches jr $ra with $ra = a caller link, the switch steals what was a genuine one-level return, runs the rest of the chain inside the callee including the shared tail's frame pop, then default-returns -- leaving the caller to re-execute the remaining calls on a guest that already finished and freed the frame. sp skews 4 bytes per spurious pass and the reloads come from stack words holding bitstream data, which is the observed ra=0x03FF03FF / s0=0x03FF07FF / fp=0x03FF0FFF saturation.

## What would falsify it

If demoting 0x8002A5F4 into 0x8002A338 (closing the collision) leaves the 0x080252D4 fault unchanged, the resume-switch mechanism itself is back under suspicion and the next probe is an entry/exit contract check on gen_func_8002A338 (snapshot sp + s-regs at entry, assert preserved at the real default: return).

## FALSIFIED 2026-07-30; mechanism corrected 2026-08-25

Its own falsifier fired. 0x8002A5F4 was demoted (symbol absent from every shard, verified), the collision it caused is gone, and the 0x080252D4 fault is BYTE-IDENTICAL: ra=0x03FF03FF fp=0x03FF0FFF s0=0x03FF07FF s1=0x47FF03FF, v0=0x080251F4. A second suspect was then eliminated the same way: restricting the in-body link+goto rendering to DEMOTED targets only (so a guest jal site renders identically in every body that carries a copy) took tools/check_resume_switch.py from 25 collisions across 6 bodies to GREEN -- and the fault is still byte-identical. So neither 0x8002A5F4-as-host-callee nor the resume-switch collision causes 0x080252D4. The later exact CFG correction above further narrows this: the old broad resume-switch collision was an emitter unsoundness on that artifact, but it was not proof that 0x8002A5F4 cannot be a host callee.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
