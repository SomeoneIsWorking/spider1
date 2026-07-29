# Claims ledger — what was "proven", with its evidence and whether it STILL HOLDS

A result cited as proof ("verified", "measured", "matches") rots, and a rotten claim is worse than
none, because work gets built on it. Every claim here carries the evidence it rests on and an
**expiry** — the concrete thing that would falsify it. A claim with no stated falsifier is a belief,
not a result.

When a claim is falsified: mark it, and **grep for who relied on it** — the damage is always
downstream.

---

## CLAIM-00 — "1580 functions recompiled, zero dispatch misses" — **FALSIFIED 2026-07-28**

*Was claimed:* the first recomp of this game produced a trustworthy 1580-function substrate, and the
port booting through it with zero `rec_dispatch` misses corroborated the boot seam.

*What falsified it:* that run used psxport `f2af64e9`, before the framework stopped shipping a
hardcoded seed list. The recompiler was therefore seeded with **Tomba!2's** addresses, and measuring
them against this game's text (`[0x80010000,0x800C6800)`) shows **27 of the 34 land inside it**. Each
declares a bogus function entry at an arbitrary offset, splitting a real function in two. The
substrate was corrupt, so the function count described nothing meaningful and "zero misses" was not
evidence about this game.

*Who relied on it — checked:* CLAIM-01 cited the miss-free boot as corroboration, and RE-00/RE-01 in
the frontier cited the function count. Both were re-derived against a clean substrate; see CLAIM-01
and CLAIM-04. Nothing else depended on the figure.

*Superseded by:* CLAIM-04.

---

## CLAIM-06 — "Nothing fills the CD dispatch table at `*0x800B390C`" — **FALSIFIED the same day**

*Was claimed (committed 2026-07-28):* the pointer at `0x800B390C` ships as `0`, no instruction ever
writes it, and the descriptor it should point at is one the BIOS is expected to install — making that
the next RE step, upstream of interrupt delivery.

*What falsified it:* `*0x800B390C` reads **`0x800B38EC`** in the load image (`ram.bin` offset
`0xB390C`, regenerable via `tools/redump_ram.py`), pointing at a fully populated struct that ships in
`.data` immediately before it. It is libcd's own low-level dispatch table — slot `+0` points at the
rcsid string for `intr.c`, so the binary names it. Confirmed live too: the `0x800B3DF0` store watch
reports hits with `pc=0x8008BBD0`, which is slot `+0x08`.

*How the error was made, which matters more than the error:* the "zero stores" measurement was real
and correctly performed, with a validated positive control. **The failure was interpretation** —
zero stores plus loads that work is the signature of static linker initialisation, and I read it as
a missing publisher. Worse, the supporting sentence "the load image ships it as `0`" was **never
measured**. I asserted it and then reasoned from it, and everything downstream inherited the error:
the claim that `CdInit`'s stores land in low RAM was inference from that assumption, not observation.

*Second error, independent:* the two `CdInit` stores were attributed to the wrong `jal`. Both sit in
**branch delay slots** and therefore store the `$v0` from before their call. See WART-05.

*Who relied on it — checked:* one commit (the frontier's "next RE step is that vtable") and the
WART-05 section; both corrected in the same pass. No code was written against it — the sequencing
rule of working the frontier's `next` step is what limited the damage to documentation.

*Lesson worth keeping:* **state which sentences are measured and which are inferred.** A single
unmeasured assertion, laundered through a real measurement, produced a confident wrong conclusion
that then set the project's next step.

---

## CLAIM-07 — "0x800A5130 is the driver-filled pad buffer" — **FALSIFIED within the hour**

*Was claimed (committed 2026-07-29):* `padSlot0Buf = 0x800A5130`, on the strength of two things —
the pad-polling routine reads its button mask from `0x800A5132` (+2, active-low, which matches the
framework's documented `buf[2]` contract exactly), and a scan of the decompiled corpus found **reads
only, no writes**, which was taken as confirming a driver-filled buffer rather than a game copy.

*What falsified it:* a runtime store watch (`PSXPORT_WWATCH`) reports **62,114 writes** to
`0x800A5130`, all from `0x8006B3C8`. The write goes through a POINTER, so a static text scan for
writes to that address could never have seen it. `0x8006B3C8` is an 8-byte copy; `0x800A5130` is the
game's own per-frame COPY of the pad state, and the real driver-filled buffer is its SOURCE at
`0x800A50EE`.

*Who relied on it — checked:* only the `GameConfig` entry and the RE-05 frontier note, both corrected
in the same pass. No behaviour depended on it, because no *correct* address was wired either.

*Correction to this entry's own reasoning (2026-07-29):* the sentence originally here — "wiring the
address changed nothing, because the framework's pad fill sits behind `padDriverFn`, which is still
zero" — **is false**, and it is fixed rather than left standing. `Pad::serviceFrame()`
(`pad_input.cpp:504-519`) writes the packet into `padSlot0Buf`/`padSlot1Buf` **directly**;
`padDriverFn` gates nothing, and in fact the framework never reads that field at all (WART-07). So
the wrong address WOULD have taken effect — it would have written pad packets over the game's own
mirror every frame. That makes the falsification more valuable than this entry first credited it.

*Resolved (2026-07-29) — the correct answer, and how the base-vs-`+2` ambiguity was settled:* pad
init `0x8006AE34` ends with `FUN_8008afbc(0x800A50EC, 0x800A510E)` = libpad `PadInitDirect(buf0,
buf1)`; the two arguments are `0x22` apart, the 34-byte direct-buffer size. The per-frame consumer
`0x8006B27C` confirms the layout independently: it walks 2 slots at stride `0x22` from `0x800A50EC`,
tests byte `+1` against `0x80` (libpad's multitap type nibble), and on the ordinary-controller path
copies 8 bytes from the buffer **base** — `{status, type, btn_lo, btn_hi, …}`, exactly the
framework's `fillBuffer` packet. On the multitap path it instead copies four sub-pads from
`+2/+10/+18/+26`, which is what made `+2` look like a slot base. So `padSlot0Buf = 0x800A50EC`,
`padSlot1Buf = 0x800A510E`; `0x800A50EE` is only the button halfword.
  `python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8006AE34 0x8006AE90`
*Expires if:* the game is ever seen taking the multitap path (buffer byte `+1` == `0x80`), which
would mean slot 0's real button data moves to `+2` and the port's single-packet fill is incomplete.

*The lesson, and it is the SAME one this project has now learned three times:* **a static scan proves
nothing about writes through a pointer.** "No callers" for `0x8008DA24` (installed into a table),
"`*0x800B390C` ships as 0" (linker-initialised `.data`), and now "no writes to `0x800A5130`". Every
one was an absence-of-evidence read as evidence-of-absence, and every one was settled in a single run
by a runtime instrument. **Reach for the store watch before concluding anything about writes.**

---

## CLAIM-08 — The offline module relocation reproduces the guest's own loader exactly — **holds**

*Claim:* `tools/extract_modules.py` — which reads the `CD.HED` index, pulls a module out of `CD.WAD`,
and applies the `.rel` relocation stream — produces byte-for-byte the image the RUNNING GAME produces
in RAM after its own loader has relocated the same module.

*Evidence, and it is a single decisive check rather than an argument:* relocating `shell.bin` offline
at its load base and comparing against a live RAM dump matches on **all 112912 bytes**, with the
`.rel` stream consuming exactly its 8416 bytes and terminating cleanly on its `0xFFFFFFFF`.

That one comparison validates four separate things at once — the `CD.HED` index walk (name,
`(nul+4)&~3` alignment, offset/size pair), the `CD.WAD` byte offsets, the relocation format
(`R_MIPS_32` / `HI16`+addend / `LO16` / `26`, type in the low 2 bits), and the load base. Any one of
them being wrong would have produced a mismatch.

*Reproduce:*

    python3 tools/extract_modules.py scratch/wad/CD.HED scratch/wad/CD.WAD scratch/overlays
    # then compare scratch/overlays/shell.bin against a RAM dump at the module's base

*Expires if:* a module is ever seen whose `.rel` stream does not terminate cleanly, or whose relocated
image diverges from guest RAM — which would mean the format has a case this implementation does not
handle (only types 0-3 are known to occur; a type outside that set would be the tell).

*Why it matters:* every one of the 30 modules is recompiled from this offline relocation. If it were
subtly wrong, the substrate would be built from bytes the game never actually executes.

---

## CLAIM-04 — The substrate is now seeded only from this game's own binary — **holds**

*Claimed:* the current recomp contains no foreign seed, so every recompiled function entry is one the
executable itself vouches for.

*Evidence:* psxport ≥ `9127e10e` ships no seeds and takes them via `--seeds`; this repo's
`game/recomp_seeds.json` is deliberately EMPTY, so discovery runs purely from the entry point, the
recompiler's pointer/table scans, and direct `jal` following. Result: **335 seeds → 1561 functions**,
against 355 → 1580 under the contaminated run. The seed file is a hash input to
`tools/ensure_recomp.py`, so a change to it forces a regenerate on every machine.

*Expires if:* a seed is ever added without a recorded rationale, or if the recompiler's own discovery
changes — either makes "every entry is binary-vouched" no longer automatic.

---

## CLAIM-05 — The guest stack lives in a RAM mirror, and the framework now models it — **holds**

*Claimed:* this game's stack is at `sp = 0x807FFFF8`, inside the fourth 2 MB mirror of PSX main RAM,
and before psxport `94118f85` every access to it was silently discarded.

*Evidence:* the executable ships `*0x800B3E70 = 0x00800000` as the stack-top constant, and crt0
computes `sp = (*that - 8) | 0x80000000`. `host_ptr` required `(addr & 0x1FFFFFFF) < 0x200000`, which
`0x7FFFF8` fails; NULL there falls through to the unmapped-I/O path. Directly measured: the
callee-saved register slot read 0 while the live register read 1, at an address confirmed by
observing the callee's own `sp` rather than deriving it.

*Verified by the fix:* lost-register count 25 → 0; CD commands `0x00` x26 → `0x01`/`0x0A` with zero
unhandled; Tomba!2 unchanged.

*Expires if:* a game is found whose accesses legitimately straddle the 2 MB wrap — the fix returns
NULL there rather than aliasing, which is deliberate but untested against real usage.

---

## CLAIM-01 — The crt0 boot group in `GameConfig` is correct — **holds (re-verified on a clean substrate)**

*Claimed:* every value in the crt0/boot group of `game/core/game_config.cpp` is the address the
retail executable actually uses.

*Evidence:* read instruction by instruction out of the crt0 at `0x8008739C` (disassembly reproducible
via `tools/redump_ram.py` + the framework's `disasm.py`; the per-field mapping is written out at the
definition). This evidence never depended on the recomp — it comes from the executable directly — so
CLAIM-00's falsification does not touch it. Independently corroborated: the framework's game-agnostic
`crt0_setup` implements this exact sequence.

*Corroboration re-established 2026-07-28:* the original "boots with zero `rec_dispatch` misses"
observation was made against the contaminated substrate and had to be discarded with CLAIM-00. It has
been re-run on the clean 1561-function substrate: the port still boots through crt0 into the guest's
`main` and executes real translated code down into `0x800649E4`. Same conclusion, honest evidence.

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

## CLAIM-03 — "Spider-Man has no overlay modules" — **FALSIFIED 2026-07-29**

*Was claimed:* the disc carries one executable (`SLUS_008.75`) plus the packed archive `CD.WAD`, the
ISO tree shows no per-stage `.BIN` images, and the recompiler reported `0 overlay module(s)`. All
three statements were TRUE and the conclusion drawn from them was FALSE.

*What falsified it:* the game loads **30 further code modules** at runtime out of `CD.WAD`, as
`<name>.bin` + `<name>.rel` pairs, relocated in place by its own loader `FUN_8001B990`. Boot reaches
them within seconds — the first call into `shell` aborted with a recomp MISS on `0x8014D5AC`, which
is past the executable's text end.

*The reasoning error, which is the part worth keeping:* "no overlay FILES on the disc" was read as
"all code is in the executable". Those are different claims, and the evidence only supported the
first. The recompiler's `0 overlay module(s)` was not independent corroboration either — it only
means the recompiler was never handed any, which was a consequence of the same assumption.

*Who relied on it — checked and corrected:* `game/recomp_seeds.json` (said overlays were N/A),
`game/core/recomp_register.cpp` (asserted the table was empty "not a gap"), `GameConfig::overlaySlots`
(same), and RE-00 in the frontier. All four now say the opposite, and all 30 modules are recompiled
and routed. See RE-09.

