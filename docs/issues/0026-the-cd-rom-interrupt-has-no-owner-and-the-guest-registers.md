---
id: 26
title: The CD-ROM interrupt has no owner at all — the guest registers a BIOS event class the framework never raises, and the BIOS CD-ROM handler does not exist
symptom: reaches retail STR movie field 1 at 0x8002AC8C and presents it, then spins; 4,199
         presented frames, 0 submitted prims, 0.00% non-black, DMA channel 3 armed once and never
         started
state_items: S004,S013,S018,S019
tags: cd,bios,openevent,event-class,interrupts,root-cause,frontier,s018
created: 2026-09-28
updated: 2026-10-09
---

> **§5's conclusion is FALSIFIED BY MEASUREMENT — read §9 before using §5.** §5 says restoring the
> guest's own interrupt-source registration "would still leave IRQ2 with no service, because the
> framework does not walk `0x800B2888` either". The framework does not walk it — **the guest does**,
> from `FUN_8008BA00`, and it is the thing standing between field 1 and field 2. §9 names the real
> cause with the disassembly and the live guest words. §5 is retained for the provenance of the
> framework-side work it proposed, not as the current diagnosis.

> **Resolved 2026-10-09.** The CD interrupt is delivered to the guest's own handler (`0x8008BA00` ->
> `0x8008DA24` -> `0x8008C3E0`), which the title's CdInit arms. What stalled the stream was the title's
> native override of `0x8008C3E0` dropping the controller acknowledge, and the missing DMA callback
> table base; see issue 0024. The `0x8008DCC8(0x190)` named in the state doc is the SIO pad
> transmit timeout (`FUN_80087F34`, I_STAT bit 7), not a CD handoff. §5's proposal to add a CD arm to
> `Hle::deliverEvent` is withdrawn.

## Answer

**There is no owner for the CD-ROM interrupt anywhere in the product, and there never was one.** Not
a mis-armed mask, not a declined verifier, not a missing BIOS libc leaf: the guest's CD-ROM service
is a **BIOS event class the framework has no arm for**, and the BIOS's own CD-ROM interrupt handler
— the thing that on hardware reads the controller response and calls the game's CdReadyCallback — is
ROM code this port does not have. Issue 0024's "armed and never started" and issue 0025's black pair
are both downstream of this.

## 1. `I_STAT&I_MASK=0x004` is IRQ2, the CD-ROM, and that part was right

From the framework's own interrupt-source header, `external/psxport/runtime/psx/irq_edge.h`:

```
IRQ_BIT_VBLANK = 0, IRQ_BIT_GPU = 1, IRQ_BIT_CD = 2, IRQ_BIT_DMA = 3,
IRQ_BIT_TIMER0 = 4, ... IRQ_BIT_SIO = 7, IRQ_BIT_SPU = 9, IRQ_BIT_PIO = 10
```

`0x004` is bit 2, and `io_peripherals.cpp:149` sets it with `game->hle.i_stat |= 1u << 2`. So the
pending source is the CD-ROM. **Correction to my own first draft of this note:** the two published
I_STAT layouts (the hardware register list and this header) **disagree above bit 3** — the hardware
list calls bit 4 "pad" where psxport calls it timer0. `tools/probe_spider1_headless_run.py` had the
hardware list and now carries the framework's, because that is the header the delivery gate uses.
Bits 0..3, the only ones this turns on, agree in both.

## 2. The CD channel's state over frames, from the guest's own words

One bounded headless run, `4:3`, `fps60=0`, sink 960x720, offscreen, silent, unpaced, one captured
PID, 4,199 presented frames reached, `executed_instructions=88,891,395`, `faults=0`, and all six
`refused_*` fallback reasons 0.

**Interrupt side** (`PSXPORT_DEBUG=irq`, 774,402 lines — the flood is the guest's own spin):

| event | count | value |
|---|---|---|
| `CdInit` arm | 1 | `I_MASK 0x009 -> 0x00D`, device write at `0x1F801074` |
| delivery to the one chain element | **4,044** | `I_STAT&I_MASK=0x001` (VBlank) |
| delivery to the one chain element | **15** | `I_STAT&I_MASK=0x005` (VBlank **+ CD-ROM**) |
| unclaimed | 1, then silent | `pending I_STAT&I_MASK=0x004; no SysEnq element claimed it (1 in chain)` |
| guest ack writes `I_STAT=0x0FE` (clears bits 1..7, **including bit 2**) | 4,059 | from `ra=0x80086D90` |

The chain element is `{0, handler 0x80087660, verifier 0x800875F8, 0}` at `0x800C1528`, and
`0x80087660` issues `GP1(0x88)` — a **VBlank** display-area start. It is not a CD-ROM service, so the
15 times it was handed the CD bit it declined, correctly.

**DMA side** (`PSXPORT_DEBUG=dmairq`), the whole run:

| guest write | value | framework's armed-channel mask | `ra` |
|---|---|---|---|
| 1 | `0x00000000` | `0x00` | `0x8008B88C` |
| 2 | `0x00900000` | `0x10` | `0x8008EF4C` |
| 3 | `0x00920000` | `0x12` | `0x800824DC` |
| 4 | `0x009A0000` | `0x1A` | `0x80086D90` |

`0x92 -> 0x9A` adds bit 3: the guest arms **channel 3 once**, from libstr. Completions over 4,199
frames: `DMA0 x2, DMA2 x5, DMA4 x1, DMA6 x2` — **`DMA3 x0`.** `PSXPORT_DEBUG=cdcr` produced **zero**
lines, so `0x1F801802`, the controller's data FIFO, is read **0** times. The controller is never
drained.

## 3. The guest DOES ask for data — so the missing start is a consequence

`PSXPORT_DEBUG=cd`, all eight of the guest's CD requests, in order:

```
CdRead 1 sector  x 2048 from LBA 16     -> 0x800C0CE0 (mode 0x80)
CdRead 1 sector  x 2048 from LBA 18     -> 0x800C0CE0 (mode 0x80)
CdRead 1 sector  x 2048 from LBA 22     -> 0x800C0CE0 (mode 0x80)
CdRead 7 sectors x 2048 from LBA 390    -> 0x800B9E68 (mode 0x80)
CdRead 1 sector  x 2048 from LBA 128303 -> 0x800C0CE0 (mode 0x80)
CdRead 1 sector  x 2048 from LBA 22     -> 0x800C0CE0 (mode 0x80)
CdRead 71 sectors x 2048 from LBA 397   -> 0x800FCA9C (mode 0x180)
```

The 71-sector STR read is issued. **The guest asks, and the answer is lost upstream of the transfer.**
So "armed and never started" is a consequence, not the cause, and the cause is the absence of any
CD-ROM interrupt owner.

## 4. The cause, from bytes

### 5a. What the guest's CD-ROM service actually is

Retail `CdInit` at `0x8008A16C`, read from the executable:

```
0x8008A16C  addiu $sp,$sp,-24 ; s0 = 4                    ; retry up to 4 times
0x8008A17C  v0 = 0x8008A1FC()                             ; the BIOS CdInit attempt
            if (v0 != 1) { s0--; if (s0 != -1) goto 0x8008A17C; return -1 }
0x8008A190  [0x800B3B14] = 0x8008A238                     ; CD sync callback slot
0x8008A1A0  [0x800B3B18] = 0x8008A260                     ; CD ready callback slot
0x8008A1B0  [0x800B1C7C] = 0x8008A288                     ; CD event callback slot
            [0x800B1C80] = 0
0x8008A1EC  return v0
```

And the three installed callbacks:

```
0x8008A238  a0 = 0xF0000003 ; jal 0x8008F9D0 (B0:0x07 DeliverEvent) ; a1 = 0x20
0x8008A260  a0 = 0xF0000003 ; jal 0x8008F9D0 (B0:0x07 DeliverEvent) ; a1 = 0x40
0x8008A288  a0 = 0xF0000003 ; jal 0x8008F9D0 (B0:0x07 DeliverEvent) ; a1 = 0x40
```

`0xF0000003` is the PSX's **HwCD** class. The framework already names the neighbouring ones in its
own source: `mem.cpp:984` — *"0xF0000009 is the PSX's fixed HwSPU class, hardware nomenclature rather
than a game value"*, and `memcard.cpp:405-417` raises `0xF4000001` (SwCARD) and `0xF0000011`
(HwCARD).

`0x800B3B18` is the BIOS's `CdReadyCallback` slot, and `0x80086C94` is the guest's get/set body for
it. On hardware the chain is: controller raises IRQ2 → **the BIOS's own CD-ROM interrupt handler**
reads the controller response → **calls the function at `0x800B3B18`** → that is
`DeliverEvent(0xF0000003, 0x40)` → the game's libcd event handler runs → it starts DMA channel 3.

### 5b. `Hle::deliverEvent` has no CD arm, and nothing calls the ready callback

`runtime/psx/hle.cpp` owns `Hle::deliverEvent`. Its callers, whole repository:

| class | raised by |
|---|---|
| `0xF4000001` SwCARD | `memcard.cpp:405,416` |
| `0xF0000011` HwCARD | `memcard.cpp:406,417` |
| `0xF0000009` HwSPU | `mem.cpp:987` |
| **`0xF0000003` HwCD** | **nowhere** |

And the BIOS CD-ROM interrupt handler itself does not exist: `cd_override.cpp` has
`cd_drive_stock_read`, which drives the ready callback, but it is reached only from the stock-read
path, not from an interrupt.

### 5c. The title declares the delivery the framework promised, and the promise is in framework source

`spider1_platform_facts.h:48`:

```cpp
inline constexpr GuestCdStreamCallbackLayout cdStreamCallbacks{
    .readyCallbackPointer = cdReadyCallbackSlot,                       // 0x800B3B18
    .owner = GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt,
};
```

`Spider1Runtime::guestCdStreamCallbackLayout()` returns it, and `cd_override.cpp:866-874` acts on it:

```cpp
if (!c->cfg && layout && layout->valid() &&
    layout->owner == GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt) {
  // The controller, not the host pump, raises INT1. The guest libcd ISR must consume its response
  // before invoking the ready callback; **irqPoll delivers it at a safe boundary after this native
  // call returns.** ...
  game->timing.serviceCdcTickSource();
  return;
}
```

**The framework disables its own working path on the strength of a delivery `irqPoll` does not
perform.** `Hle::irqPoll` offers `i_stat & i_mask` to the SysEnq chain, and its own comment is
already right about what that means:

```cpp
// Declining here only says the BIOS element chain did not own this source. Games commonly route
// CD-ROM through the custom exception exit and a separate master table, so do not misdiagnose a
// correct VBlank-only verifier as "the game has no CD service".
```

— and then the CD bit goes to the custom exception exit, where nothing walks `0x800B3B18`.

### 5d. The title's `CdInit` override also drops the guest's own registration

`Spider1FrameDriver::initializeCd` replaces the whole `0x8008A16C` body. The body's other half is
`0x8008A1FC` → `0x8008D4E4`, which is the CD-ROM controller initialisation and, at `0x8008D54C`,
`jal 0x8008B86C` with `a0 = 2`. **CORRECTION to `spider1_platform_facts.h:36-46`:** `0x8008B86C` is
**not** a B-vector thunk and not a BIOS call. From the executable:

```
0x8008B86C  lui $v0,0x800B ; lw $v0,0x390C($v0)      ; $v0 = [0x800B390C] = 0x800B38EC
0x8008B87C  lw $v0,8($v0)                             ; $v0 = [0x800B38EC+8] = 0x8008BBD0
0x8008B884  jalr $v0                                  ; call the GUEST's own 0x8008BBD0(2, name)
```

and `0x800B390C` is the title's **hardware-address table**, not a BIOS vector table:
`[+4]=0x1F801070` I_STAT, `[+8]=0x1F801074` I_MASK, `[+0xC]=0x1F8010F0` DPCR. So the comment's
"on a PSX B-vector entry 2 is the interrupt-enable call" is a step that does not exist; the conclusion
(`I_MASK` 0x009 → 0x00D) is still right because the framework's override writes the device directly,
but the stated reason was wrong.

`0x8008BBD0(2, name)` is the guest's own interrupt-source registration: it computes
`0x800B2888 + index*4`, reads a pointer, and compares it against the caller's `a1`; a miss takes the
`lhu` at `record-4` as a length. **The `0x800B2888` table is all zeros in the image** — it is
runtime-initialised, and the log shows the guest's own installs reaching it through
`B0:0x5B(0, 0x800B2888, 0x800B2884, 0)` from `0x8008BC94` and `C0:0x0A(3, 0, 0x800B2884, 0)` from
`0x8008BCA0`, both of which the framework serves as `v0 = 0` no-ops.

So the override omits the one effect that is the actual mechanism, and substitutes a mask bit for
it. That omission is a title-side defect. **It is not the blocking one**, because the framework does
not walk `0x800B2888` either, so restoring the guest's registration would still leave IRQ2 with no
service.

## 5. The fix: a framework finding, and why it is not a title one

**`Hle::deliverEvent` must have a CD-ROM arm, and `Hle::irqPoll` must invoke the BIOS CD-ROM
callback on IRQ2. Both are `psxport`, so this note proposes them and edits nothing.**

Proposed change, in `psxport/runtime/psx/`:

1. **`hle.cpp` — add `deliverEvent(0xF0000003u, spec)` to the CD data-arrival path.** The class and
   the spec values are hardware nomenclature, exactly as `0xF0000009` already is at `mem.cpp:984`.
   The right caller is the CD model's sector-arrival point, so the spec carries the guest's own
   `CdReadyCallback` status argument (the 0x20/0x40 pair `0x8008A260` and `0x8008A288` pass).
2. **`hle_interrupt.cpp` — in `irqPoll`, when `pending & IRQ_BIT_CD` and no SysEnq element claims
   it, read the BIOS CD-ROM ready-callback slot and dispatch it, instead of routing the bit to the
   custom exception exit and stopping.** The slot address is title data, so it belongs behind the
   `GameRuntime` seam the title already uses — `GuestCdStreamCallbackLayout::readyCallbackPointer`
   exists for it and is currently only read by `cd_override.cpp`. The read must be of the CURRENT
   slot value, because libstr replaces it during a stream (`0x800B3B18` is replaced by `0x800860B4`).
3. The generic name of (2) is the real gap: **psxport's BIOS HLE models the BIOS's SysEnq element
   chain but not the BIOS's own CD-ROM interrupt handler, and it has no seam for a title to supply
   one.** `GuestCdStreamCallbackLayout::DeliveryOwner::GuestInterrupt` is that seam, declared and
   unimplemented.

**Why not on the title side.** The record layout at `0x800B2888`, the comparison function
`0x8008BBD0`, the class number `0xF0000003` and the ready-callback slot `0x800B3B18` are all this
title's — but the thing that is missing is a **dispatcher for a BIOS hardware event class**, and
`Hle::deliverEvent` is the framework's owner of exactly that. A spider1 override for it would be a
second implementation of the framework's event owner, which `psxport/AGENTS.md` forbids, and it would
be a tap: the guest's own CD-ROM service would still never run.

**Explicitly rejected as fixes:**

* Flipping `cdStreamCallbacks.owner` to `HostPump`. That re-enables `cd_drive_stock_read` and would
  very likely produce sectors — and it is the exact error
  `runtime/psx/guest_cd_stream_callback_layout.h` warns about: *"Calling that callback from the pump
  first makes CdReady observe a stale libcd result."* This title is a stock-libcd consumer; its own
  ISR `0x8008C3E0` stages the controller response and must run first. That declaration is **correct**
  and stays.
* Keeping only `I_MASK |= 0x4` and adding a native "CD service" override at the title. Same reason:
  it makes `dem1` reachable while the guest's CD-ROM service still never runs.
* Writing the missing `OpenEvent` state into guest RAM from the host. That is manufacturing guest
  state.

## 6. Where the product actually is, and what was NOT verified

Reached: authenticated crt0, pre-main `ResetGraph` at `0x8008479C`, field callback `0x8005E510`,
`CdInit` `I_MASK 0x009 -> 0x00D`, the 71-sector STR read, retail STR movie field 1 at `0x8002AC8C`.
**Not reached: a second movie field, `dem1`, `l1a1`, any stage, any scene.** This is a logo/movie
checkpoint, which is not gameplay conformance.

| measure | value |
|---|---|
| submitted prims | **0** — `otattr`: `OT@0x800BD748: 1 drawing nodes, 0 attributed, 1 UNATTRIBUTED (0 spans)` |
| product's own present shot | `[present_shot] wrote scratch/screenshots/present_3500.png (960x720 headless sink) non-black 0/691200 (0.00%)` |
| guest draw clip | `GP0(E3/E4) = (0,0)..(0,0)` — the guest never programmed it on this leg |
| presented frames reached | 4,199 |
| executed instructions | 88,891,395 |
| translated blocks | 1,494 |
| faults | 0 |
| interpreter fallback, all six reasons | 0 |

**The standing measure is more than 2 submitted prims. It is 0, so the widening still has no picture
to widen, and issue 0025 stands unchanged.**

## 7. Two corrections recorded, because both were confident and wrong

1. **The BIOS-census instrument reported ZERO stubs** on its first run because a `jr` was recognised
   by testing `word & 0x1F` for the `rd` field — and bits 4..0 are the funct field's own low bits,
   which are `0x08` for every real `jr`. The census rejected 100% of them and would have read as
   "this title makes no BIOS calls". Both the accept and the reject cases are now pinned in
   `--selftest` (53/53).
2. **The framework-set parser read zero case labels out of `memcard.cpp` and
   `bios_pad_work_area.cpp`** because they write `case 0xABu:` and the pattern required a colon
   straight after the hex digits. That is a declared set silently missing libcard and the pad work
   area — 20 and 3 labels — and it would have reported a longer missing list than the truth.

Both are in the tool's own docstring, because the failure mode is reusable.

## 8. The control surface on a stuck run

**A third obstacle, and this one is a framework defect: the control surface wedges permanently
after its first command on a stuck run.** Measured here, from the same bounded run. A fresh
connection got one `frame` (2604, then 2699 on a later run) and then every `rw` timed out with **no
reply at all**, and the NEXT `create_connection` timed out too — the accept backlog is full, so the
serial `serve_conn` loop is still inside the previous connection. `runtime/psx/dbg_server.cpp:842-862`
explains it, and the mechanism is in its own source:

```cpp
while (s_req_pending || s_resp_ready) {
    if (pthread_cond_timedwait(&s_done, &s_mtx, &ts) == ETIMEDOUT) { ... goto timeout; }
}
snprintf(s_cmd, sizeof s_cmd, "%s", line);
s_req_pending = 1; s_resp_ready = 0;
while (!s_resp_ready) {
    if (pthread_cond_timedwait(&s_done, &s_mtx, &ts) == ETIMEDOUT) {
      s_req_pending = 0; // abandon: main may service a stale slot harmlessly
      pthread_mutex_unlock(&s_mtx); goto timeout;
    }
}
```

**The abandonment path clears `s_req_pending` and leaves `s_resp_ready` set.** The comment says the
main thread "may service a stale slot harmlessly", but the main thread setting `s_resp_ready` on a
request the server thread has already abandoned is exactly what makes the NEXT request's guard loop
spin to its own timeout forever. The endpoint then serves one command per process, for the rest of
the run. So on this title the live `rw` channel is **not a usable measurement surface in the stuck
state** — which is why the CD-channel numbers in section 2 come from the product's own `irq` and
`dmairq` channels over 4,199 frames rather than from sampled register reads. That is a fallback, and
it is a weaker instrument than a register read: it reports what the devices logged rather than the
values sitting in them at one instant. **The CHCR3/DPCR/DICR/MADR/BCR values quoted in issue 0024
were taken before this wedge and I did not re-derive them; I am not restating them as my own
measurement.**

Measured while writing it, and worth recording because it changes how any stuck-state run must be
driven: **`PSXPORT_DEBUG=irq` wrote 1.28 GB in 90 s and `bios,cd,irq,dmairq,cdcr` wrote 4 GB**, because
the stuck guest spins on `I_MASK` and every read is logged. At that volume the product stops servicing
its own control surface, so a measurement that needs the surface cannot be taken at all. The
endpoint's `dbg_submit` also blocks until the main loop's `service()` runs, so a stuck turn answers
roughly one command per tens of seconds: a nine-read sample did not finish in 250 s, and the same
data batched into four capped reads does. Sample by `frame >= next_sample`, never `frame % N == 0` —
unpaced, the counter passes 3,000 before one round trip returns, so an exact modulo is a coin toss
and a fixed frame number is a number that gets skipped.
