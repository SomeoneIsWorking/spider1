# Claims ledger — what was "proven", with its evidence and whether it STILL HOLDS

A result cited as proof ("verified", "measured", "matches") rots, and a rotten claim is worse than
none, because work gets built on it. Every claim here carries the evidence it rests on and an
**expiry** — the concrete thing that would falsify it. A claim with no stated falsifier is a belief,
not a result.

When a claim is falsified: mark it, and **grep for who relied on it** — the damage is always
downstream.

---

## CLAIM-01 — The crt0 boot group in `GameConfig` is correct — **holds**

*Claimed:* every value in the crt0/boot group of `game/core/game_config.cpp` is the address the
retail executable actually uses.

*Evidence:* read instruction by instruction out of the crt0 at `0x8008739C` (disassembly reproducible
via `tools/redump_ram.py` + the framework's `disasm.py`; the per-field mapping is written out at the
definition). Independently corroborated: the framework's game-agnostic `crt0_setup` implements this
exact sequence, and the port boots through it into the guest's `main`, which then executes real
translated code with **zero `rec_dispatch` misses**.

*Expires if:* a `rec_dispatch` MISS or a wild guest write appears during crt0 or early `main`, or the
disc used is not the US retail release.

---

## CLAIM-02 — Spider-Man declares no target frame rate; its vblank counter must be free-running — **holds**

*Claimed:* this game does not pace itself with blocking `VSync(N)`. It polls the free-running vblank
counter and does its own timing, so a native `VSync` must advance that counter from real time (NTSC
field rate 60000/1001 Hz) rather than only inside blocking calls.

*Evidence:* direct measurement, not inference. Instrumenting every `VSync` call over a 60 s boot
(`PSXPORT_DEBUG=vsync`, the instrument in `game/core/sync_native.cpp`) yields **427,643 calls of
`VSync(-1)`** — the query form, "return the vblank counter" — against **exactly one blocking
`VSync(0)`**, from graphics init at `ra=0x8008479C`. A blocking-only counter left the poll loop
spinning forever; making it real-time-driven cleared the hang and the boot proceeded into libcd.

*Why it matters:* it settles a design question that would otherwise be guesswork. Any frame-rate or
interpolation decision for this port is a PORT choice measured against the achieved logic rate — it
cannot be read off the binary as an authored intent, because the binary does not state one.

*Expires if:* a blocking `VSync(N>1)` call site is observed at a frame boundary once the game gets
past the CD stall and reaches its real gameplay loop — that would mean the front-end and the
gameplay loop pace differently, and the "no declared rate" conclusion would only hold for the former.
**This is a live risk: the measurement covers boot only, because boot is as far as the port runs.**

---

## CLAIM-03 — Spider-Man has no overlay modules — **holds**

*Claimed:* the recompiler needs exactly one input executable; there is no overlay/stage module set to
extract, and `GameConfig::overlaySlots` is genuinely empty rather than un-RE'd.

*Evidence:* two independent sources agree. The ISO tree (`discdump list`) shows only `SLUS_008.75`,
`SYSTEM.CNF`, `CD.HED`, `CD.WAD`, `COMPILED.XA`, and `CINEMAS/*.STR` — no per-stage `.BIN` images.
The recompiler independently reports `0 overlay module(s)`.

*Expires if:* a guest call resolves into an address range outside the recompiled `.text`
(`0x00010000..0x000C6800`), which would mean code is being paged in from `CD.WAD` at runtime — a
possibility this claim does **not** rule out. It rules out SDK-style overlay *files*, not
archive-resident loadable code.
