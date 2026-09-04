# Spider-Man 2: Enter Electro (`SLUS_013.78`)

`SLUS_013.78`, the USA disc boot executable/serial, is this title's canonical key. The launcher and
provisioner read `SYSTEM.CNF` from the selected disc on every run and refuse the Spider-Man 1 disc.
`EnterElectroRuntime` derives directly from the address-free `SpiderRuntime` base; it does not bind
Spider-Man 1's legacy `GameConfig` or `GameHooks`.

The manifest also owns the measured 786,432-byte executable identity and SHA-256
`dbe6c3f32337fe0fa7085519c728a75abf5d007b45ea0ba58178bcf84b72908a`. Provisioning checks fresh
media, and the native target repeats the serial/size/PS-X EXE/SHA-256 check before constructing
`Game`; matching the filename alone is never sufficient.

Measured boundary: the executable's crt0 group (8/8 fields) and the retired resident-only bring-up
path reached an explicit refusal at the first game-owned call, `gameMain 0x80031F54` (`EE-02`). No
gameplay, Lightrec execution, rendering, widescreen, native producer, or runtime-module support is
claimed. Enter Electro follows only after Spider-Man passes its representative-gameplay migration
gate.
The required current-frame `guestVramIsPicture()` query likewise refuses with the Enter Electro
frontier: execution cannot reach presentation before EE-02, and no Spider-Man 1 render-ownership
policy is substituted for the unmeasured title.
