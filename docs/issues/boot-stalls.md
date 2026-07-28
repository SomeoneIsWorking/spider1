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

*ROOT CAUSE, established 2026-07-28 — it is not a CD problem either.* libcd's command wait exits on a
completion byte (`0x800B3DF0`) that only its CD **interrupt** handler (`0x8008C3E0`) writes. That
handler is argument-free and reads only the four CD registers the framework already models — but
**psxport never delivers an interrupt to guest code**: `B(19h) HookEntryInt` stores the handler in
`hle.int_handler` and nothing in the runtime ever reads that field, and `C(02h) SysEnqIntRP` records
nothing at all. The wait loop's *polling* fallback is gated on `0x800B2886`, written in exactly one
place, reachable only after a `longjmp` error recovery — so on this port the handler is never called
by either route. Full derivation in `re-frontier` RE-03.

*Status:* open, tracked as `re-frontier` RE-03. The fix is a game-agnostic interrupt-delivery model in
the framework, not a Spider-Man HLE. Note this does NOT map onto the framework's generic
`hle.cdInitHandshake`, whose handler returns `v0 = 0` where this chain needs `1`.

*Deliberately left to hang.* A fabricated success return would make the boot appear to progress with
the CD subsystem unconfigured and the three callback pointers null, which is far harder to diagnose
later than a clean stop.

---

## STALL-04 — A callee-saved register is lost across a call — **RESOLVED (framework: RAM mirroring)**

*Root cause:* PSX main RAM is 2 MB **mirrored four times** across the low 8 MB of each segment.
`host_ptr` masked to `0x1FFFFFFF` and then required the result under 2 MB, so it modelled only the
first mirror. This game ships a stack-top constant of `0x00800000`, so crt0 computes
`sp = 0x807FFFF8` and the **entire guest stack lives in the top mirror**. Every stack access
resolved to NULL, and NULL falls through to `io_write`/`io_read` — an unmapped-I/O path with no
handler for a RAM address — so the accesses were **silently discarded**: stack writes vanished,
stack reads returned 0.

*Fixed* in psxport `94118f85`: RAM-region addresses mask to `0x1FFFFF` before the bounds check, so all
four mirrors resolve to the same storage. An access straddling the wrap still returns NULL rather
than silently aliasing.

*Verified:* lost-register count **25 → 0**. The CD model now receives `cmd 0x01` and `cmd 0x0A` with
**zero unhandled** (was `cmd 0x00` x26, all unhandled). Tomba!2 unaffected — its stack sits inside
the first 2 MB, so it never exercised the mirror.

*How far the symptom was from the cause:* dropped stack writes → a callee-saved register saved as 1
and restored as 0 → libcd sending command `0x00` instead of Getstat/Init → `CdInit` failing forever →
the boot stalling in the game's disc-init retry loop. Nothing at the symptom end pointed at memory
mapping.

*Wrong attributions made along the way, kept deliberately as a record of what misled:*
a recompiler mistranslation; the render/present path; guest-stack corruption. **Each was stated with
more confidence than the evidence carried, and each died to a properly covered measurement.** The
common failure was reading silence as a negative result — three separate false zeros
(`go_public` with no history, `PSXPORT_WWATCH`, a channel-gated probe). That is now a RULE at the top
of `docs/info/instruments.md`.

*Residual worth fixing upstream:* a guest access to an unmapped address is dropped with no
diagnostic. A channel-gated warning when `io_write`/`io_read` falls through with no handler would
have named this in one run instead of a session.

---

