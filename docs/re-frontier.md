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

## RE-03 — libcd `CdInit` — `re-partial`; **still `next`, but the ground has moved**

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

**Next step:** determine what libcd's low-level wait actually tests for completion — the flag the CD
IRQ handler would set — and satisfy it from this port's CD HLE at the point the synchronous read
completes. Note the chicken-and-egg to avoid: the three CD event callbacks are installed by `CdInit`
*after* `0x8008A1FC` succeeds, so the init-time wait cannot be relying on them; find what it polls
instead rather than assuming the callback path.

Everything below this line predates the fix; treat the eliminations as still valid (they were
measured) but re-derive nothing from the `cmd 0x00` framing.

---

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
