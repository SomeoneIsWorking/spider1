---
id: I004
kind: instrument
status: trusted
created: 2026-07-30
---

## Instrument

resume-switch / host-call-link collision scan over generated/*.c

## Validated by

POSITIVE: run over the attempt-19 substrate it reports 28 colliding addresses across 7 bodies (gen_func_8002A5F4, 8007CC88, 8007D0D0, 8007D978, 8007DDC8, 8007EF80, 8007F188) -- independently reproducing a result derived by a separate analysis, address for address. CAUGHT LYING ONCE, and this is the reason to record it: the FIRST version required the link assignment and the call to be on ONE line, but emit_control appends them as TWO lines, so it matched 18 links instead of 16946 and printed 'COLLISIONS: 0'. That false negative was caught ONLY because the scan prints its denominators (link sites / distinct link addrs / case addrs) next to the verdict -- a bare '0 collisions' would have read as proof of soundness and killed a correct diagnosis. It now asserts the link set is non-empty and aborts rather than reporting a void negative. LIMITS: operates on the EMITTED C, so it can only see what a given emission produced -- it cannot predict collisions for a configuration that has not been emitted, and it says nothing about bodies with no resume switch at all.

## Known failure modes

(none recorded yet)
