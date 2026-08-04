---
id: C012
kind: claim
status: holds
created: 2026-08-04
tags: module-loader,RE-09,overlay
---

## Claim

The .rel relocation table names EVERY base-dependent word in all 30 CD.WAD modules, so overlays can be emitted base-relative and placed anywhere (option C)

## Evidence

Relocated all 30 modules offline at two bases (0x800C65EC and 0x80140000) with tools/extract_modules.py's own relocate(); diffed the two images word-by-word and subtracted the .rel site set. Base-dependent words NOT named by .rel = 0, across all 30 modules. NOTE the first version of this check was a LYING INSTRUMENT: it re-derived the site offsets as CUMULATIVE (cur += off) when relocate() treats them as ABSOLUTE (off = w & ~3), and reported 3037 false offenders while the per-module COUNTS matched exactly (1878/1878, 665/665, 487/487) — the count agreement is what exposed it.

## What would falsify it

a module whose relocated image differs at a word the .rel stream does not name, or a hi16 whose lo16 partners the emitter cannot pair
