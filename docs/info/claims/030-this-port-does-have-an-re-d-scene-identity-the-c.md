---
id: C030
kind: claim
status: holds
created: 2026-08-06
tags: scene,classifyscene,re-13,re-18,re-23,identity
depends: docs/re-frontier.md
---

## Claim

This port DOES have an RE'd scene identity: the current level-name string at 0x800A568C, encoded by FUN_8005A734 into (level<<8)|sub — the same values the game's own per-frame state machine FUN_80062CE0 switches on. C026's conclusion that there is nothing to classify on generalised from the wrong source.

## Evidence

STATIC RE 2026-08-06, Ghidra headless. FUN_80062CE0 is called twice per frame from the render walk FUN_8002BD5C and switches on FUN_8005A734()'s return over the constants 0x201, 0x202, 0x301, 0x302, 0x401, 0x501, 0x502, 0x503, 0x604, 0x701, 0x702, 0x704, 0x803 plus the ranges 0x100..0x105 and 0x505..0x508. FUN_8005A734 reads a short ASCII buffer at 0x800A568C: byte[0]=='d'/'D' selects 0x99, otherwise byte[1] is folded from '0'-'9' / 'A'-'Z' / 'a'-'z' into a level index and byte[3]-'0' becomes the low byte, returning (index<<8)|sub. The same 0x800A568C buffer is what main() FUN_8002C354 hands to FUN_8005F1D4 / FUN_80018898 / FUN_80018800 on its case-3 path, alongside the string literals at 0x800B4FD8 and 0x800B4FE0 — i.e. it is the CURRENT LEVEL NAME, written by the mode switch.

## What would falsify it

This is STATIC ONLY — no run has read 0x800A568C or logged FUN_8005A734's return. It is falsified if a runtime census over boot + attract + a loaded level shows the string constant (or the encoded value constant) across scenes that must differ, i.e. if it discriminates no better than the module registry did. Do that census before building classifyScene on it.
