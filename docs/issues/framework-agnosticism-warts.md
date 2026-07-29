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

## WART-02 — The boot FMV player hardcoded the reference consumer's movie path — **FIXED upstream**

`native_boot_run` played `MOVIE/LOGO.STR` at boot — the first consumer's file, which does not exist
on this disc — so the open failed and emitted an error on every boot.

*The workaround is gone, not just the error.* This was previously "handled" by `run.sh` defaulting
`PSXPORT_NO_FMV=1`, which suppressed the message by disabling FMV wholesale — a strictly worse state,
because it also masked any real FMV problem. Both the hardcoded path and that default are now removed.

*Fix:* `GameConfig::bootFmv` — a NULL-terminated, ordered list of boot movies. The framework plays
what the game names and nothing otherwise; an all-null list is a real answer ("this game's boot plays
no movie natively"), so it logs at info rather than warning. The native `.STR` player itself needed
no change: it resolves ISO9660 paths and decodes BS/MDEC entirely in framework code, so it was never
the game-specific part.

*This port leaves the list empty, deliberately.* Spider-Man's boot runs on the recompiled substrate,
so the GUEST plays its movies — the framework must not invent an intro it was never asked for.

*Verified:* with FMV explicitly forced on (`PSXPORT_NO_FMV=0`), the boot now reports
`no boot FMV configured (GameConfig::bootFmv is empty) — nothing to play` where it previously
reported `could not resolve MOVIE/LOGO.STR on disc`. `psxport_smoke` still links standalone.

*What this port DID establish about its movies* (recorded at the `bootFmv` definition, with
provenance): a 24-byte-stride descriptor table at `0x80097DEC` —
`{ +0 path, +4 u16 w, +6 u16 h, +8 u16 frames, +0xC u32 frameBytes, +0x10 u8 flag }` — read out of
the indexing routine at `0x8002B0F4`. Self-consistent: `CINEMAS/ATVILOGO.STR` is 320×240 with
`frameBytes = 0x25800 = 320*240*2`. **Which ID the boot plays is NOT established** — both callers of
`0x8002B0F4` pass it in a register, and the port stalls in `CdInit` long before a movie is reached.
That is `re-frontier` RE-07.

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

### `B(19h)` is `SetCustomExitFromException`, and for this binary it is ERROR RECOVERY — measured

`B(19h)` is not "hook a handler function". Its argument is a **jmp_buf-shaped structure**
`{ +0 ra, +4 sp, +8 fp, +0x0C..0x28 s0..s7, +0x2C gp }`, and the BIOS exception path — after walking
the `SysEnqIntRP` chains — loads those registers and jumps to `ra` instead of resuming the interrupted
context. Dumped at the call (`PSXPORT_DEBUG=bios`):

```
B0:0x19 custom-exit buf=0x800B28BC: ra=0x8008B990 sp=0x800B389C fp=0x807FFFF8 gp=0x800B47F4
  s0 = 0x800B2884   (libcd's state struct — the base whose +2 is the polling gate)
```

Three things fall out, and the third is the one that matters:

1. **The structure identification is confirmed independently.** `gp` reads `0x800B47F4`, which is
   exactly the value RE'd into `GameConfig::gp` from crt0, and `sp` (`0x800B389C`) is a dedicated
   stack distinct from the interrupted `sp` — while `fp` holds the interrupted `0x807FFFF8`. This also
   cross-checks the `A(13h) setjmp` implementation landed earlier today: it writes that exact layout.
2. **`ra` is MID-FUNCTION.** `0x8008B990` is the instruction immediately after `jal 0x80091340`
   (setjmp) inside `CdInit`, which begins at `0x8008B928`. A static recompile addresses function
   ENTRIES; it cannot dispatch into the middle of one. Any design that "just calls the custom exit"
   is unimplementable as written, and would have surfaced as a runtime recomp-MISS rather than at
   design time.
3. **So the custom exit is not a per-interrupt trampoline — it is libcd's `longjmp` error recovery.**
   `A(13h) setjmp` at `0x8008B988` fills the buffer; `B(19h)` registers it; a CD error unwinds to
   `0x8008B990` with `$v0 != 0`, so the `beqz` falls through to `jal 0x8008BA00` — the routine that
   sets the polling gate at `0x800B2886`. That is the same conclusion the static read reached from
   the other direction, now confirmed from the run.

**Consequence for the delivery design:** the CD interrupt does NOT arrive through `B(19h)`. Nor
through `SysEnqIntRP` — Spider-Man registers exactly one element there and it is libetc's VBlank.
libcd reaches its own service routine `0x8008C3E0` through an indirect call on a driver vtable at
`*0x800B390C`, which is zero in the load image and filled at runtime by BIOS machinery this framework
stubs out (`A(71h) _96_init` and friends return 0). **~~MEASURED: nothing ever fills that vtable.~~ FALSIFIED within the hour — see CLAIM-06.** The
"zero stores" observation was real, but the conclusion drawn from it was wrong, and the supporting
claim that "the load image ships it as `0`" was **never measured at all** — it was asserted.

`*0x800B390C` reads **`0x800B38EC`** in the load image, pointing at a fully populated struct that
ships in `.data` immediately before it. Zero stores plus loads that work is the signature of **static
linker initialisation**, not of a missing publisher. The binary names the structure itself: slot `+0`
points at `0x80096450`, which is libcd's rcsid string for **`intr.c`** — so this is libcd's own
low-level dispatch table, the standard Psy-Q "static jump table plus a static pointer to it" idiom,
present so an alternate backend can be swapped in.

```
+0x00 0x80096450 -> "$Id: intr.c,v 1.75 ..."   +0x10 0x8008BD18
+0x04 0            (written by CD_init)         +0x14 0            (written by CD_init)
+0x08 0x8008BBD0                                +0x18 0x8008BDB8
+0x0C 0x8008B928  (CD_init core)                +0x1C 0x800B2884   (libcd status block)
[0x800B390C] = 0x800B38EC   then 0x1F801070 I_STAT, 0x1F801074 I_MASK, 0x1F8010F0 DPCR
```

Confirmed live as well as statically: the store watch on `0x800B3DF0` reports hits with
`pc=0x8008BBD0`, which is slot `+0x08` — so the port is already dispatching through this table.

**There is nothing for the framework to provide here.** The structure ships in the executable and the
loader already covers it. Publishing that pointer would be fabricating a mechanism that does not
exist.

**Dead end recorded — a delay-slot misread.** The two stores in `CdInit` were read as
`descriptor[+0x14] = <ret of 0x80091360>` and `descriptor[+4] = <ret of A(72h)>`. Both are in
**branch delay slots**, so they store the value of `$v0` from *before* their `jal`: `desc[+4]` gets
`0x8009152C` (the return of `0x80091360`, a real function — which is why the thunk at `0x8008B89C`
can `jalr` through it) and `desc[+0x14]` gets the return of `0x8008C290`. `_96_remove`'s return is
discarded. **MIPS delay slots execute before the call they follow; a store in one belongs to the
previous instruction's result.**

**So the frontier reverts to interrupt delivery**, which this detour did not change. The table
carrying `I_STAT`/`I_MASK`/`DPCR` pointers at `+0x24`/`+0x28`/`+0x2C` is direct evidence of what its
ISR glue polls, and libcd's every-command `-1` alongside its own `intr timeout(...)` diagnostic is
exactly what a CD interrupt that never fires produces.

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

---

## WART-07 — `GameConfig::padDriverFn` is a field the framework never reads

*What it is:* `game_iface.h:159` declares `padDriverFn` alongside `padSlot0Buf` / `padSlot1Buf`. A
grep across `runtime/` finds **exactly one hit — the declaration itself**. No `.cpp` reads it.

Its intended consumer is `pad_input.cpp:285`, `static void pad_read(Core*)`, which serves the guest's
per-VBlank SIO pad read natively. But `Pad::overridesInit()` (same file, immediately below) only
calls `init()` — it registers nothing. `pad_read` is therefore dead static code, and `padDriverFn` is
an inert field.

*Why it matters more than a tidiness complaint:* the field LOOKS like the seam a new consumer must
fill to get input working, and this port spent a frontier step planning to RE the guest's per-frame
pad routine for it. A zero here reads as "not yet RE'd" under this project's own convention, when the
truth is "wiring it changes nothing". A seam that cannot have an effect is worse than an absent one,
because it costs RE time to discover that.

*How input actually reaches the guest today:* `Pad::serviceFrame()` (`pad_input.cpp:504-519`) writes
the 4-byte packet directly into `padSlot0Buf` / `padSlot1Buf` (consulting `padSlotPtrTable` first
where a port has one). That path needs no driver function at all — which is why nothing noticed.

*Status:* this port sets `padDriverFn` to 0 and says why at the initialiser. Upstream should either
register `pad_read` at `cfg->padDriverFn` in `overridesInit()` when the field is non-zero, or delete
both the field and `pad_read` — the no-tombstones rule favours deletion unless a consumer needs the
override. Not fixed from here: removing a `GameConfig` field would shift every positional initialiser
in the sibling consumer, and that consumer's schedule is its own call.

---

## WART-08 — Main RAM is not mirrored, so a legal guest address reads as "UNMAPPED"

*What it is:* the framework maps main RAM once at `0x80000000` and reports anything above the 2 MB
window as unmapped:

    [mem:error] UNMAPPED RAM read32 @ 0x80800000 — access is being DISCARDED. This is a memory-model
    gap, not a stray I/O poke: guest writes here vanish and reads return 0.

The message is right that it is a memory-model gap. On real hardware the PSX's 2 MB of RAM is
**mirrored four times** across `0x80000000..0x807FFFFF` (KSEG0), so `0x80800000` is not an invalid
address at all — physical address `0x00800000 & 0x1FFFFF` = 0, i.e. the start of RAM.

*How this port hit it, which is the part worth keeping:* Spider-Man ships the **devkit** stack-top
constant, `*0x800B3E70 = 0x00800000` (8 MB). crt0 computes `heap = (sp - sizeglobal) - heapBase`, so
the guest's allocator believes it owns 7.5 MB on a 2 MB console. Its free-list walk therefore runs off
the end of real RAM, and the sequential reads from `0x80800000` upward in the boot logs are exactly
that walk. The game's own guard (`FUN_8006BF9C`: `if (0x801FDFFF < heap_ptr) print(...)`) shows the
developers knew the real ceiling.

*Why it matters beyond the log noise:* a discarded write followed by a zero read is silent
corruption. It is also actively misleading during RE — the address looks like a wild pointer and
invites a hunt for the bug that produced it, when it is a legal mirrored access.

*Fix:* mask main-RAM accesses to the 2 MB window (`addr & 0x1FFFFF`) across the whole `0x80000000..
0x807FFFFF` KSEG0 range rather than treating the upper mirrors as unmapped, and keep the loud
diagnostic for addresses genuinely outside RAM. Not fixed from here yet: it changes the framework's
memory model for every consumer, and this port has not yet needed a mirrored access to be CORRECT —
only to stop being reported as an error. Establish that need first.
