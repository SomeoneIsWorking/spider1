# RE frontier — Spider-Man (PSX, SLUS_008.75)

The ordered reverse-engineering dependency chain toward a faithful reimplementation. Its one job is
to record, per step, whether the port's behaviour there comes from **real RE of the binary** or from
a **hack that jumped ahead of the RE** — because a hack makes a broken port *look* finished and
blocks the real work.

Consult `next` at the START of a task and work THAT step, not a downstream one. Update in the same
commit that changes a step.

**Status vocabulary**
- `re-verified` — ground truth taken from the binary AND verified on real data.
- `re-partial` — some of it is RE'd; the remainder is named below.
- `blocked` — cannot start until a listed prerequisite lands.
- `⛔ hack` — debt. Something stands in for a mechanism that has not been RE'd. **This list must
  shrink.** There are currently **zero** entries, and that is deliberate: where the RE is not done,
  the port hangs or aborts loudly rather than fabricating behaviour.

---

## RE-00 — Provision + statically recompile the executable — `re-verified`

The disc carries ONE executable (`SLUS_008.75`, booted directly by `SYSTEM.CNF`), the packed archive
`CD.WAD`, `COMPILED.XA`, and the `CINEMAS/*.STR` movies. There are **no overlay modules** — confirmed
both by the ISO tree (`discdump list`) and by the recompiler reporting `0 overlay module(s)`.

`tools/ensure_recomp.py` extracts the executable and runs the framework recompiler, hash-gated on the
inputs so every machine builds an identical substrate.

The seed set is supplied by this repo (`game/recomp_seeds.json`) and is deliberately **empty**, so
discovery runs purely from the binary — entry point, the recompiler's pointer/table scans, and direct
`jal` following. It is a hash input, so changing it forces a regenerate everywhere.

**Evidence:** PS-X EXE header — entry `0x8008739C`, load `0x80010000`, text `0x000B6800`.
**1561 functions** emitted across 8 shards from 335 seeds.

**Corrected 2026-07-28:** the first run of this step reported 1580 functions from 355 seeds and was
*wrong* — it predated the framework taking seeds via `--seeds`, so the recompiler used Tomba!2's
hardcoded list, of which 27 of 34 land inside this game's text and split real functions. See
`docs/info/claims.md` CLAIM-00 (falsified) and CLAIM-04.

**Expires if:** a seed is added to the seed file without a recorded rationale, or a different
region/revision of the disc is used (the addresses below are US-retail).

---

## RE-01 — crt0 / boot seam — `re-verified`

The standard Sony crt0 at `0x8008739C`. Every value in `GameConfig`'s boot group is taken from it
instruction by instruction; the mapping is documented in full in `game/core/game_config.cpp`.

The framework's own generic `crt0_setup` reproduces this exact sequence (BSS-zero, `sp` from a
stack-top global minus 8, heap base at end-of-BSS, heap size `(sp - sizeglobal) - heapBase`, then
`InitHeap`), so the mapping is a structural match rather than an approximation.

**Evidence:** `bssZero 0x800B5994..0x800C65D4`, `gp 0x800B47F4`, `libcInit 0x8008DC98`
(BIOS `A(39h)` InitHeap stub), `gameMain 0x8002C354`. Verified live: the port boots through crt0 and
into the guest's own `main`, which runs real translated code.

**Expires if:** a `rec_dispatch` MISS or a wild guest write appears during crt0 — that would mean one
of these globals is not what it is recorded as.

---

## RE-02 — libetc `VSync` — `re-verified`

`VSync(int mode)` at `0x80084BE0`, reimplemented natively in `game/core/sync_native.cpp`.

Identified from ground truth, not inference: its only-caller wait helper (`0x80084D58`) emits the
string at `0x80096020`, which reads `VSync: timeout`. Full control flow, the guest globals it
touches, and the tail stores are documented at the implementation.

**The measured finding that shaped it:** instrumenting every call over a 60 s boot
(`PSXPORT_DEBUG=vsync`) gives **427,643 `VSync(-1)` (query) against exactly one blocking `VSync(0)`**,
from graphics init. This game does not pace with blocking VSync — it polls the free-running vblank
counter and times itself against it. So the counter must advance with real time at the NTSC field
rate (60000/1001 Hz), which is what the native handler does; a counter advanced only inside blocking
calls left the poll loop spinning forever. See `docs/info/claims.md` CLAIM-02.

**Verified on real data, and re-verified on the clean substrate:** the pre-fix hang (a deterministic
spin in `_vsync_wait`) is gone; the run proceeds past graphics init and down into the game's own
disc-init retry loop (RE-03), which is where it now stops.

---

## RE-03 — stock libcd — **PASSED, and the whole CD stack is now PC-native**

The boot no longer stalls on the disc. Every CD operation this game performs is served by the port,
with the data coming from the real disc image:

| guest entry | address | native replacement |
|---|---|---|
| `CD_cw` (command send) | `0x8008CE8C` | acknowledge + record the Setloc/Setmode state it maintained |
| `CdGetSector(dest,words)` | `0x8008D82C` | sequential sector FIFO from the raw sector |
| `CdRead(sectors,buf,mode)` | `0x80089ECC` | direct transfer, sector size chosen from the mode |
| `CdReadSync(mode,result)` | `0x8008A068` | complete — the transfer already happened |
| `CdSearchFile(loc,name)` | `0x80086170` | native ISO9660 lookup off the disc image |

**Measured:** retries 38 → 0, sector errors 38 → 0, `CD_init` 28 → 1. Files resolve to their real
extents (`CD.HED` LBA 390 / 12525 bytes, `CD.WAD` LBA 397 / 63213568, the whole `CINEMAS` tree), 7
sectors of `CD.HED` and 71 of `CD.WAD` load, and the game reaches asset loading — it prints
`sfx.vab`.

**What actually broke the stall**, worth recording because it was not where the effort went: the loop
at `0x800649E4` calls `CdSearchFile("\CD_HED;1")` and re-runs `CdInit` for as long as it returns 0.
Reading that loop in decompiled form named the blocker in seconds. Overriding `CdRead` at the API
level rather than per-sector then deleted the entire retry/timeout state machine underneath it.

**Two lessons this step paid for twice:**
1. *An override inherits the BOOKKEEPING of what it replaces*, not just its result — `CdLastPos`'s
   buffer has exactly one writer and it lives inside the routine `cdCommand` replaced.
2. *Intercept at the level that removes the machinery*, not the level nearest the symptom. Per-sector
   interception left the guest's timeout and retry logic running; the API-level one does not.

---

## RE-03 (superseded) — libcd `CdInit` — the earlier investigation

**`CdInit` now succeeds.** Wiring ONE `GameConfig` chokepoint — `cdCommand = 0x8008CE8C`, libcd's
command-send — cleared it:

| probe | before | after |
|---|---|---|
| `0x8008D4E4` (init A) | `0xFFFFFFFF` every time | **`00000000` ok** |
| `0x8008D3F4` (init B) | never ran | **`00000000` ok** |

Overriding that one address replaces the failing WAIT as well as the send, because the completion
wait at `0x8008D0A0` lies inside the same function (the next recompiled entry is `0x8008D298`). So the
whole send-then-wait-for-an-interrupt-that-never-arrives sequence became one native call.

**What this says about the preceding investigation, honestly:** the interrupt work (I_STAT/I_MASK, the
CD edge, chain delivery) was correct and is retained — but it was never the shortest path to a booting
port. psxport's design is to HLE the LIBRARY at `GameConfig` chokepoints, serving CD synchronously
from the real disc; this port had that entire group at zero while the hardware route got the
attention. The framework's own docs say this; the frontier did not.

**The boot now reaches the CD READ path**, and stalls there: 33 identical
`Setmode -> Setloc -> ReadN -> Pause` cycles, all from `0x80086D90`/`0x80086EC4`, seeking `LBA 8850`
and retrying because no data arrives.

**Next step, and a guess that was rejected:** the stock-libcd read primitive needs a native handler.
The framework's existing `cd_read` does NOT fit — it takes `(blocks, lba, buf)`, the ENGINE-loader
contract, whereas stock libcd's `CdRead(sectors, buf, mode)` carries no LBA and reads from wherever
`Setloc` left the head. `Cd::setloc_lba` exists for exactly this and is currently **recorded but never
consumed** — no stock-libcd read handler exists in the framework at all (the comment there notes Spyro
hit the same wall). `0x80086DE4` was the obvious candidate and is **wrong**: it masks `a0` to a byte
and uses it as an index into a table at `0x800B11A8`, so it is a slot dispatcher, not `CdRead`.
Identify the real primitive before wiring anything — a wrong override here writes disc sectors into an
arbitrary guest buffer.

---

## RE-03 (superseded detail below) — libcd `CdInit` — the investigation that got here

The `cmd 0x00` mystery that dominated this step is **gone, and it was never a CD problem**. It was a
framework memory-mapping defect: the guest stack lived in an unmodelled RAM mirror, so stack writes
were silently discarded and callee-saved registers came back as 0 — including libcd's command byte.
Full account in `docs/issues/boot-stalls.md` STALL-04; fixed in psxport `94118f85`.

**Current state, measured:** the CD model now receives exactly the commands the call sites pass —
`0x01` (Getstat) and `0x0A` (Init) — with **zero unhandled commands** and zero lost registers.

**What remains — narrowed to one leaf, measured:**

| probe | result |
|---|---|
| `0x8008D4E4` (init A) | returns `0xFFFFFFFF` **every time** — this is the failing half |
| `0x8008D3F4` (init B) | never runs (short-circuited by A) |
| command-send `0x8008CE8C` | returns `0xFFFFFFFF` for **all 60 calls**, Getstat and Init alike |

So it is not command-specific: **every** libcd command reports failure. The `-1` is set at
`0x8008D150`/`0x8008D15C`, on a path that first prints a diagnostic via BIOS printf and then calls the
controller-reset routine `0x8008D320` (the bank-1 acknowledge sequence already seen in `cdcw` traces).

The binary names its own failure. The format string that path prints (`0x8009660C`) reports a
command-wait result with **`Sync`** and **`Ready`** fields, and the routine label at `0x800966F0` is
`CD_init:`. So this is libcd's standard command-completion wait giving up: the command is written,
the response never satisfies the wait, it times out, resets the controller and returns `-1`.

**MEASURED — and the wait is not what "command wait" suggested.** Tracing CD register reads
(`PSXPORT_DEBUG=cdcr,cdcw`) shows the guest write the command and then go **straight** to the error/
reset routine `0x8008D320` with **no intervening CD register read at all**. It never polls the
controller for a response.

Disassembling the path after the command store explains why. The wait polls the **vblank counter**,
not the CD:

```
8008D048  jal 0x80084BE0      ; VSync(-1) — read the vblank counter
8008D050  addiu v0, v0, 0x3C0 ; deadline = counter + 960 fields (~16 s)
8008D060  sw   v0, 0x6394(..) ; store the deadline
...
8008D0A0  jal 0x80084BE0      ; re-poll the counter
```

So libcd waits for a **completion signal delivered by the CD interrupt callback**, and uses the
vblank counter only as a TIMEOUT. Our runtime serves CD synchronously and never delivers that
callback, so the completion flag is never set and every command burns the full deadline before
reporting failure. That is also why the boot is so slow to reach the retry: ~16 s of real time per
command, with a real-time-driven vblank counter (CLAIM-02).

**So the fix belongs in THIS PORT's HLE, not the shared CD model** — exactly the possibility the
previous revision flagged. The model is behaving correctly: it queues the response (the trace shows
the interrupt-flag register reading `0xE3`, i.e. INT3 pending, and the reset routine acking it with
`w[1803]=07`). Nothing consumes it, because the guest is waiting on a callback that never fires.

**IDENTIFIED — the completion flag is the byte at `0x800B3DF0`.**

The wait loop, read out in full:

```
8008D0A0  jal 0x80084BE0        ; VSync(-1) -> vblank counter
8008D0B4  slt  v1, deadline, v0 ; timeout 1: counter past deadline (counter + 0x3C0, ~960 fields)
8008D0E0  slt  v0, 0x3C0000, v1 ; timeout 2: spin count past 0x3C0000
8008D210  lbu  v0, (s2)         ; s2 = 0x800B3DF0 — THE COMPLETION FLAG
8008D218  beqz v0, 0x8008D0A0   ; still zero -> keep polling
```

The flag is cleared to 0 before the command is issued (`0x8008CFB4`) and the loop exits when it turns
non-zero. It is written at `0x8008C754`, which stores **2** when an error indicator is clear and **5**
when it is not — the libcd result codes for *complete* and *disk error*. So `0x800B3DF0` holds
libcd's interrupt-result code, and the routine around `0x8008C744` is the completion handler that a
real CD interrupt would run. Our synchronous CD never reaches it, so the flag stays 0 and both
timeouts expire.

**RESOLVED — the completion handler is `0x8008C3E0`, and it is real hardware code, not a stub target.**

Reached by scanning backward from the flag store to the enclosing prologue (`addiu sp, sp, -0x30` at
`0x8008C3E0`; the preceding function ends at `0x8008C39C`). It takes **no arguments**. Its whole input
is the CD controller, read through three pointer globals the executable initialises statically:

| global | value in the loaded image | port |
|---|---|---|
| `0x800B3DD8` | `0x1F801800` | index/status |
| `0x800B3DDC` | `0x1F801801` | response FIFO |
| `0x800B3DE0` | `0x1F801802` | data FIFO |
| `0x800B3DE4` | `0x1F801803` | interrupt flag |

So the earlier plan — "drive the guest's completion handler" — needs no argument reconstruction at
all. The handler services the controller and returns a bitmask of what it saw. **The framework's CDC
model already presents exactly these four registers** (`runtime/recomp/cdc_state.h`, dispatched from
`mem.cpp:160/204`), including the pending-interrupt queue. Nothing about this step requires new
hardware modelling.

**So the real blocker is one level up: nothing ever calls the handler.** Every reference to
`0x8008C3E0` in the executable is a direct `jal` — from `0x8008CAAC`, `0x8008CD2C`, `0x8008D188`,
`0x8008DA58`. The one inside the stalling wait loop is guarded:

```
8008D160  jal  0x8008b900      ; getter: lhu $v0, 0x800B2886 — nothing more
8008D168  beqz $v0, 0x8008d210 ; gate CLOSED -> skip the service call, go straight to the flag test
8008D188  jal  0x8008c3e0      ; the completion handler — only on the open path
```

`0x800B2886` is written in exactly one place, `sh $v0, 2($s1)` at `0x8008BA68` (`$s1 = 0x800B2884`),
inside `0x8008BA00`. `CdInit` calls that only when BIOS **A(13h) `setjmp`** at `0x80091340` returns
non-zero — i.e. only after a `longjmp` back into `CdInit` from an error path. **That gate is libcd's
degraded polling fallback, not its normal mode.** In normal operation the handler runs from the CD
**interrupt**, and the poll gate stays shut for the whole boot. Confirmed by scanning the full text
for stores to `0x2886` and for stores through the `0x800B2884` base: one writer, that one.

**Root cause of RE-03, stated plainly: psxport has no interrupt delivery.** `B(19h) HookEntryInt`
records the handler into `hle.int_handler` (`hle.cpp:176`) and **it is never read anywhere** — the
only two mentions of the symbol in the whole runtime are its declaration and that assignment.
`C(02h)/C(03h) SysEnqIntRP/DeqIntRP` return `$a1` and record nothing. So a guest that waits on any
interrupt-delivered completion waits forever; libcd is simply the first one this port reached.

> **CORRECTION (same day):** the paragraph below ends by sending the next step to the driver vtable
> at `*0x800B390C`. That was **wrong** — see `docs/info/claims.md` CLAIM-06. The table is libcd's own,
> ships fully populated in `.data`, and is already dispatched through at runtime. Nothing is missing
> there. The frontier reverts to **interrupt delivery**. Everything else in the paragraph — the
> `B(19h)` identification and the mid-function `ra` — was measured and still holds.

**UPDATED after measuring `B(19h)` — the CD interrupt arrives by NEITHER route this port can see.**
`B(19h)` is `SetCustomExitFromException`, and its buffer resolves to a `longjmp` target **mid-way
through `CdInit`** (`ra=0x8008B990`, the instruction after the `setjmp` call) — libcd's error
recovery, not a per-interrupt trampoline, and not an address a static recompile can even enter.
`SysEnqIntRP` carries exactly one element and it is libetc's **VBlank**. libcd instead reaches its
service routine `0x8008C3E0` through an indirect call on a driver vtable at `*0x800B390C`, which the
load image leaves **zero** and which is filled at runtime by BIOS machinery this framework stubs
(`A(71h) _96_init` and friends return 0). Full evidence in
`docs/issues/framework-agnosticism-warts.md` WART-05.

**So the next RE step is that vtable, not the dispatch code:** establish what fills `*0x800B390C` and
what it points at. It decides whether the CD path needs a chain walk at all, and writing delivery
before knowing would be building for a route this game does not use.

**Next step (framework, still required for VBlank):** an interrupt-delivery model (game-agnostic — this is not a
Spider-Man fact). Minimum shape: keep the handler chain that `SysEnqIntRP`/`HookEntryInt` register,
and invoke it from the point where `cdc_native` queues an interrupt, with `I_STAT`/`I_MASK` modelled
well enough that the handler's acknowledge sequence clears it. Then the guest's own `0x8008C3E0`
runs, sets `0x800B3DF0` itself, and the wait exits through the path the hardware would have used.

**Measurement caveat, found while confirming the above:** every RE-03 measurement up to this point
was taken with **no disc mounted** — the framework's disc resolver only knew the reference consumer's
env key (WART-06, fixed in psxport `4177ccf3`). The root cause is unaffected, because the handler is
never called with or without media, but anything about what libcd *reads back* must be re-measured.
Related, from the same pass: BIOS `A(13h) setjmp` was unimplemented, so `CdInit`'s choice between its
normal path and its `longjmp` recovery path was made on a **stale `$v0`** and varied run to run. It
now returns 0 deterministically, which means the polling gate is definitively shut.

**TRAP — the completion flag DOES reach 2, and that does not mean completion.** A store watch on
`0x800B3DF0` (`PSXPORT_WWATCH`, positive-control validated at 241 hits) reports **172 writes of `2`**
over a 35 s boot — the libcd result code for *complete*. Read naively that says the completion path
works. It does not. Attributing each store to its innermost frame (`PSXPORT_WWATCH_BT=1`, because
plain `pc`/`ra` are the known-unreliable kind here) gives:

| value | written by |
|---|---|
| `2` | `0x8008C944` (89), `0x8008D320` (88), `0x8008D4E4` (45) |
| `0` | `0x8008CE8C` (89) — the pre-command clear |

`0x8008D320` is the **controller-reset / error** routine and `0x8008D4E4` is the failing init half.
**`0x8008C3E0` — the interrupt handler — does not appear at all.** So the `2` is written by the error
path unwinding its own state machine after the wait times out, not by a completed command. The model
in this step is unchanged; what changed is that the obvious probe on this byte reports a false
positive, and a future session watching it would conclude the opposite of the truth.

Corollary for the earlier note: `0x8008C754` is *a* writer of this byte but not the only one — it was
the only store using that immediate, while the others reach it through a base register. Do not treat
an immediate-offset scan as a complete writer set.

**ESTABLISHED, not inferred: the service routine `0x8008C3E0` never runs.** Every earlier statement
to this effect was inference from absence. It is now measured with an entry probe that emits an
unconditional ARM line (INST-11): the arm line prints, **zero** call lines follow over a 40 s boot.

**CORRECTED — only ONE of the four call sites is ungated, not three.** The earlier revision of this
paragraph claimed `0x8008CAAC`, `0x8008CD2C` and `0x8008DA58` were all ungated. That was read off a
window of ~8 instructions before each `jal`, and the gate sits **ten** instructions back. Widening the
window shows:

| site | gate |
|---|---|
| `0x8008CAAC` | **gated** — `jal 0x8008B900` at `0x8008CA84`, `beqz` at `0x8008CA88` |
| `0x8008CD2C` | **gated** — `jal 0x8008B900` at `0x8008CD04`, `beqz` at `0x8008CD0C` |
| `0x8008D188` | **gated** — the wait loop |
| `0x8008DA58` | genuinely ungated — it sits at the ENTRY of `0x8008DA24`, straight through |

And the one ungated site is unreachable for a different reason: **`0x8008DA24` has no direct caller
anywhere in the text** — a full scan for `j`/`jal` to it returns nothing, so it is a public libcd
entry the game only reaches indirectly, if at all.

**So RE-03's causal loop is now closed, and it is genuinely circular:**

1. Three of the four paths to the service routine are behind the polling gate at `0x800B2886`.
2. That gate is opened only by `0x8008BA00`, whose single caller is CdInit's `longjmp`-recovery path.
3. The `longjmp` never fires either — `A(14h)` is implemented in this framework as a loud abort, and
   the port does not abort, so nothing ever unwinds into that recovery.
4. So the service routine never runs; the completion byte is written only by error paths; every
   command times out; CdInit fails — which is the condition that would have triggered the recovery.

Breaking the cycle requires commands to actually complete, which requires the service routine to run,
which requires the interrupt route — not the gate. **The frontier stays on interrupt delivery.**

Also settled, by enumerating every BIOS-call stub in the executable: there is **exactly one `C(02h)`
`SysEnqIntRP` stub** (`0x8008DCB8`) and one `C(03h)` (`0x8008DCA8`), and the single registration made
through them is libetc's VBlank element. **libcd never registers a BIOS interrupt element at all.**
And the polling-gate setter `0x8008BA00` has exactly one caller, `0x8008B998`, on CdInit's
`longjmp`-recovery path. So neither route to the service routine is open on a normal boot.

**libcd's whole interrupt side is dormant — measured, three probes, all armed.** Entry probes
(observe-only, super-calling) on the service routine and on BOTH callbacks libcd installs report
**zero** entries over a 40 s boot, each behind an unconditional ARM line:

| probe | installed by | entries |
|---|---|---|
| `0x8008C3E0` service routine | — (4 static call sites) | **0** |
| `0x8009152C` | CdInit, into descriptor slot `+4`; reached via the thunk `0x8008B89C` | **0** |
| `0x800913AC` | the registrar, as libcd callback **#3** | **0** |

Entry probes matter here specifically because a `jal`-only call graph **cannot** answer this: libcd
dispatches indirectly throughout, so "not statically reachable" would be a weak negative. A probe at
the callee's own entry fires wherever the call came from.

**Falsified along the way — the CD is NOT serviced from the VBlank interrupt.** That was the obvious
remaining hypothesis, since VBlank is the one element registered with the BIOS. Call-graph
reachability from the VBlank handler `0x80087660` does not reach `0x8008C3E0`. The graph was
control-validated first (CdInit reaches 14 nodes; an earlier version of the same query silently
reached 1 and would have "confirmed" anything asked of it) — but it follows only `jal`, so treat this
as corroboration of the probes, not as independent proof.

So nothing in this port delivers an interrupt, and nothing else stands in for one. **Interrupt
delivery is the only remaining route, now by elimination rather than by assumption.**

**LANDED — interrupt delivery works, and it proves the CD is not serviced that way.**
psxport `43a12e27` implements the whole path: `SysEnqIntRP`/`SysDeqIntRP` maintain a priority-ordered
chain of guest `InterruptElement` pointers, `Hle::irqPoll` walks it exactly as the BIOS exception
path does (run each verifier; the first that claims it gets its handler called with `$a0` = the
verifier's return), and the poll happens at **guest function entry** — a one-line emitter change, one
load-and-test of `Core::irq_pending` per call, on all 1561 wrappers.

Measured on this port (`PSXPORT_DEBUG=irq`):

```
registered interrupt element 0x800C1528 prio=2 (chain now 1)
pending I_STAT&I_MASK=0x004 but NO registered element claimed it (1 in chain)
```

So the mechanism runs end to end — registration, latch, gate, chain walk — and the outcome is a
**statement about the game, not about the framework**: the one element Spider-Man registers is
libetc's VBlank, and nothing claims CD IRQ2. The "nobody claimed it" line exists precisely because
that and "the walk never ran" are indistinguishable from outside, and only the first is evidence.

**This closes the last route.** RE-03's remaining question is no longer "how do we deliver
interrupts" — that is done and verified — but **what services the CD in this game**, given it
registers no BIOS element, its polling gate opens only on a `longjmp` that never fires, and its
service routine has no reachable caller. The next candidate is the BIOS's own CD-ROM driver, which
this framework stubs to constants (`A(70h)`/`A(71h)`/`A(72h)` all return 0) and whose descriptor
libcd fills in at `CdInit`.

**CORRECTED by decompilation — `0x800B2886` is an IN-INTERRUPT flag, not a "polling gate".**
Several revisions of this step described `0x800B2886` as a gate that opens libcd's polling fallback,
opened only via `longjmp` error recovery. That reading is **wrong**, and one decompile shows why.

`0x8008BA00` is libapi's **interrupt dispatcher** (`intr.c`). It sets `DAT_800b2886 = 1` on entry —
meaning *"we are inside interrupt context"* — then loops over **11 IRQ lines**, computing
`I_MASK & enabled-mask & I_STAT` (`*DAT_800b3914 & DAT_800b28b4 & *DAT_800b3910`), acking each
serviced bit with `*DAT_800b3910 = ~(1 << n)`, and calling that line's callback from the table at
`DAT_800b2888`. Only two functions in the whole image touch `0x800B2886`: this one writes it, and
`0x8008B900` reads it.

So `0x8008B900` answers *"am I in interrupt context?"*, and the three CD service call sites gated on
it are the **called-from-inside-the-ISR** path — the inverse of what was recorded. The CD's callback
is a slot in `DAT_800b2888`, installed at runtime, which is exactly why a static reference scan found
no caller for `0x8008DA24`.

The decompiled handler makes the completion path plain:

```c
void FUN_8008da24(void) {              // the CD line's callback
  while (true) {
    uVar2 = FUN_8008c3e0();           // service routine; returns a bitmask of what it handled
    if (uVar2 == 0) break;            // drain until nothing left
    if ((uVar2 & 4) && DAT_800b3b18) (*DAT_800b3b18)(DAT_800b3df1, &DAT_800c6384);   // ready cb
    if ((uVar2 & 2) && DAT_800b3b14) (*DAT_800b3b14)(DAT_800b3df0, &DAT_800c637c);   // sync cb
  }
}
```

**What this means for the port:** delivery must reach `0x8008BA00`, which then fans out to the CD
slot. The framework's chain walk already runs; the open question is narrower than before and stated
exactly: delivery declines with `irq_enabled = 0`, so find what leaves interrupt-enable clear.

**DIRECTION SET: the PC owns these subsystems; it does NOT emulate interrupts.** The interrupt work
(I_STAT/I_MASK, the CD edge, chain delivery, DMA3) is real and stays — it is correct hardware
modelling and other games may need it. But it is **not how this port should service the CD**. The
framework's design, and this port's, is that the PC OWNS a subsystem natively rather than
reproducing PSX hardware and re-entering guest ISRs.

Applied so far:
- `cdCommand = 0x8008CE8C` — the PC completes libcd commands. **CdInit passes.**
- `cdGetSector = 0x8008D82C` — the PC moves sector data. Its guest body is pure ceremony (program
  DMA3, spin for data-ready, kick, spin for done) around one fact: *move N words of the current
  sector into this buffer*. The native handler moves it, from the real disc, and skips all of it.

**MEASURED, and it names the remaining gap exactly:** `cdGetSector` registers (the plat-hle count
goes 1 → 2) but is **never reached**. `cd_command` acknowledges `ReadN` without making data
available, so the guest waits for a **data-ready callback that never comes** and never gets as far as
fetching sectors. Hence the 33 identical `Setmode → Setloc → ReadN → Pause` cycles.

**So the PC must own the COMPLETION, not just the command.** The decompiled handler names the two
callbacks precisely: `(*DAT_800b3b18)(DAT_800b3df1, &DAT_800c6384)` is the ready callback and
`(*DAT_800b3b14)(DAT_800b3df0, &DAT_800c637c)` is the sync callback. Both are guest function pointers
in guest RAM, so the framework can drive them directly — and `GameConfig` already has
`cdCallbackTable[4]` / `cdCallbackFn[4]` for exactly this shape. That is the next step: on `ReadN`,
serve the sector natively and then invoke the guest's ready callback, so the read completes without
an interrupt ever existing.

**PROGRESS — real sector data now flows, PC-owned, with no interrupt involved.** `cdGetSector` and
`cdReadyCbPtr` complete the design: a stock-libcd read is a per-sector callback loop, so the port
drives it by invoking the callback the game already registered. Measured: 78 `CdGetSector` transfers,
40 completed read cycles, real LBAs (16 and 8850), correct sector headers (`02:00:00 mode 02` at LBA
8850, matching its Setloc), zero read errors from the framework side.

**But the guest rejects the reads, and it now SAYS SO.** With BIOS `printf` implemented (INST-13) the
binary reports `CdRead: sector error` (34) and `CdRead: retry...` (68) — the
`FUN_80087220(&loc) != DAT_800b1c5c` branch of the game's ready callback at `0x800899A0`. That is the
drive-position check: it converts the 3-word header it just popped into a sector number and compares
it against its own expected counter.

**Note the correction this forced:** the headers being served are demonstrably right, and from that I
had concluded the check was passing. The game's own diagnostic says otherwise. Correct data at the
right offset is not the same as the check succeeding — the remaining suspects are the units
`FUN_80087220` produces (MSF→sector with or without the 150-frame lead-in) versus what
`DAT_800b1c5c` was seeded with, and whether the header must come from the sector the DATA is read
from or the one BEFORE it.

**Next:** decompile `0x80087220` and read what it computes, then compare against the seeding of
`DAT_800b1c5c` on the read-setup path. Both are one grep away in `scratch/decomp/`.

**`CdRead: sector error` is GONE — 38 → 0.** Root cause, found by grepping the decompiled corpus:
an override must inherit the **bookkeeping** of the routine it replaces, not just its result.
`CdLastPos()`'s buffer at `0x800B3B2C` has exactly ONE writer in the whole image, and it sits inside
`CD_cw` — the routine `cdCommand` replaces. The read-setup path seeds its expected-sector counter with
`CdPosToInt(CdLastPos())`, so with the record skipped it compared every sector header against stale
bytes. Fixed via `GameConfig::cdLastPosBuf`. A second bug of the same shape: Setloc now invalidates
the buffered sector, or the cursor keeps popping the previous one.

The game has moved on to reading the **ISO volume descriptor at LBA 16** — filesystem lookup.

**Open, and stated as a HYPOTHESIS rather than a finding:** 38 `CdRead: retry...` remain. That string
is printed by `0x80089CE4` when it is called with a non-zero argument, which its caller does when
sectors-remaining has gone negative. With the position check now passing, the remaining setter is the
**timeout**: `DAT_800b1c58 + 0x4B0 < VSync(-1)`. `DAT_800b1c58` is assigned in only two places, one of
them an error branch — so if the live path leaves it stale, the comparison is true immediately and
every read is marked failed regardless of success. **Not yet measured.** Log the two values before
acting; this port has repeatedly punished acting on a plausible mechanism.

**Still do not poke `0x800B3DF0`.** The handler writes more than that byte (it also fills the block at
`0x800C637C` from the response FIFO), so a direct poke is a fake completion — and now that the
handler is known to be argument-free and the registers are already modelled, there is no longer any
excuse for one.

Everything below this line predates the fix; treat the eliminations as still valid (they were
measured) but re-derive nothing from the `cmd 0x00` framing.

---

## RE-03b — SPU upload wait — `re-verified`

The boot reached `sfx.vab` and then spun forever in a `TestEvent` loop
(`0x80089790` ← `0x8006366C` ← `0x800633CC` ← guest `main`). The guest opens the SPU event
(`OpenEvent(0xF0000009, 0x20, …)` at `0x8008EF28`) and polls it until its sample upload completes —
the ordinary way to wait for a VAB. Nothing delivered that event.

Fixed in the framework, not here: the DMA4 handler now delivers `HwSPU` at the end of the transfer.
It performs the DMA synchronously, so the event is due the moment the words have moved. `0xF0000009`
is the PSX's fixed class, so it belongs in the framework rather than `GameConfig`.

**Verified:** the boot passes `sfx.vab` and reaches MDEC init.

---

## RE-03c — MDEC decode — `next`

The boot now reports the guest's own `MDEC_in_sync timeout:` and stalls at `0x80085948`
(← `0x80085000` ← `0x800860B4` ← `0x80086CA8`).

`0x80085948` is a generic **DMA-wait helper**: it takes a channel index and polls that channel's CHCR
at `0x1F801080 + ch*0x10 + 8` until the busy bit clears. For MDEC-in that is DMA0, and `mem.cpp`
*does* clear DMA0's busy bit after running `mdec_dma_in` — so the naive explanation is already ruled
out and this needs real measurement rather than a guess at the next override.

**ROOT-CAUSED, and it was not a DMA channel.** `DecDCTinSync` is `0x80085DF0`: it polls
`*0x800B114C`, which the load image ships as **`0x1F801824`** — the MDEC1 status register — waiting
for bit `0x20000000` (data-in) to clear, with a `0x100000` spin cap before printing its own
`MDEC_in_sync` timeout. The earlier backtrace naming `0x80085948` was confounded by
`-foptimize-sibling-calls`, exactly the caveat INST-07 records.

The defect is in the framework: `MDEC_DMAWrite` **silently drops a word when the input FIFO is
full**, and real DMA0 never permits that — it honours `MDEC_DMACanWrite()` and stalls until the
decoder has room. `mdec_dma_in` pushed whole blocks unconditionally, so words were lost, the decode
never completed, and the status bit stayed set.

**Measured, not inferred:** with the predicate honoured and drops reported, the log says
`input FIFO full after 1 of 32 word(s)` — the decoder was receiving **one word of a 32-word block**.

**FIXED — the decoder was never being driven.** Beetle's MDEC consumes input and produces output
only inside `MDEC_Run()`, and nothing in the runtime ever called it. So the input FIFO filled on the
first word and the rest were dropped, while the output side returned an empty drain. Real DMA0/DMA1
get this free by stalling while the decoder works; this model advances the decoder instead and
re-checks. Wedged decoders are now reported with exact counts rather than truncating silently.

**Verified:** mdec errors 0 (was `input FIFO full after 1 of 32 word(s)`), and the guest's
`MDEC_in_sync timeout:` is gone from its own output entirely.

---

## RE-03d — XA / CD streaming — `re-verified`

The streaming poller spun forever, and the reason was **upstream of the DMA it was preparing**. Its
helper `0x80085948` waits on the CD status register's **DRQSTS bit (0x40)** — "data FIFO not empty" —
and only kicks DMA3 once that sets:

```
while ((*CD_STATUS & 0x40) == 0) { }     // never set: the controller had no data
*DMA3_CHCR = 0x11000000;                 // never reached
```

A game reads the disc at **two levels**: file reads through libcd (served natively here) and
XA/streaming that bypasses libcd and drives the hardware directly. With libcd HLE'd, the controller
model never saw a command, so its FIFO stayed empty forever.

**Fix:** `cdc_begin_read` positions the controller and loads a sector from the *same disc image* the
native path reads, called from the native CD command handler on ReadN/ReadS. Neither layer invents
data; they share one source of truth.

**Corrected judgement worth keeping:** the previous revision of this step offered two options and
leaned toward owning streaming natively. That was wrong, and reading the wait is what showed it — the
spin is on a hardware status bit *before any DMA*, so a native override of the streaming reader would
never have been reached. The evidence picked the other option.

**Verified:** the poller no longer spins; the boot leaves libcd entirely.

---

## RE-04 — Movie playback — `re-partial`; the stream now RUNS

**Settled: the GUEST plays the movie itself.** `0x8002B3CC` calls `0x80086B10` (libcd's streaming
getter) and `0x800872AC` walks a sector ring at `DAT_800c1510`. It decodes through the MDEC path
fixed in RE-03c, so the framework's native `.STR` player is the wrong tool and `GameConfig::bootFmv`
stays empty — the guest wants sectors, not a replacement player.

**Three linked defects kept it pinned on one sector** (all fixed, psxport `7916dce0`):

1. **Setmode never reached the controller** — the native CD layer forwarded it to the XA streamer
   only, so `CdcState::mode` stayed 0.
2. **`load_sector` ignored the whole-sector bit** regardless. Streaming reads the first 8 words of a
   sector as header + subheader to identify it; it was getting picture data, recognised nothing, and
   re-requested the same sector forever.
3. **Advancement had no reachable trigger.** Draining cannot be it — this reader takes 2048 bytes of
   a 2340-byte FIFO and asks again. The interrupt ACK cannot be it — it drives BFRD directly and
   never acks. So the trigger is **BFRD itself**: a data request when the current sector is already
   partly read is a request for the *next* one, remainder discarded, which is what the drive does
   when it refills per sector.

Point 3 is worth keeping as a lesson: an earlier revision moved advancement to the ACK path on a
plausible hardware argument, and this file recorded "do not simply make BFRD advance". Both were
reasoning from how the hardware is usually described rather than from what this guest does. The
measurement settled it.

**Verified:** transfers now show BOTH the 8-word header and the 504-word body per sector, and the
head advances through the stream — **10 distinct LBAs, 128304 → 128313**, where it had been pinned
at 128304.

**DIAGNOSED — the consumer is fine, and the pump has no trigger.** The ring protocol is correct:
`StGetNext` (`0x80086B10`) takes a slot whose status is 2 and marks it 4; `StFreeRing`
(`0x800872AC`) returns slots to 0 and advances `DAT_800c151c`. Sectors simply stop being produced
after ten.

**Three placements for a stream pump were tried and all three were irrelevant**, for one measured
reason: **zero `ReadN` commands reach the CD command handler.** That is a consequence of serving
`CdRead` natively — the guest's finite read state machine no longer issues one — and this game's
streaming reader drives the CD registers **directly**, poking `0x1F801800`/`0x1F801803` itself rather
than going through the command entry point. So `Cd::stream_active` is never set and
`Cd::pumpStream` can never fire, wherever it is called from.

The three attempts, recorded so none is repeated:
1. **Per-field, from `vblank_advance`** — wrong home regardless: the guest makes only **13 VSync
   calls in the whole boot** and stops calling it entirely once inside the streaming loop.
2. **From `StGetNext`**, wrapping the function the spin loop actually calls — right instinct, but the
   pump underneath it was inert. The wrapper was removed; an override that wraps a hot function to do
   nothing is worse than none.
3. Both rested on the same unchecked assumption — that streaming starts with a `ReadN` through the
   command path. Checking that first would have skipped all three.

**So the real question is upstream:** what starts this stream, and what stops it after ten sectors?
The reader is at the register level, so the answer is in `cdc`'s own state — most likely `reading`
being cleared, or the BFRD advance path not being reached after the tenth sector. Instrument
`cdc_write`'s request/ack paths and the `reading` flag before touching the pump again.

**`Cd::pumpStream` is retained** in the framework: the mechanism is correct for a consumer whose
streaming does go through the command path, and it costs nothing while inactive.


## RE-04 — Per-frame OT / packet-pool layout — `blocked` on RE-03

`GameConfig`'s OT/packet-pool group is entirely zero. The framework's `native_step_frame` iterates
these to run a native frame loop; until they are RE'd this port does not use that loop at all — the
guest's own `main` loop drives, on the substrate. Blocked because the frame loop cannot be observed
until the boot gets past the CD stall.

---

## RE-05 — Scheduler task layout — `blocked` on RE-03

`taskTableBase` / `taskSlotStride` / `taskCount` / `curTaskPtr` and the stage entry PCs are zero.
`PcScheduler` is correspondingly unused; the `schedStageBody` / `schedFreshEntry` hooks fail fast if
ever reached. Neversoft's engine may not use the SDK task model at all — establish that first rather
than assuming a Tomba-shaped scheduler exists here.

---

## RE-06 — Pad driver — `blocked` on RE-03

`padSlot0Buf` / `padSlot1Buf` / `padDriverFn` / `padSlotPtrTable` are zero. The framework's native
pad override is installed but has no game-side buffer addresses to write into, so input is not yet
wired.

---

## RE-07 — Intro FMV / front-end — not started

The movies live under `CINEMAS/` on this disc. The framework's boot-time FMV player hardcodes the
reference consumer's path (`MOVIE/LOGO.STR`), so it is disabled by default in `run.sh` rather than
left to fail. Wiring this game's FMV path is downstream of the front-end coming up at all.

---

## RE-08 — Render: GTE tap → native depth — not started

The highest-leverage generic capability per the framework's porting guide: a single GTE choke point
yields per-primitive world coordinates, from which native depth, widescreen, and per-object
interpolation all follow. Nothing game-specific is needed to *start* it, but there is no picture to
verify against until RE-03 unblocks the boot.

**Frame-rate note (design, not RE):** this game declares no target frame rate — see RE-02. Any
interpolation tier is therefore a PORT decision to be made against the achieved logic rate once the
game runs, and recorded here when it is made.
