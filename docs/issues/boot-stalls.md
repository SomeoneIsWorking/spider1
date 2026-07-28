# Boot stalls — symptom-keyed record

Every stall this port has hit at boot, what it actually was, and how it was distinguished. Read this
before investigating a hang; two of the three below look identical from the outside.

**How to tell a spin from slow progress:** the watchdog prints ONE backtrace sample, which cannot
distinguish them. Run two or three times independently and compare — an identical backtrace across
runs is the spin signal. External samplers (`gdb -p`, `eu-stack -p`) do not attach in this
environment; repeated runs are the practical substitute. See `docs/info/instruments.md` INST-03.

---

## STALL-01 — Spins in libetc `VSync` — **SOLVED**

*Symptom:* `[watchdog] STUCK: no frame presented`, backtrace deterministic at
`gen_func_80084D58 <- gen_func_80084BE0 <- ... <- gen_func_8002C354`.

*What it actually was:* `0x80084D58` is libetc's VSync wait helper — confirmed by the string it
emits, `VSync: timeout`. It spins `while (*0x800B397C < target)`, waiting for the vblank ISR to
advance that counter. Nothing in a PC-native runtime advances it, so the loop never ends.

*Root cause:* the framework's hardware-sync HLE table was never populated for this game — see
`docs/issues/framework-agnosticism-warts.md` WART-01.

*Fix:* a native `VSync` registered through the public `PlatformHle::register_` seam
(`game/core/sync_native.cpp`), documented at `re-frontier` RE-02.

---

## STALL-02 — Still spins after the native VSync landed — **SOLVED, and it corrected a wrong model**

*Symptom:* same class of hang, now inside the new native handler's own call chain.

*What it actually was:* **the first implementation was wrong.** It advanced the vblank counter only
inside *blocking* VSync calls. Measurement showed this game almost never blocks — 427,643 query calls
against one blocking call over a 60 s boot — so for a poll-driven caller the counter never moved and
the poll loop spun exactly as before.

*Root cause:* modelling the vblank counter as driven by guest requests rather than by time. On
hardware the ISR advances it on the field clock regardless of what the game asks.

*Fix:* derive the counter from elapsed real time at the NTSC field rate. Recorded as CLAIM-02.

*Lesson worth keeping:* the fix for STALL-01 was correct in mechanism and wrong in model, and the
symptom barely changed between them. Measuring the call pattern — rather than reasoning about what
VSync "should" do — is what separated the two.

---

## STALL-03 — Blocks in the game's disc-init retry loop — **OPEN, and it is the current frontier**

*Symptom:* boot proceeds past graphics init, then never presents a frame.

*What it is — characterised precisely on the clean substrate:*

```
main 0x8002C354 -> 0x8006BF9C -> 0x800649E4     print, wait 100 vblanks, call CdInit, retry while 0
                                   +-> 0x8008A16C  CdInit -> always returns 0
```

`0x8008A16C` (libcd `CdInit`) calls `0x8008A1FC` up to four times and only returns 1 after installing
three CD event-callback pointers. `0x8008A1FC` descends into `0x8008D4E4 -> 0x8008CE8C`, where the
low-level controller handshake spins. Nothing wires this game's CD subsystem, so `CdInit` never
succeeds and `0x800649E4` retries forever — printing through BIOS `A(0x3F)` each pass, which is the
`UNIMPL A0:0x3F` flood in the log.

*Distinguished from STALL-01/02 by:* the vblank counter IS advancing now. This is a genuine wait on
CD state, not a frozen clock.

*Note on the earlier reading of this stall:* it was first recorded as "blocks in libcd" with a
backtrace that descended straight into `0x8008CE8C`. That observation was taken against the
seed-contaminated substrate (CLAIM-00) and the call path it showed should not be trusted. The chain
above is from the clean 1561-function substrate.

*Mechanism, measured:* `PSXPORT_DEBUG=cdc` shows the guest reaching the CD model with command `0x00`
77 times in a 45 s boot, each answered `UNHANDLED cmd 0x00 -> ack only`. That default path enqueues an
INT3, so the IRQ queue never empties and the reset handshake's "loop until the flag register reads
clear" cannot converge. Full reasoning, and the open question about what the correct fix is, in
`re-frontier` RE-03.

*Status:* open, tracked as `re-frontier` RE-03, which also records the specific trap — this does NOT
map onto the framework's generic `hle.cdInitHandshake`, whose handler returns `v0 = 0` where this
chain needs `1`.

*Deliberately left to hang.* A fabricated success return would make the boot appear to progress with
the CD subsystem unconfigured and the three callback pointers null, which is far harder to diagnose
later than a clean stop.

---

## STALL-04 — A callee-saved register is lost across a call — **OPEN; "stack corruption" was my inference, not a measurement**

*Symptom:* a callee-saved guest register is silently zeroed across a call, corrupting whatever the
caller was holding. Surfaced as libcd's command byte arriving at the hardware as `0x00`, but the
mechanism is general and nothing about it is CD-specific.

*Chain of measurement, each step ruling out the previous suspect:*

1. The command-send routine is entered 26x with `a0` = `0x01`/`0x0A`, never 0, yet all 26 of its
   command-register writes are `0x00` (1:1, so it IS that routine's store).
2. The emitted C is faithful at both ends — entry `c->r[17] = c->r[4]`, store
   `c->mem_w8(c->r[2], c->r[17])` — so the recompiler is not dropping the value.
3. The value is lost across a nested call: `0x8008C944` fails to preserve `s1` **25 times**
   (`1 -> 0`, `0x0A -> 0`).
4. That callee's save and restore are BOTH emitted, at the SAME frame offset (28), matching the
   disassembly. So it is not a translation or control-flow fault.
5. ~~Reading the frame slot after the call shows it holds `0` — the guest STACK ITSELF was
   overwritten during the call.~~ **RETRACTED.** That read was taken only AFTER the call, and
   "0 afterwards" was inferred to mean "overwritten". Sampling the same address BEFORE the call as
   well shows it is **`0` before too** — so nothing is observed overwriting anything. The slot simply
   never receives the value.

*The render-path suspicion below is now doubly unsupported* — not only was the A/B confounded, the
corruption it was meant to explain is not in evidence at all.

*SUSPECT (A/B, weak):* `0x8008C944` calls VSync immediately after its register saves, and this port's
native VSync presents frames. Suppressing presentation changed the clobber count over a fixed run
from 22 to 4.

*But that A/B does not establish causation, and a direct test did NOT confirm it.* Suppressing
presentation also changes run timing drastically, so a fixed-window count is not comparable between
the two runs — the difference may be pace, not cause.

A direct check was then run (`PSXPORT_DEBUG=presentwatch`): snapshot a window of guest RAM around the
stack pointer across every present and report any word that changes. Over 1491 presents it reported
**zero guest-RAM writes**, and the window did include the corrupted address.

*That negative is NOT yet trustworthy either, and this is the important part.* The `sp` recorded at
those presents was the CALLER's frame, not `0x8008C944`'s own (which sits ~0x40 lower). So the probe
may simply never have sampled while execution was inside the call that matters — a COVERAGE gap, not
an acquittal. "Zero hits" and "never looked" print identically, which is the failure mode the
instruments ledger exists to catch.

*So the honest state, after removing what I inferred rather than measured:*
  * **Solid:** `0x8008C944` returns with `s1` = 0 when it was called with `s1` = `0x01`/`0x0A`,
    25 times a run. The command byte really is lost across that call.
  * **Solid:** VSync calls made during that call do not change the watched word
    (coverage proven: `vsyncs-covered=2` per occurrence, not an unsampled zero).
  * **NOT established:** that anything overwrites the guest stack. The watched slot reads 0 both
    before and after.

*Most likely remaining explanations, in order of cheapness to test:*
  1. **The watched address is simply wrong.** It is COMPUTED as `(caller_sp - 64) + 28` from the
     callee's frame size and save offset, rather than observed. If the real save lands elsewhere,
     every conclusion drawn from that address is about an unrelated word. Test by determining the
     actual store address instead of deriving it.
  2. The save never executes on the path taken (an early branch past the prologue).
  3. The restore reads a different address than the save wrote.

*MEASURED 2026-07-28 (and it resolves item 1 below):* the callee saves `s1` and THEN calls VSync, so
the value visible at the first VSync inside the call is the saved one. It reads:

```
armed VSync #1: slot[807FFEA4]=00000000 (s1 live=00000001)
```

`s1` is live and correct (`1`) at that point, but the computed slot holds `0`. **So the save does not
land at `(caller_sp - 64) + 28`.** Every statement in this entry about that address describes an
unrelated word, exactly as suspected. The address arithmetic — caller sp `0x807FFEC8`, callee frame
`-64`, save offset `+28` — matches the emitted C and the disassembly, so the discrepancy is NOT in
the arithmetic and that is itself the surprise worth chasing.

*Also corrected:* the previous entry's "VSync calls during the call do not change the watched word
(coverage proven)" was a FALSE NEGATIVE. That check used `cfg_logf` on a channel that was not
enabled in the run, so it could never print. It has been re-run with the channel on. Coverage is
genuinely 2 VSyncs per occurrence, but no conclusion from the earlier silent run stands.

*Next, in order:*
  1. ~~Stop computing the slot address — OBSERVE it.~~ Done: the computed address is wrong, proven
     above. Now find where the save ACTUALLY lands (log the effective address from the emitted store
     itself, not from arithmetic) — and treat a mismatch between that and the disassembly's implied
     address as a possible recompiler frame/ABI defect worth reporting upstream. The emitted save is
     `c->mem_w32((c->r[29] + 28), c->r[17])`; log `c->r[29]` from inside the callee (an override on
     `0x8008C944` that reports sp on entry) and compare against the address the probe assumed.
  2. Only once the real address is known does any statement about that word mean anything.
  3. Do NOT re-derive steps 1-4; those are measured and settled. Step 5 is retracted.
