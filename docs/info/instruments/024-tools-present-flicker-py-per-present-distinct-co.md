---
id: I024
kind: instrument
status: trusted
created: 2026-08-06
---

## Instrument

tools/present_flicker.py — per-present distinct-colour count, consecutive per-pixel diff, and FRAME IDENTITY over a capture the run's own log defines

## Validated by

--selftest runs it against BOTH classes and is wired to print both: a 1-px alternating sequence returns oscillating=1 diffs=[1,1,1]; a 1-px monotonic change returns oscillating=0 diffs=[0,1,0]. It REFUSES rather than returning empty when the manifest has no [present_shot] lines (exit 1, 'NOTHING was analysed — this is NOT a clean result'), and it hard-errors if a logged file is missing, is a different size, or disagrees with the log's own non-black count. It never lists a directory: the result set is the run's [present_shot] manifest, because a glob over the shared scratch/screenshots/ accumulator is what produced the refuted widescreen explanation of this game's flicker. DISTRUST ONE FIELD: its OSCILLATING-PIXELS count is near-useless on real content and is labelled so in the output — it reported 48.1% on a 120-present gameplay capture whose frame identities were 37 strictly-new pictures with 2 revisits and zero alternation, because these frames hold ~1500 distinct colours over 691200 px and coincidental colour revisits dominate. Read FRAME IDENTITY, which compares whole pictures and has no such failure mode.

## Known failure modes

(none recorded yet)
