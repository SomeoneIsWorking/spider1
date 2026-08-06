---
id: I016
kind: instrument
status: DISTRUSTED
created: 2026-08-06
distrusted_on: 2026-08-06
---

## Instrument

tools/present_geometry.py stretch factor / absolute aspect [POINTER ENTRY — canonical record is docs/info/instruments.md INST-25]

## Validated by

NOT validated for the magnitude — registered DISTRUSTED-FOR-THE-NUMBER 2026-08-06. Canonical record and the four-direction validation of what it IS good for: docs/info/instruments.md INST-25. Pointer entry only, so info.py brief/list/check can see the distrust.

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-06

See docs/info/instruments.md INST-25 for the full record. In short: it measures the NON-BLACK CONTENT BBOX, not the display rect, so black content rows shrink the band and inflate the aspect. MEASURED: it reports spyro's present stretch as 1.714x where the real stretch is 1.600x (512:240 presented where 4:3 belongs — exact arithmetic). Its own <5%-of-sink CAUTION does not fire anywhere near a 7% error, so it printed a confident unqualified number. STILL TRUSTED for the VERDICT (STRETCHED vs fills-the-sink) and for a before/after transition on the same content, where the bbox error cancels. DISTRUSTED for the stretch factor and the absolute aspect as quotable numbers. To trust the magnitude: measure the display rect (the viewport the present stage set) instead of inferring it from content.

> Every result this instrument produced is suspect until it is re-validated.

## THE DISTRUST APPLIES TO **THIS REPO'S COPY** — a repaired copy already exists (same day)

`present_geometry.py` is DUPLICATED across the workspace and the copies have DIVERGED.
`spyro/tools/present_geometry.py` (= `Tomba2Engine/tools/present_geometry.py`) is a REPAIRED version,
registered as spyro **I042**: `--selftest` 16/16, mutation-tested, and it REFUSES with rc=3
(AMBIGUOUS) on exactly the frame this copy answered `STRETCHED 1.714x` for. Given
`--active 512x224 --display 512x240` it resolves that frame to `1.600x` (rc=1) and the fixed present
to `OK` (rc=0) — validated in both directions.

The 7% error is EXACT and that names the cause: `1.714 / 1.600 == 240 / 224`, and spyro's guest draws
224 of its 240 display lines. The band-only measurement charges GUEST-DRAWN black to the letterbox.

REMEDY: `cp spyro/tools/present_geometry.py spider1/tools/`. NOT done here — this step was docs-only.
Until it is, this repo's copy stays distrusted for the magnitude. Before quoting a number from ANY
copy, run `md5sum */tools/present_geometry.py` from `~/repo/psx`; measured 2026-08-06, spyro's copy
and this one have different md5s. Full record: docs/info/instruments.md INST-25.
