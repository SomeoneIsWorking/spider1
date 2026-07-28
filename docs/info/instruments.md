# Instruments ledger — can the tool be trusted to show the OTHER answer?

A broken instrument fails **silently**: "no signal" and "instrument returning nothing" are identical
on screen. Uniform output — all-black frames, all-zero dumps, "no diff", an empty log — is the tell.

Validate an instrument by feeding it a case that **must** differ and watching it say so. When one is
caught lying, mark it distrusted and re-check every result that used it.

---

## INST-01 — `PSXPORT_DEBUG=vsync` (VSync call log) — **trusted**

*What it shows:* every `VSync(mode)` the guest makes, with the caller's `ra` and the current vblank
counter. Implemented in `game/core/sync_native.cpp`.

*Validated:* it distinguishes the two answers it exists to distinguish. It reported a **427,643 to 1**
split between query and blocking calls — not a uniform stream — and reported *different* `mode`
values and *different* `ra` values within the same run. An instrument that could only ever print one
mode would have been indistinguishable from the truth here, so this mattered: the split is what
established CLAIM-02.

*Known limit — read this before citing it:* it can only observe calls the port actually reaches, and
the port currently stops in libcd during boot. So it measures the **boot** call pattern, not the
gameplay loop. Do not cite it as evidence about gameplay pacing.

---

## INST-02 — `tools/redump_ram.py` + the framework's `disasm.py` — **trusted**

*What it shows:* the guest instructions at any address, from the retail executable.

*Validated:* it produced *different, structurally coherent* disassembly at every address queried —
a recognisable Sony crt0 at the entry point, a stable-read counter loop at `0x80084BE0`, a countdown
loop at `0x80084D58` — rather than uniform garbage, which is what a wrong load-address mapping would
produce. Cross-checked independently: the string pointer the disassembly showed being loaded
(`0x80096020`) resolves to the diagnostic text `VSync: timeout`, confirming both the byte mapping and
the function identification at once.

*Known limit:* it lays down only the executable's `.text` at load time. Anything the game pages in
from `CD.WAD` at runtime is **not** in the image and will disassemble as zeros. Zeros mean "not
present in this image", not "no code there".

---

## INST-03 — The watchdog backtrace (`PSXPORT_WATCHDOG=<sec>`) — **trusted, with a caveat**

*What it shows:* where the process was when no frame got presented in time — the framework's stall
diagnostic.

*Validated:* it distinguished states rather than always saying the same thing. Across the port's
three successive stalls it named three *different* call chains (the libetc VSync deadline loop, then
the libcd chain, then the CD wait polling the real-time clock), and each pointed at a cause that,
once addressed, moved the stall somewhere new. That is the behaviour of a working instrument.

*Caveat that cost time here:* it is a **single sample**, so it cannot by itself distinguish "spinning
in a tight loop" from "making slow progress". Confirm which by sampling more than once and comparing
— an identical backtrace across independent runs is the spin signal. Attaching an external sampler
(`gdb -p`, `eu-stack -p`) did **not** work in this environment; repeated runs are the practical
substitute.
