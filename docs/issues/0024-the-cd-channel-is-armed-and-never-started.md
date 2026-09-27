---
id: 24
title: The CD channel is armed and never started, so the sector never reaches the STR ring
status: open
symptom: reaches retail STR field 1 at 0x8002AC8C and presents it, then spins; one display field
         per host turn at ~4.5M guest instructions/s and no second field, no ring publication, no
         dem1, no stage, no scene
tags: cd,dma,str,libstr,frontier,s018
created: 2026-09-27
updated: 2026-09-27
---

## The measured frontier, 2026-09-27, psxport `006eb917`, `build/consumer-verify`

One bounded headless run, 4:3 (`aspect=0`), `fps60=0`, present sink 640x480, offscreen, silent,
unpaced, killed by captured PID. `PSXPORT_DEBUG_SERVER` on a dedicated loopback port and the
framework's own `guest` command read LIVE, ten samples, so these are the shipping counters and not
a scrape of a text log:

| counter | value |
|---|---|
| executor calls | 449,741 |
| **translated blocks** | **1,494** (constant for the whole run) |
| executed blocks | 154,710,784 |
| **executed instructions** | **922,406,926** |
| host dispatches | 193,591 |
| cache hits / misses | 154,709,290 / **1,497** |
| invalidations | 14,679,123 |
| faults | 0 |
| **fallback, all six reasons** | **calls=0 instructions=0, and every `refused_*`=0** |

Fallen back to interpreter: **0 blocks, 0 instructions, of 1,494 blocks translated and 154,710,784
blocks executed.** So this is 100% dynarec execution, and it is still **not gameplay evidence** —
the denominator that matters is not the numbers above but where they were spent. 922M instructions
bought exactly one movie field.

Guest state reached, in order, each line a distinct log event rather than an inference:

```
[frame] pre-main ResetGraph field at 0x8008479C          (1)
[frame] registered field callback 0x8005E510
[cd]    CdInit armed the CD interrupt: I_MASK 0x009 -> 0x00D
[cd]    CdRead 71 sector(s) x 2048 bytes from LBA 397 -> 0x800FCA9C (mode 0x180)
[wide]  guest draw clip 512x240 -> 512x512
[str]   Spider-Man 1 resumed retail STR field 1 at 0x8002AC8C   (1 field, then nothing)
```

and then, for the remaining 100 s: no further `[str]`, `[frame]`, `[gpu_vk]` or `[wide]` line at all.

Where the product is NOT, read live through the control surface while still stuck:

```
stage(0x801fe00c)=00000000  sm48(0x801fe048)=0  scene-active(0x800BE258)=00000000
otattr -> OT@0x800BD748: 1 drawing node, 0 attributed, 1 UNATTRIBUTED
disp  -> GP0(E3/E4) draw clip = (0,0)..(0,0)      (the game never programmed it on this leg)
```

## The boundary, and it is not where the state doc says the cause is

`docs/project-state.md` S018 reads as though arming the CD interrupt was the fix and the remaining
work is a sector handoff. Two measurements say the arming changed the *symptom* and the cause is
still downstream of it.

**1. The registered chain element is VBlank, and the CD bit is not claimed by it.** Live read of the
element the guest registered:

```
rw 800C1528 8
800C1528: 00000000 80087660 800875F8 00000000 ...
                 ^handler   ^verifier
```

`0x80087660` is not a CD-ROM handler. From the image (Ghidra, `FUN_80087660`, body
`[0x80087660, 0x800877EF)`): it tests `DAT_800B12C8[10] & 2`, walks the VBlank record list through
`FUN_80087900` / `FUN_80087C34` at `DAT_800B1298 + index*0xF0`, calls `(*DAT_800B1264)(0xFFFF)`, and
writes **`0x88` to `DAT_800B12C8 + 0x0E`** — `GP1(0x88)`, the display-area-start command a VBlank
handler issues. So the one chain element is VBlank, and the framework says so itself, at the moment
of delivery:

```
[irq] pending I_STAT&I_MASK=0x004; no SysEnq element claimed it (1 in chain), custom exception
       exit installed
```

IRQ2 is now deliverable — that part of the arm worked — and the element that receives it declines
it, so the CD source is routed to the custom exception exit instead. The guest's CD-ROM channel
service is not entered.

**2. The channel is armed and never started.** This is the finding. Live device state, same run:

| register | value | meaning |
|---|---|---|
| DPCR `0x1F8010F0` | `0x3B3B3BBB` | all seven channels enabled, master enable set |
| DICR `0x1F8010F4` | `0x009A0000` | bit19 = **channel-3 flag set**, bit23 master enable |
| MADR `0x1F801080` | `0x800B1090` | a main-RAM destination was programmed |
| BCR  `0x1F801084` | `0x00010020` | 32 words x 1 block |
| **CHCR3 `0x1F8010F8`** | **`0x00000000`** | **no start bit, no direction, no trigger** |

The guest armed DICR channel 3 and DPCR channel 3 and programmed MADR and BCR, and then the channel
was never started. `PSXPORT_DEBUG=dmairq` agrees in the negative: the run logs `DMA0`, `DMA2`,
`DMA4` and `DMA6` completions and **no `DMA3 complete` at all**, so the framework never owed a
channel-3 callback and the guest's DMA callback never ran.

The DICR arm is attributed, from `PSXPORT_DEBUG=dmairq`:

```
w DICR0[4] = 00920000 -> 00920000 (armed channel mask 12) ra=800824DC
w DICR0[4] = 009A0000 -> 009A0000 (armed channel mask 1A) ra=80086D90
```

`0x92 -> 0x9A` adds bit 3, channel 3, and `ra = 0x80086D90` is inside libstr — the sector-arrival
side, next to the callbacks this repository already owns at `0x800860B4` / `0x80086C80` /
`0x80086C94`. So the guest did everything except start the transfer, from the right code.

**3. Nothing drains the controller.** `dataAvail=2340`, `dataRead=0`, on every one of the title's
own `cdready` samples (17 of 17, then every 2500th poll), and `cdcIrqSeq = 1` — **one controller
response for the entire run.** Corroborated from the other side: with `PSXPORT_DEBUG=cdcr,cdcw` on
for 25 s and 474M instructions executed, the guest's entire CD-register traffic is seven writes
during command issue plus exactly **one** read of the index register, at the title's own stream
pump:

```
[cdcr] r[1800]=3B bank=3 status pc=80086B10 ra=8002B3E0
```

`pc = 0x80086B10` is `StGetNext`, `ra = 0x8002B3E0` is inside the movie player. **`0x1F801802`, the
data FIFO, is never read: 0 reads.**

**4. And the ring is empty because of it.** The STR ring holds only its initialisation fill. Live:

```
rw 80148AD4 32
80148AD4: 00000000 33333333 33333333 33333333  33333333 33333333 33333333 33333333
         00000000 33333333 ... (48 slots, only the 0x33333333 fill that CD-driver init writes)
```

**5. The named `0x190` is confirmed, and it is a TIMEOUT, not a delivery.** Live read of the
guest's own global, twice, in two runs:

```
rw 800C63CC 2
800C63CC: 00000190 0000EE98
800C63CC: 00000190 00003595
```

`DAT_800C63CC = 0x190 = 400` and `DAT_800C63D0` is a different root-counter-2 value in each run, so
the guest re-arms the same wait over and over. That is the retry loop. From the image:

* `FUN_8008DCC8` (body `[0x8008DCC8, 0x8008DCE7]`) is `DAT_800C63CC = a0; DAT_800C63D0 = RCounter2` —
  arm a timeout of `a0` RCounter2 ticks from now. Its twelve callers pass timeouts: `0x3C` (60) from
  the memory-card path `FUN_8008E018`, `0x190` (400) at the frontier.
* `FUN_8008DCE8` is the expiry test: it reads `RCounter2` (`0x1F801120`, mode `0x1F801124`, target
  `0x1F801128`), adds the target or a 16-bit wrap when the counter is below the start, subtracts
  the start (or a third of it when bit 9 of the mode is set) and returns
  `DAT_800C63CC <= elapsed`.
* `FUN_800881AC` is the wait itself: `*I_MASK = 0xFFFFFF7F`, then poll the word at
  `DAT_800B12C8 + 4` for bit 7, timing out through `FUN_8008DCE8`.

**So `0x8008DCC8(0x190)` is a root-counter deadline on a CD-controller busy poll. It is not an
interrupt-enable thunk and not a BIOS B-vector hook**, and the recorded reading of the sector
handoff as an interrupt-delivery problem is the same error class the workspace already corrected
twice: a `lui`-addressed global read out of a RAM dump and interpreted from its role in the log
rather than from its dataflow.

## Where the owner belongs, and why I did not write one here

The transfer is started by the guest's own CD-ROM channel service, reached on a CD interrupt. The
framework's `Hle::irqPoll` already reports the exact situation and already routes it — to the
**custom exception exit**, which is the BIOS-hook mechanism, not the SysEnq chain. What is missing
is that the custom exit does not land in the guest's CD-ROM channel handler, so `CHCR3` stays 0.

That owner is a framework seam (`runtime/psx/hle_interrupt.cpp` / the BIOS custom-exit dispatch),
not a title override. Writing a spider1 override for it would be a second implementation of the
framework's interrupt-delivery owner, which `psxport/AGENTS.md` forbids, and it would be a tap:
it would make `dem1` reachable while the guest's own CD interrupt service still never runs, so the
next CD-dependent thing would fail the same way for the same reason.

The title-side facts this issue establishes that the framework work needs, all from the image:
the registered element at `0x800C1528` is `{0, handler 0x80087660, verifier 0x800875F8, 0}` and its
handler is VBlank; the guest's DICR arm comes from libstr at `ra=0x80086D90`; the wait it is stuck
in is `FUN_8008DCC8`/`FUN_8008DCE8`/`FUN_800881AC` with a 400-tick RCounter2 deadline and
`I_MASK = 0xFFFFFF7F`.

## What the run also fixed

**The live control surface did not exist on this title, and that is why the frontier had to be read
out of an I_STAT transcript.** `game/core/spider_port.cpp` owns its own frame loop and never enters
`native_boot`'s, so it never called `DbgServer::attach`, and `PSXPORT_DEBUG_SERVER` did nothing:
one bounded run with the knob set produced no `dbgsrv` line, bound no port, and answered all 46
client attempts over 90 s with `Connection refused`. The consequence is not cosmetic — `guest` is
the only command that reports translated blocks, executed instructions, cache hits and misses,
invalidations and interpreter fallback **by reason**, and every number above would otherwise have
had to come from a 1.2 GB log of the guest polling `I_STAT` in a spin. `store_observe_attach` was
missing for the same reason on the same spine. Both are now attached in `runPort`, using the
framework's own owners; with the knob unset `attach` returns the requested cap unchanged and starts
nothing, so a player's run is the same loop it was.

## A second, separate finding: 14.7M invalidation requests against 1,494 translated blocks

`invalidations` climbs to 14,679,123 in 100 s (~267k/s, rising linearly) while `cache_misses` stays
flat at 1,497 and `translated_blocks` at 1,494. So the requests are not causing re-translation —
which means `guest`'s single `invalidations` number is counting **candidates, not overlaps**, and
`psxport/AGENTS.md` requires "invalidation candidates/overlaps" as two numbers. At 9,827 requests
per translated block this is worth knowing before anyone reads the figure as 14.7M blocks dropped.
Not a spider1 defect and not fixed here.

## Widescreen cull contract — a tool, and an unresolved half

`tools/probe_cull_census.py` establishes the record-based owners (39 distinct sites against the
cell at `0x800B5918`, cross-checked as a lower bound against Ghidra's 27 references) and states in
its own closing line that it cannot decide the 372 `addiu`/`ori` literal sites. That silence is
what Crash 1 read as "widening cannot clip new geometry", and it was wrong: the bound there was a
main-RAM global, 1 writer and 20 readers, used as the GTE NEAR PLANE.

`tools/probe_global_bounds.py` (new, `--selftest` 6/6, registered as
`spider1_global_bounds_selftest`) is the instrument for that shape. On SLUS_008.75 it scans
186,741 instruction words in `[0x80010000, 0x800C65D4)`, decodes 179,634 and is refused 7,107, and
names **843** main-RAM globals through a `lui` + 16-bit-displacement pair, of which **213 have both
a writer and at least one reader**.

**213 is a candidate list, not an answer, and I did not resolve it.** Which of them, if any, bounds a
projected coordinate, and along which axis, is the comparison's dataflow, which the tool does not
compute and says so in its own output. The tool also has a blind spot that makes it an *additional*
form and not a superset: it names a global through `lui` + `addiu`/`ori` on the base register, so it
does **not** see the `lui` + `lw`-at-displacement form — `0x800B5918`, the viewport record cell
itself, is absent from its output for exactly that reason. So:

* the horizontal-window contract is established for every owner that reads the record (9 named in
  `spider1_widescreen.h`, all reached through the cell, all shifted by the same margin as `OFX`);
* whether some other main-RAM global bounds horizontally on this title is **NOT established**, and
  the widening's safety argument currently rests on the record-based owners alone.

Recorded as remaining, not as a pass.
