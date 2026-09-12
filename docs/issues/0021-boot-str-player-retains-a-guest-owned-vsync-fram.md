---
id: 21
title: boot STR player retains a guest-owned VSync frame loop before the finite mode driver
status: investigating
symptom: fatal guest VSync at 0x80084BE0 from FUN_8002AA0C return 0x8002AC8C after render seam frame 1
state_items: S002,S004,S013,S018
tags: frame-loop,fmv,vsync,spiderman1,re-22,dynarec,lightrec
created: 2026-08-27
updated: 2026-09-12
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
boundaries and exposed real scheduling-order defects, but offline rewriting of executable guest code
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

The 2026-09-12 direct-runtime boot now delivers ResetGraph's pre-main field and crosses the
measured GPU DMA timeout, inner CdSync, and public CdInit contracts through Lightrec. Its next
typed exit is `VSync(-1)` at return `0x8008D050` inside a later stock libcd command wait
(`0x8008D048` calls `0x80084BE0`). That separate CD-controller boundary must be owned before
the unchanged retail movie player can reach its field exits; the latest run did not reach STR
or `dem1`.

Authenticated executable disassembly identifies the containing routine as stock libcd `CD_cw` at
`0x8008CE8C` (13 direct `jal` sites, including the `CdControl` wrappers at `0x80086CA8`,
`0x80086DE4`, and `0x80086F18`). It copies Setloc's four bytes into `0x800B3B2C..2F` and Setmode's
byte into `0x800B3B30`, then sends the command to the CD register path. Its first `VSync(-1)` at
`0x8008D048` sets a deadline 960 fields ahead in `0x800C6394`; the next at `0x8008D0A0` checks that
deadline while polling completion byte `0x800B3DF0`. The later path calls the libcd interrupt
handler `0x8008C3E0` and registered CD callbacks. These `VSync(-1)` calls are timeout queries, not
display-field/presentation requests. Advancing the frame counter or treating them as ordinary
movie fields would hide the missing command completion.

The shared direct-runtime `PlatformHlePlan::stockCdWorkArea` now lets the existing native command
owner preserve the guest's measured CdLastPos and last-mode bytes without a title-specific command
implementation. Spider declares `CD_cw` at `0x8008CE8C`, position `0x800B3B2C`, and mode
`0x800B3B30`. The shipping `spider1_runtime_services` test dispatches Setloc and Setmode through
that binding, verifies the four position bytes and one mode byte, and proves GetTN's result survives
the separately bound inner CdSync. A direct runtime with no work-area declaration leaves those
guest bytes unchanged. The test also instruments the installed callback addresses and observes zero
invocations: the synchronous command owner retains callback pointers but does not yet assert
equivalent CD IRQ/ready/sync callback delivery. These are synthetic results. An authenticated,
headless, silent retail run after the binding stopped earlier, at the boot movie's
`VSync` frame-boundary PC `0x80084BE0` with return `0x8002AC8C`. It executed 347,812
Lightrec blocks and 1,766,751 instructions with zero interpreter fallback, but did not reach
`CD_cw`; it therefore cannot validate the live command behavior or the older `0x8008D050`
wait. The title now binds VSyncCallback before crt0 and resumes a typed movie field only at the
three authenticated VSync(0) returns. A shipping-path synthetic Lightrec test jumps to the same
VSync entry, crosses each field boundary, delivers the registered callback and one presentation
fence, then executes the next guest instruction exactly once. An unrelated return PC is refused
without changing PC, field count, or presentation fence. An authenticated, headless, silent retail
run with this change then resumed STR field 1 at `0x8002AC8C` and presented its fence. It next
registered the libstr interrupt element but made no further field progress before the host watchdog
stopped it. The watchdog's Lightrec host-side backtrace does not identify the guest PC, so the
immediate stall remains unclassified. This proves one retail field continuation, not a completed
movie or frame loop.

The previous static-product investigation measured why preserving CdLastPos matters: the guest read
path seeds its expected sector from that record and rejected every sector when it stayed stale. The
next discriminator is a bounded debugger observation after the first retail field: record the live
guest PC, StGetNext entries and dry returns, ring producer/consumer indices and slot status, ready
callback slot, and sector-pump count. The preserved `spiderman_install_cd_stream` has no live caller
in the direct runtime, and that runtime declares no ready-callback slot for `Cd::pumpStream`;
these are concrete ownership gaps but do not by themselves prove the next guest PC. The old pump's
dry-poll field wait also requires a finite coroutine that direct boot does not have, so merely
installing it would abort. Establish this boundary before deciding the complete direct-runtime
stream continuation, then measure which command and arguments reach `CD_cw` and whether CD callback
effects are missing. Do not equate a crossed wait with completed STR or `dem1`.

The shipping fix is to execute the unchanged retail movie body through Lightrec and return a bounded
executor exit at `0x8002AC8C`, `0x8002AE1C`, or `0x8002AFEC`. `Spider1FrameDriver` delivers the field,
callback, audio, input, and presentation work, then resumes the same guest CPU state. No generator,
body derivative, interpreter fallback, or conditional successful VSync HLE is permitted.

Acceptance requires both movies and the post-logo wait to complete and early `dem1` to run with
nonzero Lightrec blocks. That closes the first discriminator only. This issue cannot authorize
deleting the old pipeline until S019's representative-gameplay, invalidation, original-call,
independent-oracle, host-performance, and no-interpreter gates pass.
