---
id: C042
kind: claim
status: holds
created: 2026-08-21
tags: jit,bios,interrupt
depends: docs/issues/framework-agnosticism-warts.md#wart-05--cdinit-s-first-two-driver-table-stores-live-in-branch-delay-slots,docs/re-frontier.md#RE-08
reconfirmed: 2026-08-21 14:15:27
verified_at: 2026-08-21 14:15:27
---

## Claim

Spider-Man's HookEntryInt continuation is CdInit's mid-function PC 0x8008B990, which the dynamic runtime must resume as an ordinary guest PC after the modeled BIOS exception exit

## Evidence

The retail CdInit instruction stream calls `setjmp` at 0x8008B988 and resumes at 0x8008B990; the live HookEntryInt buffer at 0x800B28BC independently stores `ra=0x8008B990`. Historical execution failed precisely when the retired dispatcher did not admit that interior PC and progressed when it did. That result establishes the guest continuation address, not a permanent discovery or code-generation rule. The current PSXPort/Lightrec runtime must accept the restored PC through ordinary runtime block discovery and cache ownership.

## What would falsify it

The exact retail disassembly or live `jmp_buf` names a different continuation, or a runtime trace shows the modeled exit resumes at a different guest PC.

## Re-confirmed 2026-08-21 14:15:27

Historical bounded execution crossed the modeled interrupt/CD path into `dem1` and `l1a1` after admitting 0x8008B990. This reconfirmed the continuation address only; authenticated Lightrec execution has not yet re-verified the resume route.
