---
id: 1
title: recomp-MISS 0x800C6684 — three CD.WAD modules live at one pinned slot
status: resolved
symptom: recomp-MISS 0x800C6684 no recompiled fn, resident overlay VENOM, abort during boot / black screen
tags: module-loader,overlay,recomp-miss,RE-09,RE-16
created: 2026-08-04
updated: 2026-08-04
---

**Root cause (measured 2026-08-04, two independent ways).** `game/core/module_loader.cpp` pins all 30
CD.WAD modules to ONE reserved slot (0x800C65EC), justified by "the game only ever has ONE module
resident". That premise is FALSE — see docs/info/claims.md CLAIM-09.

The guest loader FUN_8001B990 keeps a doubly-linked LIST of loaded modules. On a plain boot the
sequence is SHELL/THUG/SHELL/COP/SHELL/SHELL, each bracketed by an unload — and then **L5A5LSC,
LIZMAN, VENOM back to back with NO unload**. The RAM dump at the abort holds all three descriptor
nodes with body=0x800C65EC. VENOM overwrote LIZMAN; LIZMAN`s node still holds node[+0x0C]=0x800C6684,
which in VENOM is `lw $s0,0x10($sp); jr $ra; addiu $sp,$sp,0x20` — an EPILOGUE, not an entry.

**So it is the "stale pointer / wrong overlay resident" horn**, not a function-discovery gap.
`resident overlay = VENOM` in the miss message is CORRECT (RAM matches venom.bin 100.00% over 37364
bytes; shell 69%, every other module ~13%).

**DEAD END — do not do this:** treating 0x800C6684 as a function entry in the resident module. It is
VENOM's epilogue under a stale LIZMAN pointer; dispatching it only hides the residency error while
executing the wrong module's code.

**Instrument:** `python3 tools/check_module_slot.py scratch/raw/miss_ram.bin` (exit 1 today);
`--selftest` proves POSITIVE/NEGATIVE/VOID are all reachable.

**Runtime detector:** `warn_if_coresident` in game/core/module_loader.cpp, non-fatal by design
(aborting there would stop the port earlier than it stops today).

**Not fixed.** The fix needs distinct bases for concurrently-live modules; options and their costs are
in docs/re-frontier.md RE-09.

**Also observed, and probably a SEPARATE bug:** every presented frame is black from frame 1 to frame
1600+ (PSXPORT_SHOT_AT, all 100.0%% pure black), and the display config collapses to 512x2 around
frame 150-400 before returning to 512x240. The user reports HEARING the Activision logo, so the FMV
path is live and this is video-side. Not investigated here.

### Resolution (2026-08-04)
ROOT CAUSE was the PORT, not the guest: game/core/module_loader.cpp pinned all 30 CD.WAD modules to one guest address on a premise measured from a single transition. Ghidra decomp of FUN_8001BEC4 showed the guest correctly finds LIZMAN's still-live node and calls LIZMAN's own method table — the bytes underneath had been replaced by VENOM's image.

FIX (2026-08-04): the modules are emitted BASE-RELATIVE and the game's own allocator places each body, exactly as the console does. The recompiler renders every address that moves with a module as 'link address + per-Core delta' at four site classes (HI16 lui results, router constants inside the module's own range, jal/jalr/branch-link values, the recovered jump-table switch); the framework routes dispatch through a live (base,size,module) registry the loader intercept feeds. NOTHING is pinned and NOTHING is redirected — the port only observes where the guest put each module.

VERIFIED: before = SIGSEGV at the first multi-module load, 0 frames. After = 7176 frames in 120s, 0 recomp-MISS, with L5A5LSC/LIZMAN/VENOM live simultaneously at 0x8014A6D0 / 0x801BDA30 / 0x801C6238.

DEAD ENDS, measured, do not retry: module bases outside the 2MB guest window (Core::host_ptr masks &0x1FFFFF and the PSX ordering-table link field is 24-bit by format); 30 distinct FIXED bases (the game's own allocator OOM-panics at 0x8008ACFC on the first load).
