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

## STALL-03 — Blocks in libcd — **OPEN, and it is the current frontier**

*Symptom:* boot proceeds past graphics init, then blocks with the chain
`0x8002C354 -> 0x8006BF9C -> 0x800649E4 -> 0x8008A16C -> 0x8008A1FC -> 0x8008D4E4 -> 0x8008CE8C`.

*What it is:* `0x8008CE8C` polls the vblank counter as a timeout while waiting on a CD operation that
never completes. Distinguished from STALL-01/02 by the fact that the counter *is* now advancing —
this is a genuine wait on CD state, not a frozen clock.

*Status:* open, tracked as `re-frontier` RE-03. This game's CD sync primitives have not been
identified. Deliberately left to hang rather than stubbed: a fake completion here would make the boot
appear to progress while feeding the game garbage, which is far harder to diagnose later than a
clean stop.

*Note for whoever picks it up:* the data layout is a single packed archive (`CD.WAD`), not
SDK-file-per-asset, so do not assume the loader resembles the reference consumer's.
