---
id: 18
title: STR VLC decode can overrun its output buffer and overwrite the allocator free list before dem1
status: investigating
symptom: Before dem1, FUN_8002A338 can continue decoding past its first 0x25800-byte VLC buffer until its 0x0401 output overwrites the next live free-node link; a later allocator traversal then follows 0x04010401.
tags: boot,allocator,fmv,str,vlc,nondeterminism
created: 2026-08-22
updated: 2026-08-25
---

## Evidence

After clean Clang reconfiguration and a complete passing CTest suite against recorded pin `ad5cf802`, the first 15-second mesh-census gate aborted in 5.8 seconds. `scratch/logs/gate-boot-20260822-180954.log` shows `FUN_80064FA0` reading free-list word `0x04010401` via `FUN_800651C8` while `FUN_8002AA0C` allocates immediately after the missing `TTSLOGO.STR` lookup. The process exited 139; an exact executable-path `ps` check found no survivor.

The deliberately separate 10-second census gate on the same binary did not reproduce the allocator fault. `scratch/logs/gate-boot-20260822-181035.log` reached `dem1` and 16,384 classified face calls with zero unknown callsites, invalid cursor deltas, layout mismatches, or transform mismatches before reproducing issue 0015's supervisor refusal. Exact-path process cleanup again found no survivor.

The fault recurred after regenerating all 1,672 main functions and 30 overlays at recompiler version
`2026-08-22.1`, cleanly configuring Clang against framework `57a17a14`, and passing all eight CTests.
The bounded combined gate
`python3 tools/gate.py boot --seconds 20 --grace 8 --watchdog 30 --debug allocaudit,meshprobe`
stopped after 1.4 seconds in `scratch/logs/gate-boot-20260822-190346.log`. The first watched
external write was:

```
header=801664E4 old-next=00000000 store-address=801664E4 value=00000401 width=2
pc=8002A338 ra=8002A424 sp=807FFEFC allocator-depth=0 irq-active=0
```

The host backtrace resolves that store to `gen_func_8002A338`'s halfword emission at guest
`0x8002A478`, called by `FUN_8002B430` inside `FUN_8002AA0C`. The child exited 139 and an exact
executable-path process check found no survivor.

## What the two captures now prove

`FUN_80064FA0` is the executable's address-sorted free-list insertion/coalescing routine. The older
capture showed it following `0x04010401` while `FUN_800651C8` split the `0x6EF64`-byte probe
allocation. The corrected write watch now chooses the two cases that capture could not: a live
free-node link really is overwritten, and the writer is not an allocator or interrupt. Two
consecutive VLC halfword outputs of `0x0401` explain the later `0x04010401` traversal word exactly.

The allocation sequence and the executable's own caller establish the overrun, not merely the
collateral allocator damage:

- `FUN_8002AA0C` allocates two VLC output buffers of `0x25800` bytes. The first result is
  `0x800FDAC4`; before the first `FUN_8002B430` call, selector `gp+0x69C` is zero and the caller
  passes `gp+0x6DC[0]` to `FUN_8002A338`.
- That allocation ends at `0x801232C4`. The watched write at `0x801664E4` is `0x68A20` bytes from
  the decoder's starting pointer, so the sequential halfword writer had already exceeded its
  buffer by `0x43220` bytes and crossed subsequent allocated blocks.
- `0x801664E4` is also the exact end of the final `0x2D00`-byte MDEC-out buffer allocated at
  `0x801637E4`; the allocator made the address immediately after it the first remaining free node.
  This adjacency explains why the free-list watch fires there. It does **not** make that smaller
  buffer the decoder's intended output.

The decoder's nominal output limit does not protect the allocation. Saved-project Ghidra data reads
retail `0x80097D84` as `0x00FFFFFF`, and its reference database finds exactly one xref: the read in
`FUN_8002A338`. There is no executable writer. The decoder doubles that value and adds it to the
output pointer, so the shipped game deliberately relies on a terminating VLC stream rather than
setting the limit to its `0x25800` allocation.

All four executable callers that directly mutate the sorted lists are now bounded by the opt-in
`PSXPORT_DEBUG=allocaudit` discriminator: heap initialization `FUN_800650C8`, allocation
`FUN_800651C8`, free `FUN_800654E8`, and in-place resize `FUN_80065584`. It verifies both lists at
each boundary, refuses re-entry, and watches every live free-node link outside the scoped retail
mutators (including writes made by a nested IRQ). A first version falsely accused two legitimate
operations and was corrected rather than counted as evidence:

- `scratch/logs/gate-boot-20260822-182908.log`: `FUN_80065584` inserted its split remainder.
- `scratch/logs/gate-boot-20260822-183445.log`: `FUN_800651C8` unlinked the selected node while the
  diagnostic `Core::pc` still named the last nested guest call. Generated straight-line bodies do
  not continuously update `Core::pc`, so allocator ownership must come from the explicit scope.

The corrected live A/B does not attribute the failure to the new face census. With the census off,
`scratch/logs/gate-boot-20260822-183038.log` reached the suspect allocation and pending-IRQ boundary
without an invalid boundary or external free-link write. With the same allocator discriminator plus
`meshprobe`, `scratch/logs/gate-boot-20260822-184214.log` reached frame 1901 and at least 3,320
face-builder calls without either failure. The wrappers themselves only read guest state around
their generated super-calls; their census writes host-side counters. This falsifies a deterministic
"installing the census corrupts the allocator" explanation. It does not prove that logging overhead
cannot change wall-clock scheduling.

A separate compile-time A/B, `SPIDER_BUILD_IRQ_POLL_AUDIT`, wraps the existing framework poll and
compares all 32 GPRs, hi/lo, and `pc` before and after every deferred host/IRQ turn. The ON leg in
the same `184214` run crossed the suspect pending CD interrupt and reached `dem1`/live meshes with
zero differences. The existing `pollregs` channel checked only callee-saved registers; it could not
have distinguished the original `r6/r7` symptom. The new A/B can emit the opposite answer and abort
at the first changed register, but this bounded corpus did not reproduce it.

The gate supervisor is excluded as a writer: it is a parent process and does not share the guest's
address space; its known issue 0015 occurs only while ending the already-running child. The gate was
also corrected to treat `[allocaudit:error]` and `[mem:error]` as failures before its short-progress
refusal, and its 19-case selftest demonstrates that verdict.

The `57a17a14` recurrence ran with `meshprobe` enabled but failed before the first live mesh call.
That makes the census neither the store owner nor a prerequisite for the fault. The store is in the
retail VLC decoder and the observed value is decoded output, while the allocator, IRQ poll wrapper,
and gate supervisor are downstream or out-of-process.

## Root cause / remaining bounded blocker

The immediate root cause of the allocator fault is now exact: `FUN_8002A338` exceeds its first VLC
output allocation and eventually overwrites the adjacent live free-list node. The allocator only
discovers that damage later. Increasing the allocation, masking the link, or retrying the boot would
hide the corrupting producer and is not a fix.

The upstream reason the decoder fails to encounter a terminating code remains unproven. The two
live possibilities are malformed/misordered STR data delivered by the CDC/ring path, or incorrect
saved coroutine state on a decode/resume boundary. The shared-framework timing delta is not assumed
to be causal merely because it can change the pending CD interrupt location, and the successful
bounded legs prove the same executable and disc can take the other path. The next discriminator must
capture the first failing decoder's input-sector identity/header and saved continuation, then compare
them with a matched successful first frame. Issue 0018 therefore stays investigating, and framework
`57a17a14` is not behaviorally cleared by its passing unit suite.

## 2026-08-25 static correction: 0x8002A5F4 is not the non-local return

The emitted `0x8002A5F4` helper was re-examined as a possible host-call boundary defect because its
four callers live inside `FUN_8002A338`. That hypothesis is falsified by the executable CFG. From
entry `0x8002A5F4`, every reachable path joins the already-demoted decoder block at `0x8002A478` and
returns through `jr $ra` at `0x8002A460` to the live link supplied by its caller. The helper cannot
reach the enclosing decoder's save-and-suspend tail at `0x8002A7F4`; reaching that tail requires the
separate `0x8002A424` continuation path. Keeping `0x8002A5F4` as a host callee therefore does not
explain the intermittent overrun.

`tools/callee_contract.py` had encoded the opposite conclusion as `WANT_VIOLATION=0x8002A5F4` even
though its own CFG walk correctly returned `ok`. That stale expected failure has been removed. The
tool remains report-only and its hermetic selftest still proves it distinguishes a balanced leaf,
an unbalanced pop, and a matching save/restore path.

## Next bounded discriminator: callback clock versus sector-ready clock

The remaining malformed/misordered-sector branch now has one precise composition test. A continuous
ReadS starts two independent delivery owners:

- `runtime/recomp/cdc_native.cpp::cdc_drive_service` makes the actual controller FIFO and INT1
  sector-ready event due on the deterministic emulated CPU clock; its shipping test proves zero data
  and zero INT1 at deadline minus one.
- `runtime/recomp/cd_override.cpp::Cd::pumpStream` invokes the guest's ready callback from a separate
  `std::chrono::steady_clock` budget. It can force callback one at elapsed zero and dispatch up to
  `CD_STREAM_MAX_BURST` callbacks after a host stall, whether or not the CDC has produced that many
  sector-ready events.

That split predicts the observed sensitivity to diagnostic overhead: wall time can accumulate while
guest time has not reached another sector deadline, so a burst invokes `FUN_800860B4 -> FUN_80085000`
without a matching FIFO/INT1 arrival. An empty DMA is zero-filled by the controller model; admitting
such bytes to the STR ring can remove the VLC terminator and produce the observed sequential decode
overrun. This is a root-cause candidate, not yet a resolution.

The required hermetic composition test must drive the shipping owners together. The existing CDC
seam is `cdc_bind_tick_source(CdcState*, void*, CdcTickNowFn)` plus `cdc_drive_service(CdcState*)` in
`cdc_state.h`; `tests/cdc_test_clock.h` already supplies its fake. The other shipping boundary is
`Cd::pumpStream(Core*, int)` in `cd.h`, but its `steady_clock` and `rec_dispatch` call are not
injectable yet. The test therefore needs narrow clock/dispatch bindings on `Cd` with production
defaults, rather than another reimplementation of its budget.

Start ReadS through the normal CD owner so `stream_active` and `cdc.drive_event_armed` are both set.
At host time H0 and CDC deadline minus one, call `pumpStream(c, 3)`: the old implementation forces
callback one immediately. Advance only host time to H0 + 20 ms (three double-speed sector periods),
leave CDC at deadline minus one, and pump again: the old budget reaches three callbacks total even
though `cdc_drive_service` still returns zero, `data_n` is zero, `irq_sequence` is unchanged, and
`irq_edge` is clear. The required answer is zero callbacks across both pumps. Then advance the fake
CDC clock to exactly its deadline, require `cdc_drive_service` to produce the one INT1/data event,
and require exactly one callback from the next pump; a second pump without another CDC deadline must
remain zero. This test shows the old and required answers on the same shipping composition.

The implementation owner must then replace the duplicate wall-clock entitlement with a consumable
sector-ready event owned by `CdcState`; whether that event is consumed when INT1 is queued or when it
becomes current must follow the controller's existing IRQ-order contract, not a new title-specific
flag. Candidate files are `runtime/recomp/cd_override.cpp`, `runtime/recomp/cd.h`,
`runtime/recomp/cdc_native.cpp`, `runtime/recomp/cdc_state.h`,
`tests/test_cd_stream_drive_rate.cpp`, `tests/cdc_test_clock.h`, and a new composition test. They
remain untouched while Crash Bash owns the CDC area.
