---
id: C009
kind: claim
status: holds
created: 2026-07-30
tags: RE-16
---

## Claim

The garbage filename that kills the RE-16 boot is a leaked RETURN ADDRESS (0x80010094), not a mis-computed name-table pointer -- it is the $ra pushed by 'jal 0x80017A84' at 0x8001008C inside the script VM dispatcher 0x80010080, picked up by a callee-saved register restored from a stack slot shifted by the measured 4-byte-per-call leak.

## Evidence

Fault reporter byte dump: s1/s3=0x807FFDD0 -> 'F5 FF 03 26 4B 2E 70 73 78 00'. The word 0x2603FFF5 occurs EXACTLY ONCE in all 2MB of guest RAM, at 0x80010094. 0x8005F2A8 is strncpy(src=a0,dst=a1,max=a2), and 0x80069B20 'move $a0,$s3' makes $s3 the SOURCE. Producer path ruled out on IMMUTABLE data: both 'jal 0x8005c7ec' sites in 0x8005AEC0 compute a0=record+2 only after 'lh' at record matches tag 0xF (0x8005AF20) or 4 (0x8005AF7C); a0=0x80010094 requires record=0x80010092, where the halfword in TEXT is 0x0080 -- so neither branch could have been taken. 0x80010094 == 0x8001008C+8 exactly. 0x8005C7EC calls the VM directly at 0x8005C974 (jal 0x80010008), which is what puts that $ra on the stack.

## What would falsify it

If a run shows the record array at gp+0xBCC actually containing a pointer whose tag halfword is 0xF or 4 AND whose +2 is 0x80010094, the producer path is back in play. Also falsified if fixing the 4-byte leak leaves this exact fault unchanged.
