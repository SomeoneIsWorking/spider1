---
id: 21
title: boot STR player retains a guest-owned VSync frame loop before the finite mode driver
status: investigating
symptom: fatal guest VSync at 0x80084BE0 from FUN_8002AA0C return 0x8002AC8C after render seam frame 1
state_items: S002,S004,S013,S018
tags: frame-loop,fmv,vsync,spiderman1,re-22,dynarec,lightrec
created: 2026-08-27
updated: 2026-09-27
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
timeout: the public `0x8008A16C` boundary now completes through the synchronous host CD owner
and the launch advances beyond it.

## What was tried / dead ends

Do not make `VSync` succeed conditionally for these three return addresses. That would hide the
guest-owned loop behind a title exception while leaving cadence ownership in retail code. Do not
skip the intro movies merely to reach the menu; issue 0004 has prior real-disc evidence that both
shipped logos decode and display when their service dependencies are correct.

## The SLUS_008.75 libetc VSync contract, recovered from the image (2026-09-27)

Whole-image Ghidra reference census of `0x80084BE0`: **32 call references, 0 non-call**, so every
caller in the image is one of the 32 sites below and none is reached through a register or a table.
Recovered from `0x80084BE0..0x80084DEC` by disassembly, not inferred from X4's shape.

### The measured state words

| word | read by | written by | what it is |
|---|---|---|---|
| `0x800B0FA0` | `0x80084BE0`, `0x80084C9C`, `0x80084CD0` | — (a cell) | holds `0x1F801810` (GPUSTAT) |
| `0x800B0FA4` | `0x80084BE8`, `0x80084C14`, `0x80084D10` | — (a cell) | holds `0x1F801810`-class counter 1 / HBlank clock |
| `0x800B0FA8` | `0x80084C2C` | `0x80084D24` | VSync's own previous counter sample |
| `0x800B0FAC` | `0x80084C68`, `0x80084C80` | `0x80084D18` | the field counter at the last completed sync |
| `0x800B397C` | `0x80084C44`, `0x80084CAC`, `0x80084D08`, and the wait helper `0x80084D68`/`0x80084DCC` | the title's field owner | the VBlank field counter the whole contract is expressed in |

**The two entry spins, read from the image.** `0x80084C04..0x80084C20` spins until the HBlank-clocked
counter cell's contents CHANGE (`bne v1,v0` against a saved sample) — a horizontal wait, not a
field. `0x80084D1C..0x80084D38` is the same spin after the bookkeeping. Neither costs a display
field, and neither is a `FrameBoundary`.

### The arms, and the per-mode field count

```
0x80084BE0  entry
0x80084C04  spin: HBlank counter changed            (not a field)
0x80084C38  bgez a0 -> 0x80084C50                   ; a0 >= 0
0x80084C40    a0 < 0: v0 = [0x800B397C]; j 0x80084D44     RETURN the VBlank count, no wait, no store
0x80084C50  li v0,1 ; beq a0,v0 -> 0x80084D40      ; a0 == 1
0x80084D40    v0 = s1 (the entry sample); jr ra     RETURN, skipping BOTH waits AND the two stores
0x80084C5C  blez a0 -> 0x80084C7C                  ; a0 <= 0
0x80084C64    a0 > 0: v0 = [0x800B0FAC] - 1 + a0   ; target = lastSync + a0 - 1
0x80084C7C  a0 <= 0: v0 = [0x800B0FAC]             ; target = lastSync
0x80084C84  blez a0 -> 0x80084C90 / a1 = a0 - 1     ; the helper's SPIN BUDGET
0x80084C90  jal 0x80084D58                          ; WAIT 1: target field count, budget
0x80084C98  v0 = [0x800B0FA0]; s0 = *v0             ; re-sample GPUSTAT
0x80084CA8  a0 = [0x800B397C] + 1 ; a1 = 1
0x80084CB4  jal 0x80084D58                          ; WAIT 2: one field past the counter
0x80084CBC  and v0,s0,0x40 ; beq -> 0x80084D04      ; GPUSTAT retrace gate
0x80084D04  [0x800B0FAC] = [0x800B397C]             ; store 1: lastSync
0x80084D1C  [0x800B0FA8] = *counter ; spin changed  ; store 2: previous sample
0x80084D40  v0 = s1                                 ; the return value for a0 != 1
0x80084D44  epilogue
0x80084D58  the wait helper: a1 <<= 15 (SPIN BUDGET), spin [0x800B397C] < a0, on exhaustion
            print "VSync: timeout\n" (0x80096020) and exit(3)
```

`a1` is the helper's spin budget, never a second field count: `0x80084D5C sll a1,a1,0xf` and
`0x80084D98 bne v0,v1` against -1. So the field count of a positive mode is entirely in `a0`.

- **`a0 < 0`** — a QUERY. Returns `[0x800B397C]`, waits for nothing, changes nothing. This is the arm
  `0x8008CBF8` and `0x8008CC44` (stock `CdReady`) and `0x8008D048`/`0x8008D0A0` (stock `CD_cw`) use, and
  the arm the title's `startGpuDmaTimeout` replaces at `0x80083C68`.
- **`a0 == 1`** — returns the entry sample immediately, skipping BOTH waits AND both stores.
- **`a0 == 0`** — WAIT 1 targets `lastSync`, which the counter has already passed, so it costs
  nothing; WAIT 2 targets `counter + 1` and costs **ONE field**.
- **`a0 >= 2`** — WAIT 1 targets `lastSync + a0 - 1`, costing `a0 - 1` fields; WAIT 2 costs one more.
  **`a0` fields in total.**

So `fields(a0) = 0` for `a0 < 0`, `0` for `a0 == 1`, `1` for `a0 == 0`, and `a0` for `a0 >= 2` —
which is X4's `fieldsForMode` re-derived from this image rather than copied from it. The two images
agree on the rule and differ on the words, which is the point of recovering it here.

### What each of the 32 call sites passes

Ghidra's reference model gives the site; the `$a0` producer at each is read from the disassembly.

| caller | return PC | `$a0` | meaning |
|---|---|---|---|
| `FUN_80065708` | `0x8006593C` | 0 | allocator-failure report |
| `FUN_80089ECC` | `0x80089F08`, `0x80089F20`, `0x8008A010` | -1 | stock libcd sync/ready waits |
| `FUN_80089CE4` | `0x80089D40`, `0x80089D6C`, `0x80089EA4` | -1 | stock libcd |
| `FUN_8008A068` | `0x8008A098`, `0x8008A0C8` | -1 | `CdReadSync` |
| `FUN_8008C944` | `0x8008C978`, `0x8008C9C4` | -1 | inner `CdSync` |
| `FUN_8008CBC4` | `0x8008CBF8`, `0x8008CC44` | -1 | `CdReady` |
| `FUN_8008CE8C` | `0x8008D048`, `0x8008D0A0` | -1 | `CD_cw` command wait |
| `FUN_80014F00` | `0x80014F4C` | — | game field wait |
| `FUN_80084778` | `0x80084794` | 0 | `ResetGraph` pre-main field |
| `FUN_8002AA0C` | `0x8002AC8C`, `0x8002AE1C`, `0x8002AFEC` | 0 | the three retail STR fields |
| `FUN_800649E4` | `0x80064A14`, `0x80064A6C`, `0x80064A44`, `0x80064ACC`, `0x80064AF0` | — | game's own field waits |
| `FUN_80083C60` | `0x80083C68` | -1 | GPU DMA timeout arm (already owned) |
| `FUN_80083C94` | `0x80083C9C` | — | GPU DMA timeout poll |
| `FUN_8008D6C4` | `0x8008D6E4`, `0x8008D72C` | -1 | stock libcd |
| `FUN_800899A0` | `0x80089B08`, `0x80089B34`, `0x80089B68` | -1 | stock libcd |

**17 of the 32 sites pass a negative mode**, i.e. they are counter queries, not field requests. That
is the measured census, and it is why "a `VSync(0)` trap" was the wrong shape to begin with: the
protected entry aborts on the *positive* arm while most of the image's traffic is the negative one.

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
equivalent CD IRQ/ready/sync callback delivery. These are synthetic results. An authenticated, headless, silent retail run after the binding stopped earlier, at the boot movie's
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
preserves the outer registers, retries its original body, and advances only the later dry-poll host
field. The shared missing-counter negative test also passes. The earlier
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

## 2026-09-27: which blocker this actually is — the CdReady result-bit guard, not the VSync abort

**Measurement that decided it.** A fresh authenticated Clang run of the CURRENT tree,
`scratch/dem1/probe_boot.log`, against psxport `e0485d33` with `PSXPORT_NATIVE_FRAMES=3000`:

```
[frame] Spider-Man 1 completed pre-main ResetGraph field at 0x8008479C
[cd] CdRead 1 sector(s) x 2048 bytes from LBA 128303 -> 0x800C0CE0 (mode 0x80)
[str] Spider-Man 1 resumed retail STR field 1 at 0x8002AC8C
[ring] base=0x80148AD4 slots=48 frameStart=0 cons=0 writeIdx=0 cdIrqSeq=1 cdIrqType=1 cdDataRead=0
[present_shot] wrote present_1100.png (960x720) non-black 0/691200 (0.00%)
[present_shot] wrote present_2200.png (960x720) non-black 0/691200 (0.00%)
[present_shot] wrote present_2900.png (960x720) non-black 0/691200 (0.00%)
```

`VSync` is **not** the wall any more. Field 1 at `0x8002AC8C` is crossed and presented, the
`LBA 128303` stream start is read, an INT1 is queued (`cdIrqSeq=1 cdIrqType=1`), and every one of
the three captures is 0.00% non-black. The run is not aborting at `0x80084BE0`; it is making no
further field progress while the guest's own libstr producer refuses every sector. That is the
CdReady result-bit guard, and it is a **different bug** from a boot abort.

### The guard, read out of the image

Producer `0x80085084` calls `0x80086C60`, whose whole body is a tail to stock `CdReady` `0x8008CBC4`.
That returns the guest's own queue-discipline byte, which the producer then tests at `0x800850B0`:

```
80085084  jal 0x80086C60          ; a0 = 1, a1 = sp+0x30 (the result buffer)
8008508c  li v1, 0x5
80085090  beq v0, v1, 0x8008590c  ; CdReady returned 5 -> give up
80085098  lbu v0, 0x30(sp)        ; the response byte
800850a8  lhu v0, 0x22(sp)
800850b0  andi v0, v0, 0x4        ; THE GUARD
800850b4  beq v0, zero, 0x800850c8
800850bc    [0x800B1000] = 3      ; reason 3, and return before any DMA or STR-header check
```

**What that byte is.** `CdReady(1, buf)` fills `buf` from the 8-byte response the guest's own libcd
ISR staged. The staging is at `FUN_8008C3E0`, and every write of the three status bytes is a
literal, read from `0x8008C730..0x8008C888`:

- `0x8008C750` writes `[0x800B3DF0] = 2` (or 5 at `0x8008C74C` when `s1 != 0`), then copies the
  response into `0x800C637C`.
- `0x8008C7B8` writes `[0x800B3DF1] = 1` (or 5 at `0x8008C7AC`), then copies into `0x800C6384`.
- `0x8008C824` writes `[0x800B3DF2] = 4` and mirrors it into `[0x800B3DF1]`, then copies into
  `0x800C638C`.
- `0x8008C8A4` writes `[0x800B3DF1] = 5` and mirrors it into `[0x800B3DF0]`, then copies into
  `0x800C6384`.
- `0x8008D3AC..0x8008D3B8` (the sync path) clears `[0x800B3DF2]` and mirrors it into `[0x800B3DF1]`.

**So the byte the producer tests is a QUEUE-DISCIPLINE COUNTER, not a status flag.** It is the
*depth of the one-deep response queue* the libcd ISR maintains, and it counts down as the consumer
drains it. `0x04` on that byte does not mean "the drive is not ready"; it is a queue the guest has
backed up. A host that raises that byte is claiming the guest's response queue is four deep, and
the producer's refusal is then correct behavior.

**First: the guard was the wrong target, and the measurement says so.** `cdready` on the current
tree reports producer reason `0x800B1000 = 0` and a CLEAR `0x04` across 200,000 polls in 183 seconds.
The 8/8 reason-3 result was real when it was taken, against an older framework revision; it is not
what this tree does now. Nothing further should be built on the guard hypothesis.

**Second, and this one is real and title-owned: the CD interrupt was never armed.** See
`Spider1FrameDriver::armCdInterrupt` in `titles/spiderman1/spider1_frame_driver.cpp`. The retail
`CdInit` body reaches the B-vector interrupt-enable thunk `0x8008B86C` with `a0 = 2`
(`0x8008A17C` -> `0x8008A1FC` -> `0x8008D4E4` -> `0x8008D54C`); the title's override replaced that
body and dropped the arm, so `Hle::irqPoll` computed `i_stat & i_mask = 0x004 & 0x009 = 0` and the
guest's own registered CD element `0x800C1528` (handler `0x80087660`) never ran. Fixed, and measured:
`iMask 0x009 -> 0x00D`, IRQ2 delivered, the guest leaves the `StGetNext` spin.

**Third, still open — the sector handoff.** After the arm, the controller still holds `dataAvail=2340`
with `dataRead=0`, the guest's staged response is `0x00`, and the ring indices stay zero while the
guest sits in a further CD wait at `0x8008DCC8(0x190)`. The next discriminator must read what the
guest's libcd ISR `FUN_8008C3E0` saw when it staged its 8-byte response: it reads those bytes from
the cell `0x800B3DDC` (naming `0x1F801801`) at `0x8008C45C..0x8008C498`, but only after testing bit
`0x20` of the cell `0x800B3DD8` (naming `0x1F801810`) at `0x8008C470`. **That gate is suspicious and
unresolved:** `runtime/psx/mem.cpp` answers a guest read of `0x1F801810` with `gpu_read_word()` under
a comment calling it GPUREAD, and answers `0x1F801814` with a GPUSTAT-shaped `0x1C000000 | toggle`
that cannot set bit `0x20` at all. On PSX hardware `0x1F801810` is GPUSTAT and `0x1F801814` is
GPUREAD. If that mapping is inverted framework-wide, every guest that gates work on a GPUSTAT bit —
this STR reader included — reads the wrong register, and the fix belongs in the framework, not here.
This session did not confirm it and deliberately did not change it.

**Fourth, framework-owned and reported, not worked around.** `Cd::pumpStream` dispatches the guest's
ready callback from a host steady-clock budget (`runtime/psx/cd.h`: `stream_t0_ns` /
`stream_delivered`, `CD_STREAM_MAX_BURST`), while `cdc_drive_service` (`runtime/psx/cdc_native.cpp`)
makes the controller's INT1 sector-ready event due on the emulated CPU clock. Two independent delivery
owners for one sector. Related: `GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt` promises
that the controller raises INT1 and the guest libcd ISR consumes it, but nothing in the framework arms
the guest's CD interrupt mask — a title that replaces `CdInit`, as this one does, has to supply that
arm itself or the promise is silently unkept. Issue 0018's "callback clock versus sector-ready clock"
section already names the composition test and says the `Cd` clock and `rec_dispatch` calls are not
yet injectable.

Acceptance requires both movies and the post-logo wait to complete and early `dem1` to run with
nonzero Lightrec blocks. That closes the first discriminator only. This issue cannot authorize
deleting the old pipeline until S019's representative-gameplay, invalidation, original-call,
independent-oracle, host-performance, and no-interpreter gates pass.
