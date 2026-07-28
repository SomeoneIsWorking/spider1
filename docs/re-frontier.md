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

## RE-03 — libcd `CdInit` — `blocked` on nothing; **this is `next`**

Where the port stops, now characterised precisely (the earlier, vaguer version of this entry was
written against the contaminated substrate):

```
main 0x8002C354 -> 0x8006BF9C -> 0x800649E4        the game's disc-init RETRY loop
                                   |
                                   +-> 0x8008710C  BIOS A(0x3F) printf  (the "UNIMPL A0:0x3F" flood)
                                   +-> VSync x100  wait ~100 fields
                                   +-> 0x8008A16C  CdInit -> returns 0 -> retry forever
```

`0x800649E4` is a bounded-wait retry: print, wait 100 vblanks, call `CdInit`, loop while it returns 0.
It never succeeds, so the boot spins here indefinitely.

`0x8008A16C` is libcd's `CdInit`: it calls `0x8008A1FC` up to **4 times** (counter `s0` starts at 4),
and only on a return of 1 does it install three CD event-callback pointers (into `0x800B3B14`,
`0x800B3B18`, `0x800B1C7C`), clear `0x800B1C80`, and return 1.

`0x8008A1FC` returns 1 **iff both** `0x8008D4E4` and `0x8008D3F4` return 0 — read directly:

```
8008A204  jal 0x8008D4E4          ; low-level init A
8008A20C  bnez v0 -> 0x8008A224   ; A non-zero  -> return 0 (failure)
8008A214  jal 0x8008D3F4          ; low-level init B
8008A220  sltiu v0, v0, 1         ; v0 = (B == 0)
```

`0x8008D4E4` clears the libcd state globals, then runs the controller reset handshake at
`0x8008D588`: select index 1, write 7 to the IRQ-flag register to acknowledge, write 7 to the
IRQ-enable register, read the flag register back, and loop while `& 7` is non-zero.

### The measured mechanism (2026-07-28)

`PSXPORT_DEBUG=cdc` — the framework CD model's own channel — reports, 77 times in a 45 s boot:

```
[cdc] cmd 0x00 params=0 [00 00 00]
[cdc] UNHANDLED cmd 0x00 -> ack only
```

So the guest is writing `0` to the command register (`0x1F801801` with index 0) and the framework's
`exec_command` is treating that as command `0x00`. Its `default:` branch calls `cdc_irq(s, 3, ...)`,
which **enqueues an INT3 acknowledgement**. Each phantom IRQ leaves the queue non-empty, so the flag
register reads `0xE0 | type` instead of `0xE0`, `& 7` stays non-zero, and the handshake loop cannot
converge. (An empty queue reads `0xE0`, whose low 3 bits are 0 — the loop's own exit condition.)

**Open question, and it must be answered before changing anything:** the real CXD1199 has no command
`0x00`, and would respond to an invalid command with INT5 (error), not INT3 (ack). So the framework's
"unhandled -> ack only" default looks wrong in general. But whether the correct fix is (a) not
treating a `0` write as a command at all, (b) responding INT5 to invalid commands, or (c) something
in how the guest reaches that write, is NOT yet established — and (b) changes behaviour for the
reference consumer, which may depend on unhandled commands being acked. Determine what the guest
intends by that write first; do not change the shared CD model on a hunch.

**Caller chain, established by host backtrace (2026-07-28):**

```
0x8008CE8C <- 0x8008D4E4 <- 0x8008A1FC <- 0x8008A16C <- 0x800649E4 <- 0x8006BF9C <- 0x8002C354
```

`0x8008CE8C` is libcd's command-send routine: `a0` = command byte, `a1`/`a2`/`a3` = params/result,
and it indexes a command-name table at `0x800B3B38 + cmd*4` for a debug print gated on
`*(0x800B3B1C) >= 2`. `0x8008D4E4` issues four commands through it (`0x8008D628/D650/D664/D680`)
with `a0` = **0x01 Getstat, 0x01, 0x0A Init, 0x0C Demute** — every one of which the framework's
model implements. (Thirteen call sites exist in total; see the enumeration below.)

**The live contradiction — three facts that do not fit, and none of them is a guess:**

1. **Runtime.** The command register receives `0x00`, and at the store both `a0` and the callee's
   `s1` read `0` (`PSXPORT_DEBUG=cdcw`).
2. **The translation is faithful.** `gen_func_8008CE8C` emits `c->r[17] = c->r[4]` at entry and
   `c->mem_w8(c->r[2], c->r[17])` at the store — an exact match for `move $s1,$a0` and
   `sb $s1,($v0)`. The caller's `a0 = 1` at `L_8008D5D8` is emitted too, and is not clobbered before
   the call. So the recompiler-mistranslation hypothesis from the previous entry is **not supported**
   for this path.
3. **No call site passes zero.** All **13** `jal 0x8008CE8C` sites in the text were enumerated
   (nine at `0x80086D3C..0x80086FF0`, four at `0x8008D628..0x8008D680`); the command argument is
   `0x01`, `0x02`, `0x0A` or `0x0C`. Never `0`.

So the caller reaching the store with `a0 = 0` has **not been identified**, and that is the state of
this step — not a conclusion about the game.

**Why caller identification is hard here, and the trap to avoid:** BOTH available signals mislead.
  * Guest `pc`/`ra` are not refreshed on static gen-to-gen calls (INST-07).
  * The host backtrace is confounded too: generated code is built with `-foptimize-sibling-calls`
    (required — guest tail-jump loops would otherwise grow the stack without bound), so a tail call
    REPLACES the caller's frame. A backtrace can therefore name a function that merely tail-called
    into the chain, with intermediate frames gone. The `gen_func_8008D4E4` attribution recorded
    earlier rests on exactly that signal and should be treated as **unconfirmed**.

**Suggested next step:** stop relying on frames. Install a framework override on `0x8008CE8C` that
logs `a0` on entry and then super-calls the original body. That yields a per-entry argument log that
neither stale registers nor frame collapse can distort, and it will say directly whether a
zero-argument caller exists.

**Two corrections to earlier entries in this file, both now settled:**
  * An earlier revision said the chain `0x8008A1FC -> 0x8008D4E4 -> 0x8008CE8C` came from the
    contaminated substrate and "should not be trusted". **That was wrong, and the original reading
    was right.** `0x8008D4E4` does call `0x8008CE8C`, from four sites at `0x8008D628..0x8008D680` —
    past `0x8008D620`, which is simply where the earlier disassembly stopped. Confirmed by host
    backtrace on the clean substrate. Distrusting a correct record cost more than the original error.
  * `PSXPORT_WWATCH` remains distrusted (INST-06) — it is not what produced this chain. The working
    instrument is `PSXPORT_DEBUG=cdcw,cdcbt` (INST-07).

**Guest `pc`/`ra` are unreliable here.** The recomp does not refresh them on static gen-to-gen calls.
The first trace attributed the CD command write to `0x8008B900` — a three-instruction getter that
loads a halfword and returns, with `ra=0`. Only the host backtrace named the real chain.

**Do not fabricate a success return.** Forcing `CdInit` to report 1 without the handshake and the
callback state would make the boot appear to progress with the CD subsystem unconfigured and the
three callback pointers null — far harder to diagnose later than a clean stop.

Downstream, not started: the actual data path. This game's assets live in one packed archive
(`CD.WAD`) rather than as ISO files, so the loader will not resemble an SDK-file-per-asset game's.

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
