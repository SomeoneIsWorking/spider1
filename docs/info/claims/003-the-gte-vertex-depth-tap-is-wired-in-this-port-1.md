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

## EVIDENCE DEPENDENCY FLAGGED 2026-08-06 — the instrument behind the `records=0` was caught lying (INST-26)

STATUS DELIBERATELY LEFT `holds`: nothing measured says this claim's CONCLUSION is wrong, and
falsifying it on suspicion would be as unfounded as the reading it corrects. What is now known is
that its EVIDENCE cannot bear the weight it was given.

`PSXPORT_DEBUG=ndepth` (INST-26, DISTRUSTED 2026-08-06) resets `stats`/`projprim` counters on EVERY
present (`gpu_native.cpp:1673` / `:1660`) while reporting only when `s_frame % 60 == 0` (`:1646`).
Each printed line is therefore a ONE-FRAME snapshot with no denominator, and this claim's "636 lines,
f60..f600+" resolve to roughly ten such snapshots — all at the same frame parity. Spider-Man draws on
ALTERNATE FIELDS and 60 is even, so every one of those samples landed on a non-drawing field. On a
field where nothing was drawn, `records=0 lookups hit=0 miss=0` is what the channel prints whether the
tap fired earlier that pair or never fired at all.

So "the tap has NEVER executed" and "the tap executed on the fields I did not sample" produce the same
line, and this evidence cannot separate them. The other two legs of the claim are untouched and still
good: the 10 `gte_record_pz` sites are a static grep, and the gate/plumbing eliminations are reads of
`gte_beetle.cpp` and of `gte_record_pz` itself, neither of which goes through this channel.

**TO RE-ESTABLISH IT** the ndepth report has to carry its denominator and sample by DRAW EVENT rather
than by frame parity (see INST-26), or the same question has to be answered without it — e.g. a
counter on `ProjPrim::setPz` that is never reset and is dumped once at exit. Until then this claim's
own falsifier is not evaluable: a run reaching 3D gameplay would still report `records=0` on a
non-drawing field, so the stated test could not distinguish the two outcomes it was written to
distinguish.
