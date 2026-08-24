# Spider-Man (`SLUS_008.75`)

Current implemented target (USA). `SLUS_008.75`, the disc boot executable/serial, is its canonical
title key. `Spider1Runtime` is the title-derived owner of its compatibility services, overrides, and
optional temporal presentation. The shared `SpiderRuntime` base owns no guest address. Its measured
749,568-byte executable SHA-256 is
`d2270e35581ba083d9441166e9a45ead4f869ab07e890f9a512ad7ee4cc0b15b`; both provisioning and native
boot authenticate that identity before using the cache.

The legacy `GameConfig` compatibility facts and Spider-Man 1 render/core modules still live under
repository-level `game/`; this is bounded migration debt, not a claim that they are shared with
Enter Electro. The `enter_electro_port` target does not link them. Generated Spider-Man 1 code keeps
the historical `generated/` namespace while Enter Electro uses `generated/spiderman2/`. The derived
runtime also projects this title's measured `preserveVramBackdrop=1` policy through
`guestVramIsPicture()` because Spider-Man 1 still runs guest drawing and upload-only screens are
picture content; that answer is not a lineage default.
