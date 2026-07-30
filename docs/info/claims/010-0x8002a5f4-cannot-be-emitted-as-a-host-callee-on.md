---
id: C010
kind: claim
status: falsified
created: 2026-07-30
tags: RE-16
falsified_on: 2026-07-30
---

## Claim

0x8002A5F4 CANNOT be emitted as a host callee: on the leak-fixed build all four of its call sites' resume addresses lie INSIDE its own extent, so its jr-$ra resume switch necessarily collides with its callers' host-call links, and the callee double-executes the guest sequence. This -- not an unknown cause -- is the 0x080252D4 fault.

## Evidence

Static, on the emitted attempt-19 artifact, reproduced by an independently written scan: 28 addresses across 7 bodies have a 'case 0x..u:' in a 'switch (c->r[31])' that is ALSO a host-call link ('c->r[31] = 0x..u;' followed by a func_..(c) call). gen_func_8002A5F4 carries 0x8002A77C/784/78C/794 -- exactly the links emitted at L_8002A774 in gen_func_8002A338. Denominators: 102 files, 30 switch sites, 48 distinct case addrs, 16946 link sites / 13354 distinct link addrs. Independently, over the executable: 0x8002A5F4 has 4/4 jal call sites whose site+8 falls inside its extent [0x8002A5F4,0x8002A840). Mechanism: the callee's fast path reaches jr $ra with $ra = a caller link, the switch steals what was a genuine one-level return, runs the rest of the chain inside the callee including the shared tail's frame pop, then default-returns -- leaving the caller to re-execute the remaining calls on a guest that already finished and freed the frame. sp skews 4 bytes per spurious pass and the reloads come from stack words holding bitstream data, which is the observed ra=0x03FF03FF / s0=0x03FF07FF / fp=0x03FF0FFF saturation.

## What would falsify it

If demoting 0x8002A5F4 into 0x8002A338 (closing the collision) leaves the 0x080252D4 fault unchanged, the resume-switch mechanism itself is back under suspicion and the next probe is an entry/exit contract check on gen_func_8002A338 (snapshot sp + s-regs at entry, assert preserved at the real default: return).

## FALSIFIED 2026-07-30

Its own falsifier fired. 0x8002A5F4 was demoted (symbol absent from every shard, verified), the collision it caused is gone, and the 0x080252D4 fault is BYTE-IDENTICAL: ra=0x03FF03FF fp=0x03FF0FFF s0=0x03FF07FF s1=0x47FF03FF, v0=0x080251F4. A second suspect was then eliminated the same way: restricting the in-body link+goto rendering to DEMOTED targets only (so a guest jal site renders identically in every body that carries a copy) took tools/check_resume_switch.py from 25 collisions across 6 bodies to GREEN -- and the fault is still byte-identical. So neither 0x8002A5F4-as-host-callee nor the resume-switch collision causes 0x080252D4. The collision analysis remains CORRECT as an unsoundness proof and the fix is worth keeping; it simply is not this fault's cause.

> Anything that cited this claim as proof must be re-checked. Grep the repo for it.
