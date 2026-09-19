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

An earlier 2026-09-12 direct-runtime boot delivered ResetGraph's pre-main field and crossed the
measured GPU DMA timeout, inner CdSync, and public CdInit contracts through Lightrec. It then
stopped at `VSync(-1)` return `0x8008D050` inside a stock libcd command wait (`0x8008D048`
calls `0x80084BE0`). The later native CD command binding crossed this earlier boundary; that
run did not yet reach STR or `dem1`.

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

The bounded authenticated post-field GDB probe reached one qualifying `BudgetExhausted` exit after
one completed movie field: `guestPc=Core::pc=0x80086B10` at `StGetNext`, return address
`0x8002B3E0`, and 564,488 guest cycles. The 48-slot guest ring was empty at consumer/write index
zero, the ready callback slot `0x800B3B18` held `0x800860B4`, and the host stream was active with
zero sectors delivered. This located the dry poll. The title now installs the StGetNext override
in direct boot, binds the measured ready-callback slot, pumps through the guest's libstr producer,
and yields one typed host field only after the unchanged StGetNext body still reports dry. Focused
Lightrec tests cover dry, ready, and interrupted-callback cases; they do not establish a retail
movie advance.

The next authenticated headless/silent run resumed movie field 1 at `0x8002AC8C`, then stopped at
`VSync` PC `0x80084BE0` with reported RA `0x8002B3E0`, `a0=0x807FFEF0`, 352,889 translated
blocks, 1,798,345 instructions, and zero fallback. Those registers are the *outer* StGetNext
context: shared `Cd::pumpStream` restores its saved R3000 register file before propagating a guest
callback's typed exit. A bounded read-only GDB probe stopped earlier, at the first post-field
`PlatformHle` VSync boundary before that restoration. With one field entered/completed and one
post-field VSync boundary observed, the live values were RA `0x8008CC00`, `a0=-1`, `v0=0`, ready
callback `0x800860B4`, `stream_active=1`, and `stream_delivered=1`. Authenticated code confirms the
chain: the ready callback calls producer `0x80085000`, which calls `0x80086C60` and stock
`CdReady(1, result)` `0x8008CBC4`; its `0x8008CBF8` call is `VSync(-1)` with return `0x8008CC00`.
The libetc body loads the field count at `0x800B397C` for negative arguments and returns without
waiting. This is a counter query inside sector delivery, not another movie field.

The shared `PlatformHlePlan` query contract and Spider's measured counter declaration now return
that guest word for negative VSync arguments, while nonnegative calls retain their protected typed
frame boundary. A missing counter declaration aborts explicitly. A shipping-path synthetic
Lightrec test runs a guest ready callback that calls `VSync(-1)`, observes the returned count,
preserves the interrupted StGetNext registers, retries its original body, and advances only the
later dry-poll host field. The shared missing-counter negative test also passes. The earlier
read-only VSync output remains gitignored at `scratch/logs/spider1-nested-vsync-gdb.log`.

The combined psxport `51df140f`/Spider Clang product crossed that nested query, but a 90-second
headless/silent, normally paced retail run returned movie field 1 only. At a separate 20-second
snapshot, the presenter had 1,029 fences and libetc had 2,056 VBlanks; `Core::pc=0x80086B10`,
`stream_active=1`, `stream_delivered=1,026`, and the 48-slot ring's write, frame-start, and consumer
indices were all zero. `stream_delivered` increments *before* guest callback dispatch, so it counts
INT1-ready attempts, not sectors accepted into libstr. A bounded GDB discriminator stopped at the
first eight **returned** callbacks: 8/8 left producer reason `0x800B1000=3`, all ring indices and
slot 0 status zero, and the CDC FIFO unread (`data_n=2340`, `data_rd=0`, `bfrd=0`). Its first-sector
header was the correct `28 32 54 02` at LBA 128304; the controller's INT1 stayed queued
(`q_head=0`, `q_tail=1`). The 0/8 ring-publication negative is therefore reached and discriminating.
Raw output is gitignored at `scratch/logs/spider1-ring-first-transition.log`.

Authenticated instructions show the first missing transition: producer `0x80085084` calls
`CdReady(1, sp+0x30)`, then `0x800850B0` tests bit `0x04` of the returned result byte. If set,
`0x800850BC` writes reason 3 and returns before any DMA or STR-header check. The direct runtime
does not install the legacy native CdGetSector override; this is the guest's stock CdReady and
controller path. The pending INT1 plus direct host invocation of the ready callback suggests a
shared libcd/CDC IRQ ordering gap. The next focused discriminator must compare the byte copied by
CdReady with the current CDC status and libcd response work area, then prove an INT1 transition
through the guest ISR before invoking the callback. Do not write a title-local ring state or status
value to bypass this guard.

The unchanged retail movie body remains under Lightrec. `Spider1FrameDriver` delivers fields,
callbacks, audio, input, and presentation at the three authenticated VSync(0) return PCs, then
resumes the same guest CPU state. No generated body, interpreter fallback, or title-specific
successful VSync(0) HLE is permitted.

Acceptance requires both movies and the post-logo wait to complete and early `dem1` to run with
nonzero Lightrec blocks. That closes the first discriminator only. This issue cannot authorize
deleting the old pipeline until S019's representative-gameplay, invalidation, original-call,
independent-oracle, host-performance, and no-interpreter gates pass.
