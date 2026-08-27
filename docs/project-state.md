# Project state — spider1

This is the authoritative capability inventory. It is independent of epic goals and ordered RE
work. Evidence details name only what has been observed; gaps remain explicit.

## Current focus

S002 — Spider-Man native frame-loop ownership and finite boot.

## Capability inventory

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Frozen launcher selects and authenticates one title, provisions it, and builds its product target | verified | — | G001, G004 |
| S002 | Spider-Man boots through guest main and reaches game scenes | partial | S001 | G001 |
| S003 | Enter Electro executes through its measured crt0 and first game-owned-call boundary | partial | S001, S014 | G001 |
| S004 | Spider-Man platform services are owned through measured native seams | partial | S002 | G003 |
| S005 | Spider-Man render seam owns frame policy and a native frame envelope | partial | S002 | G002, G003 |
| S006 | Spider-Man scenes have complete native display-list producers | missing | S005, S007 | G002, G003 |
| S007 | Animated mesh input, pose identity, and pre-GTE composition contracts are available to a native producer | partial | S005 | G002, G003 |
| S008 | Spider-Man publishes a title-correct widescreen projection from active game state | partial | S002 | G002 |
| S009 | Spider-Man presents true per-object interpolated 60fps | missing | S006, S007 | G002 |
| S010 | Unported Spider-Man scenes remain runnable through explicit non-interpolated guest-frame debt | verified | S005 | G002, G003 |
| S011 | Maintainer gates can verify hermetic contracts and bounded product behavior | partial | S001 | G004 |
| S012 | Input, memory card, and runtime module placement support Spider-Man gameplay | verified | S002, S004 | G003 |
| S013 | FMV and audio delivery run through host-owned services | partial | S004 | G003 |
| S014 | Same-engine lineage keeps title identity, facts, generated code, and capability policy isolated | verified | — | G001 |
| S015 | First-party C++ passes Clang build, format, clang-tidy, and structure policy | verified | — | G004 |
| S016 | Enter Electro has title-derived native producers, widescreen projection, and temporal interpolation | missing | S003 | G002 |

### S001 — Authenticated default launcher

Observable conditions: the slim shell shim enters frozen `uv`; the Python launcher discovers the
disc boot serial, authenticates the extracted executable, selects one title target, and can stop
after provisioning/build without launching.

Evidence: `run.sh`, `bootstrap.py`, `tools/run.py --selftest`, `tools/title_catalog.py`, and the
launcher-policy CTest cover the two known serials plus missing, unknown, ambiguous, and mismatched
inputs. `launcher_help` runs both `-h` and `--help` through the actual shell launcher and product
executable with all `PSXPORT_*` variables removed, proving usage exits 0 before dependency, disc,
asset or executable-identity discovery. The direct frozen `--prepare-only` route has built
Spider-Man from the player tree.

### S002 — Spider-Man boot and scene reach

Demonstrated subset: the bounded real-disc product reaches guest main, both intro movies, the render
seam and `dem1` with the authenticated Spider-Man substrate. The current tree replaces the
non-returning guest main with a title-local finite prefix plus finite native owners for the
`0x8002C354` outer selector, `0x8002C174` primary loop, `0x800604CC` two-submit transition,
`0x800160EC` menu loop, and `0x8006F294` alternate loop. The authenticated jump-table map, static
ownership gate, transition test, and focused runtime test cover the route. Issue 0021's build-derived
STR body yields at the three authenticated field boundaries while retaining the generated super;
title-local ownership also supplies asynchronous stream fields and the exact 300-field post-logo
pad/input wait. `scratch/logs/spider1-postlogo-owned-live.log` completes both movies, completes the
finite prefix, enters `dem1` at host frame 4941, reconciles 5,400/5,400 fences and exits 0 without a
guest VSync call.

Gap: the inspected `dem1` captures render real characters but retain a sparse black background, and
the run did not yet reach `l1a1` or prove real selector transitions through every finite mode. Issue 0018 records an intermittent
STR VLC overrun before `dem1`; issue 0015 prevents the boot supervisor from cleanly terminating every
progressing capped run. This item therefore does not claim deterministic full boot or finished
gameplay. The exact-pinned `99a42aa3` meshprobe attempt in
`scratch/logs/gate-boot-20260826-235605.log` reproduced the issue 0018 allocator-fault signature at
frame 2 after one render-seam call. A preceding missing `TTSLOGO.STR` lookup is recorded as a separate
event; this run provides no evidence that it caused the allocator damage.

### S003 — Enter Electro measured boot boundary

Demonstrated subset: `SLUS_013.78` has an authenticated manifest, separate generated namespace,
direct `EnterElectroRuntime`, and 8/8 executable-derived crt0 facts. It refuses at gameMain
`0x80031F54` and declares widescreen-only rendering capabilities, so Spider-Man native/60fps controls
are not exposed.

Gap: EE-02, the first game-owned call, is unported. No Enter Electro gameplay, native producer,
temporal interpolation, runtime-module, or rendered-pixel claim exists.

### S004 — Native platform service ownership

Demonstrated subset: `Spider1FrameDriver` owns display-field timing, VSync callback delivery,
per-field audio, per-frame pad service, and the single-fence invariant. `Spider1ModeDriver` owns the
retail field waits and submit ordering across primary, transition, menu, alternate, countdown, and
invalid-selector states. Repeated display fields pace the held image without rotating temporal
logic history; true no-submit early exits use an unpresented fence. Libetc VSync `0x80084BE0` is an
all-mode abort and cannot be replaced by a title handler. Native CD sector service, memory card
handling, base-relative runtime module routing, and the measured guest program image remain on
cohesive game/framework seams.

Earlier live evidence: `scratch/logs/gate-boot-20260827-022304.log` aborted at the stock CdInit controller
timeout's `VSync(-1)`. The public CdInit boundary now installs the authenticated callback table
synchronously through the host CD owner. `scratch/logs/gate-boot-20260827-022834.log` advances past
that boundary and aborts at the next residual call, the STR player's initial `VSync(0)`.
`scratch/logs/finite-str-wide-20260827.log` crossed that call but its seven inspected captures were
black and it never exited boot. That falsified the first fiber scheduling order. The corrected
real-disc run `scratch/logs/spider1-postlogo-owned-live.log` visibly renders and completes both logo
movies, completes the exact post-logo wait, reaches `dem1`, reconciles every capped frame and exits
0. The VSync trap remains installed, and the product made no guest VSync call.

Gap: Legacy `GameConfig`/`GameHooks`
compatibility views remain for measured Spider-Man facts, and FMV/audio synchronization remains
incomplete under S013.

### S005 — Render seam and frame envelope

Demonstrated subset: the override of retail submitFrame `FUN_80061308` is reached and owns the frame
policy boundary; the native envelope reproduces executable-derived DRAWENV/DISPENV words; the
title-local frame and mode drivers map every finite retail submit completion to one framework queue
fence, including the transition/menu/alternate paths.

Gap: the envelope's only fully owned scene is a black boot-init frame, so its producer A/B cannot
falsify pixel output (issue 0013). Named scenes still lack complete native geometry and use S010.

### S006 — Complete native display-list production

Missing capability: no Spider-Man named scene currently has a complete native display-list producer.
The common animated path, fixed-point projection/outcodes, face cull/clip/lighting/colour, and final
queue emission are not implemented together.

Atomic work: issue 0013 and RE-21. Native producers may consume only pre-GTE game state; guest GTE,
OT, packet, scratchpad, and rendered-VRAM output are excluded as producer inputs.

### S007 — Pre-GTE animated mesh contract

Demonstrated subset: `mesh_animated_vertex.*` decodes projection/reuse/retain flags and both near/far
fixed-point staging modes. `mesh_pose_contract.*` decodes the base transform, secondary rotation,
authored pose, and owner+pose temporal identity. Hermetic opposite-answer tests exercise the shipping
decoders. Observe-only `mesh_probe.cpp` captures the exact pre-GTE records and retail CR0..CR7 only as
a post-super-call oracle (C054, C055).

Gap: the first serialized product attempt ran on clean framework `99a42aa3`, but
`scratch/logs/gate-boot-20260826-235605.log` terminated at frame 2 with issue 0018's allocator-fault
signature before any face or pose boundary ran. Meshprobe armed and self-tested, but emitted zero
`faceCall`, `POSE_CORPUS`, or `PROGRESS` rows, so it supplied no live corpus evidence. A successful
serialized run must still establish valid pose rows, real temporal changes, mesh bindings, owner
mismatch counts, and repeat-input oracle comparisons before a PC matrix composer or temporal store
can be trusted. The progress record also names calls excluded after the bounded 64-pose roster fills.
No interpolation or draw is enabled by this item.

### S008 — Spider-Man widescreen projection

Demonstrated subset: `Spider1Widescreen` overrides the title's sole world-render/projection boundary
`FUN_80075D0C`, reads its active viewport descriptor, publishes that native geometry through the
framework's guest-projection latch, and widens the descriptor before the unchanged retail renderer
computes frustum planes, GTE H/offset, culling and object output. Width and lens divisor scale by the
same ratio, preserving focal length while expanding horizontal view; disabling widescreen restores
the observed native descriptor rather than scaling an already-widened value. The production helper
test covers the retail 512 -> 684 16:9 result, byte-identical native output, and both bound
orientations. The first live mapping instrument exposed issue 0022's cumulative descriptor bug
(`512 -> 684 -> 912 -> 1024`); scoping the projected tuple around the retail super-call removed it.
Final real-disc evidence `scratch/logs/spider1-wide-scoped-final.log` records exactly one stable
`512x240 -> 684x240` / lens `2365 -> 3159` mapping across repeated `dem1` renders, reconciles
5,150/5,150 fences, and its inspected capture contains live demo character/text output.
Spider-Man's runtime exposes this policy without enabling its missing native renderer or
interpolation controls.

Gap: `l1a1` and a paired standard-aspect leg have not been captured on the new finite route, so the
live proof establishes stable expansion/reach but not a complete scene-by-scene A/B. Compare those
legs for genuinely expanded
world content with no stretch, missing edge geometry, or HUD displacement. Enter Electro remains
separate under S016.

### S009 — True interpolated 60fps

Missing capability: Spider-Man has no complete native producer whose previous/current authored
state can be sampled for extra presentation frames. Its runtime therefore exposes neither native
rendering nor temporal interpolation; guest-frame output is mechanically non-interpolated.

Required owner: target `game/render/mesh_pose_history.*` plus native producer integration after S006
and live validation of S007.

### S010 — Explicit guest-frame debt

Observable conditions: an unported named scene can run the actual retail submit body for the whole
frame, with native envelope/geometry overlap and interpolation refused.

Evidence: `guest_frame_fallback.*`, its CTest, C044, and HACK-03 record the mutually exclusive policy
and denominators. This capability is compatibility debt, not native-producer progress.

### S011 — Verification coverage

Demonstrated subset: hermetic CTests cover production contracts, title runtimes, executable identity,
launcher policy, C++ policy, and render policy; `tools/gate.py check-log` recognizes known-good and
known-broken captures and prints denominators.

Gap: issue 0015 leaves the progressing live boot supervisor unable to terminate/reap every capped
run. Issue 0009 records that fixed present indices are not content-stable, so visual comparisons need
content identity rather than an index alone.

### S012 — Gameplay support services

Observable conditions: forced pad input changes the menu, a 128 KiB memory-card image is created and
the card check completes, and concurrently live CD.WAD modules occupy distinct guest allocations
without a recomp miss.

Evidence: resolved issue 0001, the measured module relocation claim C013, and the input/memory-card
behavior recorded in the durable issue/claim ledgers.

### S013 — FMV and audio

Demonstrated subset: both intro logo movies deliver sectors, decode visible frames, and complete
under the native field owner; the host advances the SPU mixer and XA samples are produced in
headless capture. The 5,400-field real-disc run continues through the post-logo wait into `dem1`.

Gap: no durable A/V synchronization measurement exists, audio has not been user-confirmed, and issue
0018 keeps the STR decode path nondeterministic.

### S014 — Two-title isolation

Observable conditions: title selection is serial-keyed; each title target links one generated
namespace and one derived runtime; the address-free lineage base owns no guest address; Enter
Electro refuses unknown behavior rather than borrowing Spider-Man values.

Evidence: C050, C051, C052 and focused `spider_runtime` / `enter_electro_runtime` tests. Both title
runtimes currently expose only their implemented GTE/widescreen presentation; their independent
tests prevent either title from inheriting unimplemented native/temporal capability by lineage.

### S015 — C++ policy

Observable conditions: touched first-party C/C++ compiles with Clang, matches `.clang-format`, passes
clang-tidy against real compile commands, and respects the 1,200-line structure ceiling.

Evidence: the `cpp_policy` CTest invokes the shared non-mutating checker; focused and full CTest runs
record the checked file and translation-unit denominators.

### S016 — Enter Electro native enhanced presentation

Missing capability: Enter Electro has no title-derived render seam, native producer, widescreen
projection publication, pose history, or temporal interpolation product. Its current
`widescreenOnly()` capability declaration intentionally removes unimplemented native-renderer and
60fps controls from player surfaces.

Required capability: after EE-02 and the title's render ownership boundary are RE-derived, implement
Enter Electro's own native+wide+temporal path without copying Spider-Man addresses or claiming the
current capability refusal is permanent.
