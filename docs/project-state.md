# Project state — spider1

This is the authoritative capability inventory. It is independent of epic goals and ordered RE
work. Evidence details name only what has been observed; gaps remain explicit.

## Comparison baseline

The comparison baseline is the original USA PSX releases as run in a faithful emulator. The current
repository also has a retired intermediate baseline: a native-enhanced product whose remaining guest
code was emitted offline as C and compiled into per-title corpora. Evidence from that path remains
valid only for the exact behavior it observed; it is not evidence that the native/Lightrec product
exists or conforms.

## Current focus

S018 — the second movie field. The CD interrupt delivery gate is fixed and the guest advances past it;
the next boundary is the sector handoff at `0x8008DCC8(0x190)` and the sector still is not accepted
into the libstr ring.

**Re-measured 2026-09-29 against the tree that carries the CD-stream recovery (`4b96e51`) and the
cadence instrument (`a2a0175`), both legs taken on this machine and both captures opened and looked
at — the picture is still black, and the widening is still live:**

| leg | settings | last `[wide]` line | presented picture |
|---|---|---|---|
| 4:3 | `config/aspect_4x3.ini`, sink 960x720 | `native_width=512 render_width=512` | **0 / 691,200 non-black (0.00%)** |
| 16:9 | `config/aspect_16x9.ini`, sink 1284x720 | `native_width=320 render_width=428` | **0 / 924,480 non-black (0.00%)** |

Quoting the **LAST** `[wide]` line in each leg, per the trap that has bitten this workspace twice:
`picture_announce` prints on CHANGE, so the 16:9 leg carries `512 == 512` at its first line and
`320 -> 428` at its last. **The widening is 428 vs 320 and the picture is 0 of 924,480**, so issue
0025's refusal stands unchanged: a widened canvas with nothing in it is not a widened picture. The
captures are `scratch/screenshots/present_400.png` (overwritten by the 16:9 leg) and the run logs
are `scratch/picture_4x3.log` / `scratch/picture_16x9.log`.

## Capability inventory

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | Frozen launcher selects and authenticates one title and provisions its executable | partial | — | G001, G004 |
| S002 | Spider-Man reaches both intro movies and early `dem1` under its native owners | partial | S001 | G001 |
| S003 | Enter Electro executes through its measured crt0 and first game-owned-call boundary | partial | S001, S014 | G001 |
| S004 | Spider-Man platform services are owned through measured native seams | partial | S002 | G003 |
| S005 | Spider-Man render seam owns frame policy and a native frame envelope | partial | S002 | G002, G003 |
| S006 | Spider-Man scenes have complete native display-list producers | missing | S005, S007 | G002, G003 |
| S007 | Animated mesh input, pose identity, and pre-GTE composition contracts are available to a native producer | partial | S005 | G002, G003 |
| S008 | Spider-Man publishes a title-correct widescreen projection from active game state | partial | S002 | G002 |
| S009 | Spider-Man presents true per-object interpolated 60fps | missing | S006, S007 | G002 |
| S010 | The shipping Spider-Man path has no whole-frame compatibility fallback | verified | — | G002, G003 |
| S011 | Maintainer gates can verify hermetic contracts and bounded product behavior | partial | S001 | G004 |
| S012 | Input, memory card, and runtime module placement support Spider-Man gameplay | partial | S002, S004 | G003 |
| S013 | FMV and audio delivery run through host-owned services | partial | S004 | G003 |
| S014 | Same-engine lineage keeps title identity, facts, runtime images, and capability policy isolated | verified | — | G001 |
| S015 | First-party C++ passes Clang build, format, clang-tidy, and structure policy | verified | — | G004 |
| S016 | Enter Electro has title-derived native producers, widescreen projection, and temporal interpolation | missing | S003 | G002 |
| S017 | psxport executes remaining Spider guest code through a per-Core Lightrec runtime with no selectable interpreter mode | partial | S001 | G001, G003, G004 |
| S018 | Spider-Man reaches `dem1` dynamically and resumes the retail movie player through host-owned field boundaries | missing | S004, S017 | G001, G003 |
| S019 | Representative Spider-Man gameplay conforms on each released host through the native/Lightrec product | missing | S006, S008, S009, S013, S018 | G001, G002, G003, G004 |

### S001 — Authenticated default launcher

Demonstrated subset: the slim shell shim enters frozen `uv`; the Python launcher discovers the disc
boot serial, authenticates the extracted executable, and selects one title target.

Evidence from the retired pipeline: `run.sh`, `bootstrap.py`, `tools/run.py --selftest`, `tools/title_catalog.py`, and the
launcher-policy CTest cover the two known serials plus missing, unknown, ambiguous, and mismatched
inputs. `launcher_help` runs both `-h` and `--help` through the actual shell launcher and product
executable with all `PSXPORT_*` variables removed, proving usage exits 0 before dependency, disc,
asset or executable-identity discovery. The direct frozen `--prepare-only` route has built
Spider-Man from the player tree.

The current launcher provisions only the authenticated executable and builds the native/Lightrec
product. Gap: no authenticated title run has yet demonstrated that the launcher reaches Spider-Man's
first runtime discriminator with nonzero translated blocks and bounded fallback.

### S002 — Spider-Man boot and scene reach

Demonstrated subset from the retired generated-code product: a bounded real-disc run reached guest
main, both intro movies, the render seam and `dem1`. The current tree replaces the
non-returning guest main with a title-local finite prefix plus finite native owners for the
`0x8002C354` outer selector, `0x8002C174` primary loop, `0x800604CC` two-submit transition,
`0x800160EC` menu loop, and `0x8006F294` alternate loop. The authenticated jump-table map, static
ownership gate, transition test, and focused runtime test cover the route. Issue 0021's build-derived
STR derivative yields at the three authenticated field boundaries while retaining the emitted body;
title-local ownership also supplies asynchronous stream fields and the exact 300-field post-logo
pad/input wait. `scratch/logs/spider1-postlogo-owned-live.log` completes both movies, completes the
finite prefix, enters `dem1` at host frame 4941, reconciles 5,400/5,400 fences and exits 0 without a
guest VSync call.

Gap: the complete route has not run through Lightrec. The inspected `dem1` captures render real
characters but retain a sparse black background, and
the run did not yet reach `l1a1` or prove real selector transitions through every finite mode. Issue 0018 records an intermittent
STR VLC overrun before `dem1`; issue 0015 prevents the boot supervisor from cleanly terminating every
progressing capped run. This item therefore does not claim deterministic full boot or finished
gameplay. The exact-pinned `99a42aa3` meshprobe attempt in
`scratch/logs/gate-boot-20260826-235605.log` reproduced the issue 0018 allocator-fault signature at
frame 2 after one render-seam call. A preceding missing `TTSLOGO.STR` lookup is recorded as a separate
event; this run provides no evidence that it caused the allocator damage.

### S003 — Enter Electro measured boot boundary

Demonstrated subset: `SLUS_013.78` has an authenticated manifest, title-isolated executable facts,
direct `EnterElectroRuntime`, and 8/8 executable-derived crt0 facts. The retired bring-up path
refuses at gameMain
`0x80031F54` and declares widescreen-only rendering capabilities, so Spider-Man native/60fps controls
are not exposed.

Gap: EE-02, the first game-owned call, is unported. No Enter Electro gameplay, native producer,
temporal interpolation, runtime-module, or rendered-pixel claim exists.

### S004 — Native platform service ownership

Historical subset: `Spider1FrameDriver` owns display-field timing, VSync callback delivery,
per-field audio, per-frame pad service, and the single-fence invariant. `Spider1ModeDriver` owns the
retail field waits and submit ordering across primary, transition, menu, alternate, countdown, and
invalid-selector states. Repeated display fields pace the held image without rotating temporal
logic history; true no-submit early exits use an unpresented fence. Libetc VSync `0x80084BE0`
retains protected typed nonnegative field boundaries and cannot be replaced by a title handler;
its authenticated negative query reads the declared guest field counter. Native CD sector service, memory card
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

The legacy `GameConfig`/`GameHooks` static-product adapters are removed; measured facts now belong to
the title runtime or cohesive native owners. The finite movie/mode owners still need a cooperative
Lightrec continuation, and FMV/audio synchronization remains incomplete under S013.

The direct runtime now installs its measured SetGeomOffset, SetGeomScreen, CdRead, CdReadSync,
CD_cw, and VSync entries through the framework's existing `PlatformHlePlan`; the two pad receive
buffers use `GuestPadBufferLayout`. It also prepares the existing `Spider1FrameDriver` before authenticated
crt0, serves the pre-main ResetGraph field at its measured VSync return, and binds the GPU DMA
timeout arm, inner CdSync, public CdInit success contract, and VSyncCallback registration. A bounded real-disc Lightrec run
crossed those boundaries and next stopped at a separate stock libcd `VSync(-1)` from
0x8008D050. The `spider1_runtime_services` test exercises the production native CdInit and GPU DMA
timeout bindings with no `GameConfig`, plus projection setters, the protected VSync exit, and the
retail pad receive buffers. It also proves Setloc/Setmode guest work-area publication, GetTN result
survival through inner CdSync, and no work-area write without a declaration. The direct runtime
also binds the measured CD-ready callback slot and pumps the unchanged StGetNext body on a dry poll.
A synthetic guest callback calling VSync(-1) returns the declared libetc field count, preserves the
outer registers, and delivers no field; missing-counter and callback-exit controls refuse the
wrong paths. Full callback equivalence and crossing the retail command wait remain unverified. The earlier service-binding regression had
zero translated guest instructions; the new real-disc boot provides the translated-execution evidence stated in S017.
Finite movie/mode attachment and authenticated gameplay remain unverified.

### S005 — Render seam and frame envelope

Partial capability. Historical evidence proved a submit-frame boundary and executable-derived
DRAWENV/DISPENV calculations. The static dispatch seam was removed during break-first migration;
the preserved envelope and finite frame/mode owners are not attached to the current product.

Gap: first establish JIT gameplay conformance, then attach a new image-aware render boundary. Named
scenes still lack complete native geometry.

### S006 — Complete native display-list production

Missing capability: no Spider-Man named scene currently has a complete native display-list producer.
The common animated path, fixed-point projection/outcodes, face cull/clip/lighting/colour, and final
queue emission are not implemented together.

Atomic work: issue 0013 and RE-21. Native producers may consume only pre-GTE game state; guest GTE,
OT, packet, scratchpad, and rendered-VRAM output are excluded as producer inputs.

### S007 — Pre-GTE animated mesh contract

Demonstrated subset: `mesh_animated_vertex.*` decodes projection/reuse/retain flags and both near/far
fixed-point staging modes. `mesh_pose_contract.*` decodes the base transform, secondary rotation,
authored pose, and owner+pose temporal identity. Historical probe evidence records the exact pre-GTE
records and retail CR0..CR7 (C054, C055); the retired runtime probe source is not part of the product.

Gap: the first serialized product attempt ran on clean framework `99a42aa3`, but
`scratch/logs/gate-boot-20260826-235605.log` terminated at frame 2 with issue 0018's allocator-fault
signature before any face or pose boundary ran. Meshprobe armed and self-tested, but emitted zero
`faceCall`, `POSE_CORPUS`, or `PROGRESS` rows, so it supplied no live corpus evidence. A successful
serialized run must still establish valid pose rows, real temporal changes, mesh bindings, owner
mismatch counts, and repeat-input oracle comparisons before a PC matrix composer or temporal store
can be trusted. The progress record also names calls excluded after the bounded 64-pose roster fills.
No interpolation or draw is enabled by this item.

### S008 — Spider-Man widescreen projection

Historical evidence showed the title's world-render/projection boundary and exposed issue 0022's
cumulative descriptor bug (`512 -> 684 -> 912 -> 1024`); scoping the projected tuple around the
retail guest call removed it.
Final real-disc evidence `scratch/logs/spider1-wide-scoped-final.log` records exactly one stable
`512x240 -> 684x240` / lens `2365 -> 3159` mapping across repeated `dem1` renders, reconciles
5,150/5,150 fences, and its inspected capture contains live demo character/text output.
The retained `Spider1Widescreen` owner now contains only the pure aspect calculation. Its former
static guest-call attachment was removed, so the current product exposes no widescreen override.

Gap: `l1a1` and a paired standard-aspect leg have not been captured on the new finite route, so the
live proof establishes stable expansion/reach but not a complete scene-by-scene A/B. Compare those
legs for genuinely expanded
world content with no stretch, missing edge geometry, or HUD displacement. Enter Electro remains
separate under S016.

**2026-09-27: the picture pair exists as a REFUSAL, which is the correct answer.** Both legs were
shot on the current tree, each in its own process because `PSXPORT_PRESENT_SINK` is process-wide, at
4:3 `960x720` and 16:9 `1284x720`, and `psxport/tools/port/widescreen_pair.py` returned:

```
  predicted offset for a pure widening : +162
  left  margin 162px wide  :   0.0% non-black, 1 colours, 161/161 repeated columns   <- NOT SCENE
  right margin 162px wide  :   0.0% non-black, 1 colours, 161/161 repeated columns   <- NOT SCENE
REFUSED: ... NOTHING WAS COMPARED at the joins.
```

Both legs measured `non-black 0/691200 (0.00%)` and `0/924480 (0.00%)`, so the margins are empty
canvas, not scene content, and the tool is right to refuse. **The margin measurement therefore says
nothing yet about the 39 cull sites or the OFX widening** — it has still never seen a game scene. The
widening itself is live and correct where it can be observed: the 16:9 leg published the guest draw
clip `512x240 -> 684x684` and reached `native picture: aspect=1 native_width=320 render_width=428`,
but no frame was ever displayed. Re-shooting until the tool passes is the wrong move; the answer only
becomes a measurement once the product reaches `dem1`.

### S009 — True interpolated 60fps

**This item's SCOPE IS UNDECIDED, and the reason is now measured rather than assumed — and the
measurement is DONE.** `docs/issues/0031` took the live two-number run `docs/issues/0030` named,
with an instrument registered in the gate (`tools/probe_spider1_cadence.py`, `--selftest` 22/22,
CTest `spider1_cadence_measure_{selftest,census}`).

What it measured, from one paced run: **59.71 presents/second and exactly 1.0000 display fields
per presented frame** (133/133, and 19/19 in a second run), `interp=0` throughout. The counter's
address is recovered from the crt0's own bytes — the PS-X EXE header's `gp` is 0 for this image, so
`[gp+0x0C74] = 0x800B5468` comes from `lui $gp,0x800B ; addiu $gp,$gp,0x47F4` at 0x80087418/1C —
and it has exactly one `+1` writer at `0x8005E53C` over 186,880 text words.

**And that number is THIS PORT'S cadence, not retail's, and the reason is measured.** `0x8005E748`
(the guest's field wait) and `0x8002C354` (the outer selector) are native overrides in this port,
and `0x8002C174` — the guest's own frame loop, whose second wait site is the `DrawSync(1)`
back-edge loop that made the per-frame field count a non-constant — is referenced nowhere in the
title tree and never runs. The host supplies `quota=1` to the framework's frame pacer, 57 of 57
consecutive paced lines from the product's own `pacer` channel. So the guest's cadence is not
observable on a build where the guest's cadence has been replaced.

**Consequence, stated so nobody re-derives it: this item may not be closed as out-of-scope and may
not be started as in-scope.** The rate is not "probably 30 fps because every other measured title
here is"; it is unmeasured, for a named reason. `docs/issues/0031` §7 names the two changes that
would make it answerable — `0x8002C174` executing as translated guest code, and `0x8005E748` served
as a poll on the guest's counter rather than a host field injection — and both are blocked on the
black-picture frontier, not on instruments.

Two measurement traps this item paid for, both recorded because both produced a confident wrong
number: a store sweep that omitted `sw` (0x2B) reported **zero writers** on a counter with one, and
the first run of the instrument was **unpaced at 1,204 presents/second** because
`agent_environment` sets `PSXPORT_NOPACE=1` for every agent run — at which point dividing by the
59.94 Hz field rate printed "58.68 game frames/second", within 2% of the right answer, for a program
no player runs. The verdict now refuses an unpaced run by name, and the selftest pins the refusal on
that measured figure.

Missing capability, unchanged: Spider-Man has no complete native producer whose previous/current
authored state can be sampled for extra presentation frames. Its runtime therefore exposes neither
native rendering nor temporal interpolation; guest-frame output is mechanically
non-interpolated.

Required owner: target `game/render/mesh_pose_history.*` plus native producer integration after S006
and live validation of S007 — and, before either, the cadence change named in `docs/issues/0031` §7.

### S010 — No whole-frame compatibility fallback

Observable condition: the former whole-guest-frame compatibility owner, its selector, and its tests
are absent. Gameplay enters the runtime JIT product path; unported native presentation remains an
explicit missing capability rather than switching to a second whole-frame product.

Evidence: `tools/source_policy.py` rejects the retired path, identifiers, and generated source roots.

### S011 — Verification coverage

Demonstrated subset: `tools/verify.py` delegates one exact-pinned configure, both-title product build,
all 19 CTests, and the self-tested repository/CMake/linked-product execution boundary inspection
to PSXPort's shared consumer verifier. The complete gate passed locally on 2026-09-08 with
Clang/Ninja against PSXPort `a5a79652` and Lightrec `b1457137`; its unchanged build performed zero
compilations. The asset-free Linux x86-64 workflow invokes the same entry point. Its earlier
product-composition result passed on main commit
`265ff0862820052fe6d84daf45e5b2e257701f02` in
[run 33960072758](https://github.com/SomeoneIsWorking/spider1/actions/runs/33960072758).
This is composition evidence only; the retired static-product gate is absent.

Hosted [run 34222419138](https://github.com/SomeoneIsWorking/spider1/actions/runs/34222419138)
failed during compilation: the workflow omitted `CC`/`CXX`, so its fresh CMake tree selected GNU
15.2, which rejected the REPL's implicit array-to-`std::span` conversion through a function pointer.
The Linux job now selects Clang explicitly; PSXPort `9e104d9f` constructed that span explicitly and
retained GCC compatibility. Hosted
[run 34688672212](https://github.com/SomeoneIsWorking/spider1/actions/runs/34688672212)
then failed only the pin test: the workflow checked out hardcoded `9e104d9f` while the title
recorded `0b432b46`. CI now clones through `tools/psxport_sync.py --clone` after locked Python
setup, preserving its explicit submodule restore. The `ci_workflow` negative test rejects the old
hardcoded checkout. The 2026-09-12 local canonical verifier configured both products with Clang 22
against recorded PSXPort `86eea8cd`, passed all 19 CTests and the linked-product execution-boundary
selftest/check. A hosted run of this correction remains pending.

Gap: issue 0015 leaves the progressing live boot supervisor unable to terminate/reap every capped
run. Issue 0009 records that fixed present indices are not content-stable, so visual comparisons need
content identity rather than an index alone.

### S012 — Gameplay support services

Historical conditions: forced pad input changes the menu, a 128 KiB memory-card image is created and
the card check completes, and concurrently live CD.WAD modules occupy distinct guest allocations
without a guest-execution miss.

Historical evidence: resolved issue 0001, the measured multi-image residency claim C013, and the
input/memory-card behavior recorded in the durable issue/claim ledgers concern the retired product.

Current direct-runtime subset: `Spider1Runtime` supplies the RE-05 receive-buffer layout to the
shipping `Pad::serviceFrame` owner. The `spider1_runtime_services` CTest writes a held input into the
retail slot-0 packet, reports slot 1 disconnected, and preserves the game's separate mirror.
Gap: the native frame owner is not attached, and input, cards, and module placement have not been
qualified through interactive Lightrec gameplay.

### S013 — FMV and audio

Demonstrated subset: both intro logo movies deliver sectors, decode visible frames, and complete
under the native field owner; the host advances the SPU mixer and XA samples are produced in
headless capture. The 5,400-field real-disc run continues through the post-logo wait into `dem1`.

Gap: no durable A/V synchronization measurement exists, audio has not been user-confirmed, and issue
0018 keeps the STR decode path nondeterministic.

### S014 — Two-title isolation

Observable conditions: title selection is serial-keyed; the address-free lineage base owns no guest
address; Enter
Electro refuses unknown behavior rather than borrowing Spider-Man values.

Evidence: C050, C051, C052 and focused `spider_runtime` / `enter_electro_runtime` tests. The old
targets also kept their emitted namespaces isolated, establishing why runtime image identity must be
title-specific. Both title
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

### S017 — Runtime Lightrec execution

Implemented subset: the product enters authenticated crt0 through psxport's per-`Core`
`dispatchGuest` boundary. Spider pins PSXPort `86eea8cdf52c0274e280fa59b924ba70cd57b782`, which
requires maintained Lightrec runtime ABI `b1457137c31cedff5f440d59da29401d021ba2da`; the exact pair
links into both title products. Image-aware native
dispatch, scoped `callOriginal`, invalidation, typed exits, and translation/fallback counters are
present, and the old generator/dispatcher/selector/product are absent. The shared Linux x86-64
synthetic contract proves nonzero translated blocks, native/original dispatch, and self-modifying-code
retranslation without fallback. It separately admits one classified difficult block under the
configured limit and refuses a zero-limit block before any interpreter instruction executes.

Authenticated Spider-Man boot crossed pre-main ResetGraph field delivery, the GPU DMA timeout arm,
inner CdSync, and public CdInit through translated code with zero interpreter fallback. The
executor's bounded-turn continuation resumes the retail allocator's finite heap fill without a
guest-code derivative. One earlier boundary was stock libcd `VSync(-1)` at return `0x8008D050`;
the native CD_cw binding has synthetic coverage but no reached retail command. Subsequent runs
resumed the first movie field, then found a dry StGetNext poll and a nested libcd `VSync(-1)` query
inside its ready callback. The pre-query retail run executed 352,889 translated blocks and
1,798,345 instructions with zero interpreter fallback and stopped at that nested query. The
combined query path now crosses it in authenticated retail execution, but no later movie field
has been reached.

Gap: authenticated Spider-Man has not yet proved original-call title dispatch or address-reusing
runtime-module invalidation. Authenticated Spider gameplay has not
exercised the enforced per-execution limit or established an aggregate fallback-share release
threshold. Multi-`Core` and host backends outside Linux x86-64 also remain absent; no interpreter
gameplay selector may be added to cover those gaps.

### S018 — `dem1` dynamic discriminator

Missing capability: the authenticated Spider-Man image has not yet reached `dem1` through Lightrec.

Implemented prerequisite: the build-derived movie-fiber source is gone and `Spider1MovieExecution`
expresses unchanged-retail `FUN_8002AA0C` execution through the scoped-original runtime boundary.
The direct-boot frame driver now services the three authenticated movie VSync(0) return PCs without
unwinding the guest movie body. A synthetic shipping Lightrec call at each PC preserves CPU return
state, delivers a registered field callback and exactly one presentation fence, and resumes the
following guest instruction once; an unrelated return is refused without state advancement.
An authenticated retail Lightrec run resumed the first STR field at `0x8002AC8C` and presented
one fence, then stalled without another field. A bounded debugger probe captured the dry
`StGetNext` (`0x80086B10`) poll with an empty 48-slot ring and zero sectors delivered. The direct
runtime now attaches the guest-ready stream pump and typed dry-poll field. A subsequent retail run
attempted one ready callback, then exposed a nested `VSync(-1)` counter query from stock CdReady:
live RA `0x8008CC00`, `a0=-1`, and one INT1-ready callback attempted. The title declares the
authenticated libetc counter, and the shared query contract passes a synthetic nested guest-callback
test without advancing a field. A combined retail run crossed the query and kept presenting for
90 seconds, but no second movie field appeared. A later snapshot found 1,026 INT1-ready callback
attempts with ring write, frame-start, and consumer indices all zero. A bounded GDB control captured
8/8 returned callbacks, 0/8 ring publications, producer reason 3, and an unread correctly headed
CDC FIFO: the producer exits after `CdReady(1)` returns a result byte with bit `0x04` set, before
DMA or STR-header validation.

**2026-09-27 re-measurement overturned two of those conclusions and replaced them.** The current tree
against psxport `e0485d33` reaches retail STR field 1 at `0x8002AC8C` and presents it every run, so
the **boot VSync abort is no longer the wall** — that boundary is crossed. And the CdReady
result-bit guard **does not fire**: the title's new `cdready` diagnostic reports producer reason
`0x800B1000 = 0` and a CLEAR `0x04` on the staged response on every one of 200,000 polls in a
183-second run. The guard branch at `0x800850B0` is real and the byte it reads is the guest's own
libcd response-queue discipline, staged by the guest ISR `FUN_8008C3E0` and never by the host, so it
is not where a host could have gone wrong.

What the same measurement did find is the CD **interrupt delivery gate**, and it is arithmetic
rather than a guess. `scratch/dem1/probe_gate.log`:

```
[irq] registered interrupt element 0x800C1528 prio=2 (chain now 1)
[irq] CD raised IRQ2 -> I_STAT=0x005 (mask=0x009, masked off by the guest)
[cdready] ... cdcIrqType=1 dataRead=0 dataAvail=2340 ... iStat=0x004 iMask=0x009 IRQ2 LATCHED
           chain=1 handler=0x80087660 pendingWork=0
```

`Hle::irqPoll` delivers `i_stat & i_mask`, and `0x004 & 0x009 == 0`, so the gate is cleared and the
guest's own registered CD element never runs while the controller holds 2340 unread sector bytes.

**Fixed this session, cause and not symptom:** `Spider1FrameDriver::initializeCd` replaces the retail
`CdInit` body `0x8008A16C`, whose observable effects include reaching the B-vector interrupt-enable
thunk `0x8008B86C` with `a0 = 2` (`0x8008A17C` -> `0x8008A1FC` -> `0x8008D4E4` -> `0x8008D54C`).
That is what arms IRQ2. The replacement installed the four callback slots faithfully and dropped the
arm, so replacing the leaf silently lost one of its observable effects. `armCdInterrupt` restores it
through the device register `0x1F801074`, OR-ing bit 2 into the mask the guest already holds rather
than writing a literal. Measured effect: `iMask 0x009 -> 0x00D`, `pendingWork` goes `0 -> 1`, IRQ2 is
delivered (`i_stat` returns to `0x000`), and the guest advances out of the `StGetNext` spin into a
further authenticated CD wait at `0x8008DCC8(0x190)`, with per-run poll count rising from 2 to 17+.

Gap: still no second movie field and no `dem1`. The sector is still not accepted (`dataRead=0`,
`dataAvail=2340`, staged response `0x00`, ring indices all zero) and the next boundary is that
further CD wait. Reaching `dem1` requires completing both intro movies, including the authenticated
`0x8002AC8C`, `0x8002AE1C`, and `0x8002AFEC` field exits. This issue cannot authorize deleting the
old pipeline until S019's representative-gameplay, invalidation, original-call, independent-oracle,
host-performance, and no-interpreter gates pass.

### S019 — Representative gameplay conformance

Missing capability: a bounded representative interactive Spider-Man scenario must reach at least the
current verified frontier with native and scoped-original dispatch, executable-module invalidation,
independent-oracle state checks, and the declared correctness/frame-time budget on each released host.
The obsolete generator, corpus, seeds, dispatcher/tests, and build/provisioning route have already
been removed break-first; none may return as a fallback.

## Hosted platform coverage

| Platform boundary | CI state | Exact gap |
|---|---|---|
| Linux x86-64 | partial | The asset-free product composition gate passes locally and in the hosted run recorded in S011. Authenticated gameplay and release performance are unverified. |
| Windows x86-64 | missing | No qualified PSXPort/Lightrec product backend or Spider build contract exists. |
| macOS x86-64 | missing | No qualified PSXPort/Lightrec product backend or Spider build contract exists. |
| macOS arm64 | missing | The AArch64 emitter, executable-memory/ICache boundary, packaging, and gameplay qualification are absent. |
| Android arm64-v8a | missing | The AArch64 backend, shared Android packaging/runtime integration, touch layer, and device performance qualification are absent. |

The hosted workflow is configured for only the real Linux x86-64 product boundary. A policy-only
job is not counted as Windows, macOS, or Android support.
