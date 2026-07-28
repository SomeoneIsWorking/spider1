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

*Status:* open, tracked as `re-frontier` RE-03, which records the specific trap — this does NOT map
onto the framework's generic `hle.cdInitHandshake`, whose handler returns `v0 = 0` where this chain
needs `1`.

*Deliberately left to hang.* A fabricated success return would make the boot appear to progress with
the CD subsystem unconfigured and the three callback pointers null, which is far harder to diagnose
later than a clean stop.
