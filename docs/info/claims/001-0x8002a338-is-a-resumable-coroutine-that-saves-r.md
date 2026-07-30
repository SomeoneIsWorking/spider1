---
id: C001
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

0x8002A338 is a resumable coroutine that saves/restores its own register context INCLUDING its continuation ($ra) in a global block at 0x80097D88; jr $ra at 0x8002A460 is a computed jump to an in-body resume point, not a return

## Evidence

disasm of scratch/raw/miss_ram.bin 0x8002A338-0x8002A840: lw $ra,0x30($t6) at 0x8002A390 + bnez $ra at 0x8002A398 restore the continuation; or $at,$zero,$ra at 0x8002A7EC + sw $at,0x30($t6) at 0x8002A834 store it back. Reproduce: python3 external/psxport/tools/disasm.py scratch/raw/miss_ram.bin 0x8002A338 0x8002A840

## What would falsify it

if 0x80097D88 turns out to be written by some other routine as a plain data buffer, or if 0x30($t6) is shown to hold a non-code value at runtime
