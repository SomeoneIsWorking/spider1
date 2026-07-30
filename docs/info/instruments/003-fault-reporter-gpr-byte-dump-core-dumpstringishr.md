---
id: I003
kind: instrument
status: trusted
created: 2026-07-30
---

## Instrument

fault-reporter GPR byte dump (Core::dumpStringishRegs, psxport runtime/recomp/mem.cpp)

## Validated by

POSITIVE: on the RE-16 menu-advance fault it printed the constructed filename buffer at s1/s3=0x807FFDD0 as 'F5 FF 03 26 4B 2E 70 73 78 00' (the .psx suffix visible), which identified the source pointer as 0x80010094 -- the question 17 prior attempts could not answer. NEGATIVE discrimination present in the SAME output: 28 GPRs examined, 23 resolved into mapped RAM (5 rejected as unmapped and named), and registers holding non-pointers print all-zero rather than being silently skipped. LIMITS: only sees memory reachable from a GPR live AT THE FAULT -- a value reached via a stack buffer or struct field whose pointer was already overwritten is invisible. The string CLASSIFIER alone is too strict to rely on: it reported '0 looked like C strings' on the very buffer that mattered, because the name begins with non-printable bytes. Trust the byte dump, not the verdict.

## Known failure modes

(none recorded yet)
