# Framework agnosticism warts found while standing up a second consumer

**Symptom keys:** `REFUSED 0x00000000`, VSync spins forever, `initBuiltins` hooks the wrong function,
`MOVIE/LOGO.STR` not found, a new psxport consumer hangs at boot.

psxport claims to be game-agnostic and mostly is — this port inherited the recompiler, runtime, PSX
hardware, and build unchanged, and the game-side surface is four files. Standing it up against a
*second* game surfaced places where a game-specific literal is still baked into the framework. These
are recorded here rather than worked around silently, because each one costs the next consumer the
same debugging session.

The pattern for all of them is the one psxport already applied to `recMainLo`/`recMainHi` and to the
recompiler seed set: route the value through `GameConfig` instead of baking it in. Two have since
been fixed upstream (WART-01, WART-04) rather than worked around here — the entries are kept because
the reasoning is what makes the next one recognisable.

---

## WART-01 — `PlatformHle::initBuiltins()` registered the reference consumer's addresses — **FIXED upstream (psxport 7c212eb5)**

`runtime/recomp/sync_overrides.cpp` used to register the hardware-sync HLE table from hardcoded
literals: VSync `0x80085900`, CdReadSync `0x8008A96C`, CdDataSync `0x8008B4B8`, the CdInit handshake
`0x8008B2D8`, the libgpu DMA-timeout pair, and the task-switch funnel `0x80080880` — all Tomba!2's
addresses.

For a different game that is **worse than a no-op in both directions**: it misses every primitive the
new game actually uses, *and* it installs handlers over whatever unrelated functions happen to sit at
those addresses in the new binary — a wrong abort waiting to fire. Spider-Man has real code at
`0x80085900`.

*Root cause:* the addresses are game data living in framework code.

*Fixed 2026-07-28, in the framework rather than around it.* `GameConfig` gained an `hle` group
carrying the sync entry points, the two globals the GPU timeout arms, the address windows
`register_()` validates against, and the resident-code range the guest backtrace scans.
`initBuiltins()` now registers only the entries a game declares non-zero, and the framework ships
none of its own. Same remedy as `recMainLo`/`recMainHi` and the seed set.

Two design points came out of it:
  * The VSync trap became **opt-in** (`hle.vsyncTrap`). "Nothing may reach VSync because the native
    frame loop owns all timing" is a port POLICY, true only once such a loop exists. This port runs
    the guest's own loop, so it leaves the trap zero and registers a faithful VSync instead.
  * `register_()` refuses everything when no window is configured, and says so, rather than silently
    accepting any address — which would disable the guard for exactly the games that forgot to
    declare their map.

*The interim workaround (skipping `initBuiltins()` entirely) is GONE* — `main.cpp` calls it normally.

*Verified:* Tomba!2 unchanged (90 headless frames, exit 0, 18 primitives, zero traps/refusals);
framework still builds standalone (`psxport_smoke ok`); this port installs 0 from `hle` (nothing but
VSync is RE'd) plus its own VSync, and reports both.

---

## WART-02 — The boot FMV player hardcodes the reference consumer's movie path — **worked around**

`native_boot_run` plays `MOVIE/LOGO.STR` at boot. This game's movies are under `CINEMAS/`, so the
open fails and emits an error on every boot.

*Handled here by:* `run.sh` defaulting `PSXPORT_NO_FMV=1`. Wiring this game's own FMV path is
`re-frontier` RE-07.

---

## WART-03 — `guest_memset_install()` is dead code carrying a game-specific address — **no action needed**

`runtime/recomp/mem.cpp` defines `guest_memset_install()`, which installs an override at
`0x8009A420` (Tomba!2's guest `memset`). Nothing in the framework calls it — it is defined and never
referenced.

*Why it is recorded anyway:* it makes `RecompRegistry::guestMemset_gen` look mandatory to a new
consumer reading the seam, when in fact `nullptr` is safe and cannot be dereferenced. This port
passes `nullptr`. Verified by grep: `guest_memset_install` has exactly one occurrence in the
framework outside comments — its own definition.

---

## WART-04 — `REFUSED 0x00000000` at startup — **ROOT-CAUSED and FIXED (psxport 3c7f8b14)**

Nine of these appear before the config dump on every boot: `PlatformHle` is queried with address `0`
and correctly refuses it, since `0` is not in the BIOS-library window.

*Root cause, found 2026-07-28:* `cd_override.cpp` passed `cfg->cd*` straight to `register_()`, so
every CD address a game has **not configured** arrived as literal `0` and was correctly refused. This
port's CD group is entirely zero, and there are exactly nine such registrations — hence nine
messages, one per boot.

So it was never framework noise: it was the framework accurately reporting nine attempts to register
address 0. Harmless in effect, but it printed as an ERROR ahead of the config dump and buried the
diagnostics that mattered.

*Fixed upstream:* `cd_override` now skips zero addresses, honouring the same "zero means not
configured / not RE'd yet" convention the rest of `GameConfig` uses.

*Verified:* this port goes from 9 refusals per boot to 0. Tomba!2 is unaffected — all nine of its CD
addresses are non-zero, so every registration still happens (18 primitives installed, unchanged).

*Worth keeping:* the earlier entry recorded this as "benign, cause not chased". It was benign, but
"not chased" was the right thing to write down — it is what made it obvious later that the nine
refusals and the nine unconfigured CD entries were the same nine.
