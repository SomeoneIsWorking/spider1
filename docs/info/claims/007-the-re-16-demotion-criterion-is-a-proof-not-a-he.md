---
id: C007
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

The RE-16 demotion criterion is a PROOF, not a heuristic: a jal-discovered function entry reached by a NON-LINKING branch (bgez/j/beq...) from inside a body cannot be a function, because control arrives with no return address established. On this binary that selects exactly 0x8002A478 and correctly SPARES the four link-branch subroutines (0x8007CD44/D160/D1F0/D254, reached by bltzal/bgezal only) that the branch-and-link discovery fix deliberately seeded.

## Evidence

Scanned all 1672 functions for entries that are branch targets from another body: 5 hits. Full reference sets: 0x8002A478 = bgez x14 + j x1 + jal x3 (mixed, hosts all inside the 0x8002A338 guest body); 0x8007CD44 = bgezal x2; 0x8007D160 = bltzal x8; 0x8007D1F0 = bltzal x2; 0x8007D254 = bltzal x2 (link-only). No prologue on any of them, so the old prologue test could not separate them -- which is why attempts 1-3 demoted 78/325/50.

## What would falsify it

if a game legitimately branches into a real function entry from another body (e.g. a hand-written tail-merge across functions), this over-demotes; check that any newly demoted target has no independent epilogue before trusting it
