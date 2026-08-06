---
id: I027
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

The render seam's own reach + scene census (PSXPORT_DEBUG=rseam / the unconditional [rseam] and [scene] lines, game/render/render_seam.cpp)

## Validated by

WHAT IT ANSWERS: (a) did the override at 0x80061308 actually install and RUN, (b) what scene identity did the game hold when it ran. WHY IT IS NOT AN atexit SUMMARY: this port is killed by SIGTERM and the watchdog owns SIGINT/SIGTERM and calls _exit(130) (external/psxport/runtime/recomp/watchdog.cpp on_interrupt), so atexit NEVER runs — an end-of-run summary would silently not exist. The evidence is therefore emitted DURING the run: an unconditional line at install, an unconditional 'submitFrame override REACHED' on the first call carrying frame + ra + leg + scene, and another unconditional line every 512 calls carrying the running call count and the running scene-change total. A run killed at any instant still leaves its denominator in the log. THE NEGATIVE IS DESIGNED, not incidental: the install line says in as many words that the absence of a REACHED line means the override never ran, and install REFUSES (logging an error) if rec_func_index() says the address is not a MAIN-module function entry — the exact failure mode that makes an override silently never fire. BOTH CLASSES OBSERVED 2026-08-06: POSITIVE — 'REACHED — call #1 at frame 2, ra=80061218' in every run this session, matching C029's independent fntrace measurement of first-call-at-frame-2-from-ra=80061218 to the digit; NEGATIVE for the census half — the very first sample reports raw=00,00,00,00 printable=0 (name not yet written) and only later 'dem1' then 'l1a1', so the census can and does report 'no identity yet' rather than inventing one. BLIND SPOTS: it counts only NON-RECURSIVE entries (the override is uninstalled while the super-call runs), it sees only submitFrame and therefore says nothing about intro-FMV frames (which do not use this pair — RE-19), and the periodic line's cadence is calls, not time, so a stalled run stops updating it.

## Known failure modes

(none recorded yet)
