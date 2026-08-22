---
id: I025
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

PSXPORT_FNTRACE=<addr>,... AS A REACHABILITY + OVERRIDE-ABILITY census (external/psxport/runtime/recomp/fntrace.cpp, wired last by SpiderRuntime::registerOverrides)

## Validated by

VALIDATED BOTH WAYS IN ONE RUN, 2026-08-06 (scratch/re12/logs/fntrace1.log): 8 addresses traced over a 100 s headless boot; six reported REACHED with per-site call counts (0x80061308=1761, 0x800612B8=1765, 0x8002C174=2, 0x80061140=1, 0x80061230=1, 0x800604CC=1) and TWO reported 'NEVER CALLED — control did not reach it in this run' (0x800160EC, 0x8006F294). So the instrument demonstrably prints both answers on the same run, and the zero is an explicit statement rather than an absence. It also reports its own denominator ('tracing 8 guest function(s); a zero count at exit means NEVER REACHED'). SECOND THING IT PROVES, and the reason it is worth citing for RE-20: fntrace works by INSTALLING A REAL OVERRIDE at each address and re-dispatching the body, so a non-zero count is direct evidence that the recomp override table reaches that address — i.e. it answers 'can the port hook this?' as well as 'does the game call this?'. LIMITS, from the file's own header and observed here: it claims the override slot, so never trace an address whose override is doing real work; MAIN-module entries only (no overlay addresses); recursion is not counted; and it records only the FIRST call's ra, so it cannot attribute a multi-caller function's hits across its callers — 0x800612B8's 1765 hits could not be split between its six call sites this way.

## Known failure modes

(none recorded yet)
