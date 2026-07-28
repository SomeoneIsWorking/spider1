# Framework agnosticism warts found while standing up a second consumer

**Symptom keys:** `REFUSED 0x00000000`, VSync spins forever, `initBuiltins` hooks the wrong function,
`MOVIE/LOGO.STR` not found, a new psxport consumer hangs at boot.

psxport claims to be game-agnostic and mostly is — this port inherited the recompiler, runtime, PSX
hardware, and build unchanged, and the game-side surface is four files. Standing it up against a
*second* game surfaced places where a game-specific literal is still baked into the framework. These
are recorded here rather than worked around silently, because each one costs the next consumer the
same debugging session.

None of these were patched in the submodule from this repo — the fixes belong upstream, and the
pattern for all of them is the one psxport already applied to `recMainLo`/`recMainHi`: route the
value through `GameConfig` instead of baking it in.

---

## WART-01 — `PlatformHle::initBuiltins()` registers the reference consumer's addresses — **worked around**

`runtime/recomp/sync_overrides.cpp` registers the hardware-sync HLE table from hardcoded literals:
VSync `0x80085900`, CdReadSync `0x8008A96C`, CdDataSync `0x8008B4B8`, the CdInit handshake
`0x8008B2D8`, the libgpu DMA-timeout pair, and the task-switch funnel `0x80080880`. Those are
Tomba!2's addresses.

For a different game this is **worse than a no-op in both directions**: it misses every primitive the
new game actually uses, *and* it installs handlers over whatever unrelated functions happen to sit at
those addresses in the new binary — a wrong abort waiting to fire. Spider-Man has real code at
`0x80085900`.

*Root cause:* the addresses are game data living in framework code.

*Handled here by:* not calling `initBuiltins()` at all, and registering this game's own RE'd
primitives through the public `PlatformHle::register_` seam instead (`game/core/sync_native.cpp`).
That seam is public and gates on the BIOS-library address window, so nothing reaches around the
framework. This is a deliberate substitution, not a bypass.

*Cost of not fixing upstream:* every new consumer must know to skip `initBuiltins()`, and nothing
tells them so.

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

## WART-04 — `REFUSED 0x00000000` at startup — **benign, cause not chased**

Nine of these appear before the config dump on every boot: `PlatformHle` is queried with address `0`
and correctly refuses it, since `0` is not in the BIOS-library window.

*Status:* the message is the framework behaving correctly, and it has no observed effect on the run —
it appears before any guest code executes and the boot proceeds normally past it. **Not
root-caused.** Recorded so the next session recognises it as known noise rather than re-investigating
it as a new symptom, and so that if it ever *does* correlate with a failure, there is a note saying
it was never actually explained.
