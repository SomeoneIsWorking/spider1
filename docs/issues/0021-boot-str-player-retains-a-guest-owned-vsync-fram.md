---
id: 21
title: boot STR player retains a guest-owned VSync frame loop before the finite mode driver
status: investigating
symptom: fatal guest VSync at 0x80084BE0 from FUN_8002AA0C return 0x8002AC8C after render seam frame 1
state_items: S002,S004,S013,S018
tags: frame-loop,fmv,vsync,spiderman1,re-22,dynarec,lightrec
created: 2026-08-27
updated: 2026-09-04
---

## Root cause

`Spider1FrameDriver::runBootPrefix` still dispatches the whole retail boot sequencer
`FUN_8006BF9C` before installing `Spider1ModeDriver`. That sequencer calls the sole STR player
`FUN_8002AA0C` for logo IDs 0 and 1. The player owns a persistent decode loop and directly calls
libetc `VSync(0)` at return PCs `0x8002AC8C` (initial display), `0x8002AE1C` (each decoded frame),
and `0x8002AFEC` (teardown). The protected `0x80084BE0` trap therefore aborts correctly before the
finite native outer/mode driver can be reached.

This is not the expected missing `/CINEMAS/TTSLOGO.STR;1`: the authenticated game pre-scans all 24
movie entries and deliberately records zero for absent files. It is also not the earlier CdInit
timeout: the public `0x8008A16C` boundary now completes through the synchronous host CD owner and
the launch advances beyond it.

## What was tried / dead ends

Do not make `VSync` succeed conditionally for these three return addresses. That would hide the
guest-owned loop behind a title exception while leaving cadence ownership in retail code. Do not
skip the intro movies merely to reach the menu; issue 0004 has prior real-disc evidence that both
shipped logos decode and display when their service dependencies are correct.

## Prior generated-path discriminator

The retired product used a build-time derivative of `FUN_8002AA0C` that replaced exactly the three
authenticated VSync calls with `Spider1FrameDriver` fiber yields. That established the field
boundaries and exposed real scheduling-order defects, but offline rewriting of a generated guest body
is not the target architecture. It is preserved here only as evidence about behavior.

The first bounded real run, `scratch/logs/finite-str-wide-20260827.log`, crossed the former
`0x8002AC8C` abort and reconciled 2,600 host frames with no VSync timeout. It did not prove the fix:
all seven present captures were visually black and boot never completed. The measured cause was
ordering in the new owner: it paused guest work during presentation pacing, so the elapsed-time host
timer immediately yielded again at the next guest function entry and starved decode. Bounded teardown
also canceled the fiber before stopping that timer, producing an exit-only SIGSEGV in
`host_turn.cpp::timer_main` after frame-loop completion. Both orderings were corrected. Mode-local guest-stack frames are now trivially destructible with
explicit normal-path restoration, so `Coro::cancel` cannot `longjmp` across their destructors when a
later in-mode movie is field-blocked at bounded shutdown.

The subsequent product runs found and fixed three further ownership defects instead of weakening
the VSync trap:

- the elapsed-time bootstrap host-turn remained armed after its one required handoff and could
  re-yield the movie fiber before decode reached an authenticated STR field boundary;
- a dry `StGetNext` poll could hold one host step while the console's display/SPU would have kept
  advancing asynchronously, eventually backpressuring the XA ring; the title's stream boundary now
  preserves the real "not ready" result and yields that waited field;
- stock libcd's inner `CdSync` body at `0x8008C944` contains `VSync(-1)` timeout polls even though the
  host CD operation is already complete. It now uses the same synchronous complete/ready contract
  as the public stock wrapper. After the movies, the authenticated init loop
  `0x8006C2FC..0x8006C35C` calls pad service `0x8006B514` once per field while waiting 300 fields or
  input; its exact return site `0x8006C304` now yields to the native field owner after super-calling
  the complete retail pad-service body.

Real-disc Clang evidence: `scratch/logs/spider1-postlogo-owned-live.log` completed both logo calls
(204 and cumulative 423 STR fields), completed the finite boot prefix, entered `dem1` at host frame
4941, reconciled all 5,400 frame fences, and exited 0 with no guest VSync violation. Visual captures
were inspected: `present_4400` contains the second logo imagery and `present_5200`/`present_5400`
contain the live `dem1` characters. The latter still have sparse black background output; that is a
remaining scene/rendering gap, not part of this now-resolved cadence issue.

Evidence: `scratch/logs/gate-boot-20260827-022834.log` reaches render-seam call 1 / frame 1, reports
the expected TTSLOGO miss, then aborts at `VSync` with `ra=0x8002AC8C`; the native outer-dispatcher
ownership line is correctly absent.

## Open native/Lightrec resolution

The shipping fix is to execute the unchanged retail movie body through Lightrec and return a bounded
executor exit at `0x8002AC8C`, `0x8002AE1C`, or `0x8002AFEC`. `Spider1FrameDriver` delivers the field,
callback, audio, input, and presentation work, then resumes the same guest CPU state. No generator,
body derivative, interpreter fallback, or conditional successful VSync HLE is permitted.

Acceptance requires both movies and the post-logo wait to complete and early `dem1` to run with
nonzero Lightrec blocks. That closes the first discriminator only. This issue cannot authorize
deleting the old pipeline until S019's representative-gameplay, invalidation, original-call,
independent-oracle, host-performance, and no-interpreter gates pass.
