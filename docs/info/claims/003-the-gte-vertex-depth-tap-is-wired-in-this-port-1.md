---
id: C003
kind: claim
status: holds
created: 2026-07-30
tags: RE-08
---

## Claim

The GTE vertex-depth tap is wired in this port (10 gte_record_pz sites across 6 projection functions) but has NEVER executed: projprim records=0 over a full boot+menu run. The cause is that the port never reaches 3D content, not a broken tap or a closed gate.

## Evidence

grep -h gte_record_pz generated/shard_*.c | wc -l -> 10, in gen_func_8007B628/8007B798/8007B9CC/8007BE90/8007C2AC/80080988. PSXPORT_DEBUG=ndepth over a 100s run (636 lines, f60..f600+) reports 'projprim(vtx) records=0 lookups hit=0 miss=0' every sample. Gate ruled out: attach_enabled() and native_depth_on() both return 1 unconditionally (gte_beetle.cpp:358-360). Plumbing ruled out: gte_record_pz calls projprim.setPz directly with no other condition.

## What would falsify it

if a run that reaches 3D gameplay still reports records=0, the tap IS broken and the 10 sites are the wrong ones -- re-check vertex_pz_stores coverage against the real projection routines
