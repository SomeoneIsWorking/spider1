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

---

## WART-05 — There is no interrupt delivery at all — **OPEN, blocks RE-03**

*What it is:* the framework accepts every interrupt-registration call a guest can make and discards
all of them. `B(19h) HookEntryInt` assigns `hle.int_handler` (`runtime/recomp/hle.cpp:176`) and no
other line in the runtime reads that field — the declaration and the assignment are its only two
mentions. `C(02h)/C(03h) SysEnqIntRP/DeqIntRP` return `$a1` and record nothing. So guest code that
waits on an interrupt-delivered completion waits until its own timeout, every time.

*Why it is a wart and not a Spider-Man fact:* nothing about the gap is game-specific. Any PSX title
whose SDK library completes work in an ISR — libcd, libspu's transfer callbacks, root-counter
callbacks — hits it. The reference consumer does not, only because its native overrides bypass the
libraries that would.

*Not a workaround candidate.* The game-side alternative is to write the completion state the ISR
would have written, from this port's HLE. That is a fake completion by construction (see RE-03: the
handler fills a response block, not just the flag byte), so it is banned here. The fix has to be the
real one, upstream.

*Shape of the fix:* keep the registered handler chain; drive it from wherever the framework already
knows an interrupt became pending — `cdc_native.c`'s queue is the first such site — with `I_STAT` /
`I_MASK` modelled far enough that the handler's own acknowledge sequence clears the condition.
Everything the CD handler needs on the register side is already present (`cdc_state.h`, dispatched
from `mem.cpp:160/204`); the missing piece is only the dispatch into guest code.

**MEASURED 2026-07-28 — the two facts a delivery model needs, both from the run rather than an SDK
header** (`PSXPORT_DEBUG=bios`, which now dumps the element at registration):

1. **`InterruptElement` layout.** Spider-Man registers exactly one element, priority 2, at
   `0x800C1528`, deq-then-enq from `0x80087828`:
   `[+0] next = 0 · [+4] = 0x80087660 · [+8] = 0x800875F8 · [+C] = 0`.
   Disassembly assigns the roles unambiguously by shape: **`+8` is the verifier** — it tests two bits
   and returns `1` or `0` on every path — and **`+4` is the handler**, which is longer and returns no
   flag. So: `{ next, handler, verifier, pad }`.

2. **Verifiers gate on `I_STAT`/`I_MASK`, which this framework does not model at all.** The verifier
   loads its register base from `*0x800B12C4`, and the executable ships that global as
   **`0x1F801070`** — `I_STAT`. It tests `[+4] & 1` (`I_MASK`, IRQ0) then `[+0] & 1` (`I_STAT`,
   IRQ0), i.e. this is libetc's **VBlank** element. `mem.cpp` has no case for `0x1F801070`/`0x74`, so
   both reads return 0 through the unmapped-I/O path and **every verifier rejects unconditionally**.

Note what this changes about the plan: the element that gets registered here is **VBlank**, not CD —
libcd went through `B(19h) HookEntryInt` instead (`0x800B28BC`, from `0x8008B9B8`). A delivery model
therefore has to cover both registration routes, not just `SysEnqIntRP`.

**LANDED 2026-07-28 (psxport, next commit) — the interrupt CONTROLLER, not yet the delivery.**
`I_STAT`/`I_MASK` are now modelled in `mem.cpp` against per-Game state in `Hle`, with the PSX's real
acknowledge semantic (a bit written as **0** is cleared; write-1-to-clear would be wrong). Bit 2 is
latched from a new `CdcState::irq_edge`, set where the controller raises — in `cdc_irq()`, and again
when acking one response makes a QUEUED one current, which is a fresh interrupt rather than a
continuation of the acked one.

*Latching where it is raised, not lazily on the next I_STAT read,* was a deliberate second pass: the
lazy version was correct-looking but **unobservable** in a run where nothing reads I_STAT, which is
exactly this boot. Code that cannot be seen firing is code that has not been verified.

*Verified on real data* (`PSXPORT_DEBUG=irq`, 40 s boot): **152 `CD raised IRQ2` latches**, and the
guest's own `I_MASK` reads **`0x00D`** — bits 0 (VBlank), 2 (CDROM) and 3 (DMA) all enabled. So the
game does want the CD interrupt; the only thing still missing is the dispatch into guest code.

*Still missing, and it is the whole remaining wart:* nothing calls the registered handlers. The chain
from `SysEnqIntRP` is still discarded and `hle.int_handler` is still never read.

*Honesty constraint on the I_STAT work:* the registers are easy to add, but only sources the
framework ACTUALLY models may assert a bit. `cdc_native`'s pending queue is a real source for bit 2.
Bit 0 (VBlank) has no modelled source yet, and asserting it from a free-running timer would be
fabricating an event — the thing this frontier exists to prevent.

---

## WART-06 — The disc resolver hardcoded the reference consumer's env key — **FIXED upstream (psxport 4177ccf3)**

*What it was:* `disc.c`'s `resolve_disc_path()` looked for `PSXPORT_TOMBA2_DISC`, then the generic
`PSXPORT_DISC`, then a `*.chd` drop-in. This port sets `PSXPORT_SPIDERMAN_DISC` (run.sh, `.env`,
README), so **the framework never saw it and every run booted with no media** while the CD model
answered from an empty disc backend.

*Why it went unnoticed for so long:* the only signal was one `cfg_logi` line, `[disc] no disc image
(PSXPORT_TOMBA2_DISC/PSXPORT_DISC/.env)`, sitting in the middle of ordinary boot output at info
level. Nothing downstream failed loudly — `disc_read_sector` just returned 0. This is the
silence-is-not-a-negative trap again, in the framework's own logging.

*Fix:* `GameConfig::discEnvVar` carries the consuming game's key; `Game()` copies it into
`DiscState::env_key`; the resolver tries that first, then the generic key, each from the environment
and then from `.env`, then the drop-in scan. The not-found path is now a **warning** that names the
key it looked for and states that the CD model will run with no media.

*Verified:* `[disc] opened /…/Spider-Man (USA).chd (38783 hunks, 8 frames/hunk)` where the same
command previously logged `no disc image`.

*Caveat on the RE-03 measurements taken before this:* they were all made with no disc mounted. The
RE-03 root cause (no interrupt delivery) is unaffected — the handler is never called either way — but
any future measurement of what libcd *reads back* must be re-taken now that media is present.
