# PLAN — Spider-Man 2: Enter Electro joins this repo (multi-title split)

Status: **IN PROGRESS.** Written 2026-08-12, corrected for inherited runtime ownership, and advanced
through the serial/provisioning/crt0 boundary on 2026-08-22. The measurements below remain
reproducible historical baselines from the original planning session, not current tree counts.
Implemented: per-title manifests, serial-driven launcher selection, isolated generated namespaces,
an address-free `SpiderRuntime : GameRuntime` base, `Spider1Runtime`, and a direct
`EnterElectroRuntime` with 8/8 measured crt0 facts. Enter Electro deliberately refuses at EE-02;
the remaining title-address extraction from shared `game/` and all rendering/gameplay work are open.

The boot executable/serial is the canonical title key throughout this plan: Spider-Man is
`SLUS_008.75`; Spider-Man 2: Enter Electro is `SLUS_013.78`. Names are labels, not runtime selectors.
Each product target binds to exactly one key and refuses a mismatched disc. The
`titles/spiderman1/` and `titles/spiderman2/` seams are now implemented through EE-02; later steps in
this plan describe the remaining extraction and title behavior, not placeholder creation.

---

## 1. The premise, re-measured (and it holds)

`python3 ../psxport/tools/exe_similarity.py` (the tool lives in **psxport/tools/**, not
`Tomba2Engine/tools/`), over boot executables freshly extracted with `discdump` into `scratch/exes/`:

```
SPIDER1    SLUS_008.75      186879 words -> 138858 unique shingles
SPIDER2    SLUS_013.78      196095 words -> 148939 unique shingles
CRASH1     SCUS_949.00       72191 words ->  62078
SPYRO1     SCUS_942.28      103935 words ->  89936
TOMBA2     MAIN.EXE         178687 words -> 151029

           SPIDER1 SPIDER2 CRASH1  SPYRO1  TOMBA2
SPIDER1     .      74.2%   3.6%    4.7%    11.8%
SPIDER2    74.2%    .      3.6%    4.2%    10.5%
CRASH1      3.6%   3.6%     .      6.6%     4.9%
SPYRO1      4.7%   4.2%    6.6%     .       6.1%
TOMBA2     11.8%  10.5%    4.9%    6.1%      .
```

**74.2% reproduced exactly.** Negative controls in the same run: 3.6–6.6%. The workspace decision is
confirmed by re-measurement, not merely inherited.

**The Tomba!2 loader trap does not apply here — checked, not assumed.** Tomba!2 boots a small
`SCUS_944.54` loader that then runs `MAIN.EXE`, so comparing against its loader inflates the score
with Sony boot boilerplate. Both Spider-Man discs boot their engine directly:

| | boot exe | size | text words | SYSTEM.CNF | other `.EXE` on disc |
|---|---|---|---|---|---|
| SPIDER1 | `SLUS_008.75` | 749,568 B | 186,879 | `BOOT=cdrom:\SLUS_008.75;1` | none |
| SPIDER2 | `SLUS_013.78` | 786,432 B | 196,095 | `BOOT = cdrom:\SLUS_013.78;1` | none |

Both are engine-sized (Tomba!2's engine `MAIN.EXE` is 178,687 words; its *loader* is 167,936 bytes).
So 74.2% is engine ↔ engine.

### Disc inventories, side by side

| | SPIDER1 (Spider-Man USA) | SPIDER2 (Enter Electro USA) |
|---|---|---|
| index + archive | `CD.HED` 12,525 B + `CD.WAD` 63,213,568 B | `CD.HED` 13,053 B + `CD.WAD` 65,261,568 B |
| CD.HED entries | 616 | 633 |
| XA audio | `COMPILED.XA` 198,737,920 B | `COMPILED.XA` 187,334,656 B |
| FMV | `CINEMAS/` 24 `.STR` | `CINEMAS/` 26 `.STR` + `HEADLINE/` 13 `.STR` (new) |
| extra | — | `SPIDER.WAV` 32,281,244 B (new) |
| CD.WAD ext histogram | `.psx` 269, `.rle` 98, `.trg` 66, `.vab` 58, `.sfx` 57, `.bin`/`.rel` 30/30, `.fnt` 5 | `.psx` 282, `.rle` 149, `.vab` 46, `.sfx` 45, `.trg` 44, `.bin`/`.rel` 28/28, `.fnt` 5 |
| runtime code modules | 30 `.bin`+`.rel` pairs | 28 pairs |

### The measurement that matters more than the shingle score

The boot-exe percentage says nothing about whether this repo's *tooling* transfers. So both were run
against Enter Electro directly, with this repo's own unmodified code:

* **`tools/extract_modules.py:parse_index` (the port of guest `FUN_80064B3C`) parses Enter Electro's
  `CD.HED`: 633 entries, 28 `.bin`/`.rel` pairs.** The archive index format is identical.
* **`tools/extract_modules.py:relocate` (the port of guest `FUN_8001BF58`) relocates 28 of 28
  Enter Electro modules cleanly, 0 failures**, each with a plausible type histogram
  (`shell`: 490×R_MIPS_32 / 211×HI16 / 211×LO16 / 737×R_MIPS_26).
* **Negative controls prove that is not a permissive parser:** feeding a module's own code bytes as a
  `.rel` stream → `ValueError: relocation at +0x3C020000 is past the 20560-byte module`; random bytes
  of the same length → rejected the same way. `parse_index` over raw `CD.WAD` payload yields 6 junk
  entries, not 600, so its 633 is not noise.

**So the module/loader half of this port is already title-agnostic in fact** — RE-09's offline loader
works on title 2 today, before any refactor.

### And the measurement that limits the sharing (new, not in the workspace doc)

Boot-exe similarity does **not** extend to the runtime modules. Same normaliser as the validated tool,
applied to relocated module images; 8 stems exist in both titles:

| stem | s1 bytes | s2 bytes | similarity |
|---|---|---|---|
| hostage | 3,808 | 3,880 | 64.8% |
| chopper | 26,340 | 8,464 | 60.8% |
| thug | 46,484 | 58,404 | 53.8% |
| mj | 984 | 1,000 | 52.2% |
| shell | 112,912 | 84,660 | 40.3% |
| turret | 7,508 | 9,772 | 22.2% |
| lizard | 956 | 27,236 | 7.8% |
| training | 3,672 | 3,624 | 1.6% |

Negative control (different modules, cross-title, 8 random pairs): 0.0, 0.1, 0.9, 1.0, 1.0, 2.1, 2.5,
14.9%.

**Read: the ENGINE is one codebase (74.2%); the per-module GAME code is related but rewritten
(1.6–65%).** Two stems share only a name. This is the single most load-bearing constraint on the
layout below: shared code may hold **mechanism** (loader, ring pump, sync, render-seam shape), and must
not assume a native reimplementation of module behaviour ports across titles.

---

## 2. What is already in `game/` that is title-specific

`game/` is 18 files / 3,460 lines. Two kinds of title-specificity, both counted with the framework's
own definition (`external/psxport/tests/test_no_game_address_literals.cpp::is_game_addr`: KSEG0
`0x80010000–0x801FFFFF`, its KSEG1 mirror, scratchpad offsets; `0x80010000` and `0x801FFFF0` exempt),
comments and string literals stripped:

**A. Guest-address literals in live code — 69 across 8 files. 10 of 18 files are already clean.**

| file | literals | what they are |
|---|---|---|
| `game/core/game_config.cpp` | 27 (26 unique) | the whole file IS the title's `GameConfig` |
| `game/core/diag_overrides.cpp` | 17 | 16 override installs + 2 text-range bounds (`0x800C6800`) |
| `game/core/sync_native.cpp` | 9 | `kVSync 0x80084BE0`, `kVSyncCallback 0x8008B8CC`, `kVblankWait 0x8005E748`, 5 libetc/counter globals |
| `game/core/cd_stream.cpp` | 6 | 5 libstr ring globals `0x800C1510..0x800C1520` + `StGetNext 0x80086B10` |
| `game/render/gpu_env.cpp` | 4 | libgpu globals `0x800B0E2B/2C/2E`, `0x800B393C` |
| `game/core/module_loader.cpp` | 3 | loader `0x8001B990`, alloc `0x800651C8`, free `0x800654E8` |
| `game/render/render_seam.cpp` | 2 | `submitFrame 0x80061308`, `currentDb 0x800B54A8` |
| `game/render/scene_id.h` | 1 | level-name `0x800A568C` |
| clean today | 0 | `main.cpp`, `game_hooks.cpp`, `recomp_register.cpp`, `frame_census.{cpp,h}`, `frame_envelope.{cpp,h}`, `gpu_env.h`, `render_seam.h`, `scene_id.cpp` |

Note the raw grep for `0x8xxxxxxx` in `game/` returns **373** hits; only **69** are live code. The
other 304 are provenance comments. A plan sized off the grep count would be sized 5.4× wrong.

**B. Generated-substrate symbol references — the harder half, and easy to miss.**
`game/` names **20 distinct `gen_func_XXXXXXXX` symbols in 60 references**, plus 19
`engine_set_override_main(...)` call sites. Those symbol names are **derived from the guest address
with no title namespace** (`emit.py:534` `MAIN_NAMES = Names("gen_func", …)`,
`emit.py:542` `overlay_names(tag) -> ov_<tag>_*`). A shared file that names `gen_func_80061308` cannot
compile for title 2, and — worse — *would* compile if title 2 happened to have a function at that
address, silently super-calling a foreign body.

**C. Title name / path hardcodes outside `game/`:** 33 occurrences of `spiderman`/`SLUS_008` across 8
files — `tools/ensure_recomp.py` 14, `run.sh` 9, `tools/redump_ram.py` 3, `tools/re_frontier.py` 3,
and 1 each in `callee_contract.py`, `extract_modules.py`, `ra_classes.py`, `ghidra_query.py`.
Plus 9 live references to the `generated/` path (`cmake/spiderman_port.cmake:47,48`;
`tools/ensure_recomp.py:73,74,75`; `callee_contract.py:34`; `ra_classes.py:39`;
`check_resume_switch.py:28`; `ghidra_seed.py:35`).

**D. Registry state at the 2026-08-12 plan capture (historical, not current).**
`python3 tools/re_frontier.py check` → 30 entries parsed, no unknown deps, no cycles.
`next` → RE-13 (scheduler task layout), RE-08 (native per-vertex depth), RE-21 (render-walk
inventory). The `⛔ hack` list was empty then; HACK-03 exists now. Consult the live frontier and
codemap before work rather than treating this capture as current. Two brief/doc defects found while
doing the original measurement are recorded in §6.

---

## 3. The layout

```
game/                     SHARED — lineage code. MUST hold 0 guest-address literals and 0 gen_func_ names.
  core/  main.cpp spider_runtime.cpp sync_native.cpp cd_stream.cpp
         module_loader.cpp diag_overrides.cpp
  render/ render_seam.cpp frame_envelope.cpp gpu_env.cpp frame_census.cpp scene_id.cpp
titles/
  spiderman1/  title.json  spider1_runtime.cpp
  spiderman2/  title.json  enter_electro_runtime.cpp  recomp_seeds.json  port.cmake
generated/
  ...Spider-Man 1 files...                 historical default namespace
  spiderman2/                              Enter Electro namespace
tools/                    unchanged engines, parameterised by --title
docs/                     codemap / re-frontier / issues / info — SHARED, with per-title status rows
```

### `generated/` must be per-title. This is measured, not stylistic.

Emitted symbol and file names are address- and stem-derived with no title prefix, and the two titles
**share 8 module stems** (`chopper hostage lizard mj shell thug training turret`) whose contents differ
(40.3% for `shell`). So one `generated/` would collide on file names (`ov_shell_shard_0.c`), on
dispatch symbols (`ov_shell_dispatch`), and across the whole MAIN text range on `gen_func_*` (both
titles link at `0x80010000`). Two separate executables, two separate substrate directories. There is
no variant of "one binary, both titles" that links.

### One inherited runtime per title; no title configuration bag

Each binary installs one process-lifetime derived runtime before constructing `Game` or `Core`:

```cpp
class SpiderRuntime : public GameRuntime { /* measured shared lineage behavior only */ };
class Spider1Runtime final : public SpiderRuntime { /* SLUS_008.75 facts and overrides */ };
class EnterElectroRuntime final : public SpiderRuntime { /* SLUS_013.78 facts and overrides */ };
```

The split now has an address-free `SpiderRuntime : GameRuntime`. `Spider1Runtime` alone composes the
bounded legacy adapter for framework consumers that still read `Core::cfg`; that debt was not copied
into title 2. Remaining Spider-Man 1 fields must move into narrow typed runtime interfaces over later
steps. Title-specific guest addresses and `gen_func_*` thunks live in the derived title runtime or
the cohesive title subsystem that owns the operation. Do not replace `GameConfig` with a differently
named all-fields binding struct.

Executable identity is likewise inherited policy, not filename convention: each derived runtime
publishes its manifest-bound serial, measured file size, and SHA-256 through `ExecutableIdentity`.
The shared boot authenticates those bytes before runtime installation and `Game` construction.

An unimplemented title operation is an explicit derived override that aborts naming its frontier
step. It is never a zero-filled configuration row and never a silent no-op. This keeps incomplete
Enter Electro work honest while letting shared code use ordinary virtual dispatch. The build target,
not a human-facing name, installs the derived runtime for its canonical boot serial; disc validation
then refuses a serial mismatch before guest execution.

### One cmake build, two binaries

`CMakeLists.txt` uses `set(SPIDER_TITLES "spiderman1" CACHE STRING …)` — **default Spider-Man 1 only**, so a
bare clone and every existing gate command keep working and an incomplete title 2 can never break
title 1's build. Then:

```cmake
include(${PSXPORT_DIR}/cmake/psxport.cmake)          # once: libpsxport + psxport_smoke
foreach(t IN LISTS SPIDER_TITLES)
  include(titles/${t}/port.cmake)                    # defines one executable
endforeach()
```

`cmake/spiderman_port.cmake` keeps the target name **`spiderman_port`** (renaming it would invalidate
every command in `docs/codemap.md` and every gate script); `titles/spiderman2/port.cmake` defines
`enter_electro_port`. Each does:

```cmake
include(${CMAKE_SOURCE_DIR}/generated/${t}/rec_sources.cmake)
list(TRANSFORM GEN_REC_SRCS PREPEND generated/${t}/)
add_executable(<target> ${SHARED_GAME_SRC} titles/${t}/${TITLE_RUNTIME_SRC} ${GEN_REC_SRCS})
```

with the existing per-source `-O1 -foptimize-sibling-calls -fno-strict-aliasing -fwrapv` properties
(the sibling-call flag is a correctness requirement, not an optimisation — see the comment in
`cmake/spiderman_port.cmake`), `add_dependencies(<target> gen_gpu_shaders)`, and
`RUNTIME_OUTPUT_DIRECTORY scratch/bin`.

### The lint that keeps `game/` clean

New: `tools/check_title_literals.py`, modelled directly on
`external/psxport/tests/test_no_game_address_literals.cpp` (same `is_game_addr` definition, same
shrink-only ratchet, same three failure modes: **NEW** = a literal was added; **STALE** = one was
removed without shrinking the baseline; **FIXED** = delete the row). Differences, because this repo
needs two rules where the framework needs one:

1. **Rule A — no guest-address literal in `game/`.** Scans `game/**` only; `titles/`, `generated/`,
   `external/`, `tools/`, `docs/` exempt. Baseline starts at today's **69 across 8 files** and
   shrinks to 0 as §4 proceeds; each move lands with its baseline edit in the same commit.
2. **Rule B — no substrate symbol in `game/`.** Fails on any `gen_func_`, `ov_<tag>_gen`,
   `stub_gen_func`, `shard_set_override`, `ov_<tag>_set_override` token in `game/**`. Baseline today:
   **20 distinct symbols / 60 references**, target 0. `game/core/recomp_register.cpp` is the one
   deliberate exception (it is the substrate seam and moves to `titles/<t>/`).
3. **Refuse, don't return empty:** exit non-zero if `game/` is missing or 0 files were scanned, with
   the text "scanned NOTHING". Every negative prints its denominator: "scanned N files / M hex
   literals, 0 game addresses, blind to: runtime-computed addresses, addresses arriving via a
   `GuestFn` field."
4. **`--selftest`, wired into the same command CI runs:** a synthetic file containing
   `0x80061308` + `gen_func_80061308` MUST produce exactly 2 findings, and a clean synthetic file
   (`0x1F801810`, `0x80000080`, `0xBFC00000`, `0x80010000`) MUST produce 0. Both directions, because a
   one-sided discriminator is not known to discriminate.
5. **Wiring:** spider1 has **0 `add_test` and no `enable_testing()`** today, so this is the repo's
   first test. Add `enable_testing()` + `add_test(NAME title_literals COMMAND ${Python3_EXECUTABLE}
   tools/check_title_literals.py)` so `ctest` runs it, and call the same script from the pre-commit
   hook (none exists today; `git config core.hooksPath` is unset).

### What breaks if this is done wrong

* **A shared file keeps one title literal — the primary failure mode.** `game/render/render_seam.cpp`
  keeping `0x80061308` compiles for `spider2_port` only if some `gen_func_80061308` exists in title 2's
  substrate. Given 74.2% shingle overlap and a shared link base, *the odds that address is populated in
  title 2 are high, and the odds it is `submitFrame` are low.* The port then installs the render seam on
  an arbitrary function, super-calls a foreign body, and either draws nothing or diverges silently. This
  is exactly the `ot_attr.cpp` decay the framework's test was written for (Tomba's packet-pool constants
  made spyro/spider1 report "no spans", indistinguishable from "the guest submitted no packets") — except
  a mis-bound *override* executes rather than merely observing. Rule A + Rule B are the only mechanical
  defence; a paragraph in a doc is not one.
* **Copying Spider-Man 1 runtime facts into Enter Electro to "get it booting".** The two link differently
  (`STACK=801fff00` vs `801FFFF0`, text 0xB6800 vs ~0xC0000 bytes). 74.2% shingle similarity is
  address-INDEPENDENT by construction — the tool masks immediates and jump targets precisely so link
  addresses cannot matter. It is therefore *no evidence at all* that any single address transfers. A
  copied config is the jump-ahead the frontier tracker exists to catch, and it would show up as a
  plausible-looking boot.
* **Sharing `generated/` or building one binary for both.** Does not link (measured above).
* **Assuming a native module reimplementation ports.** `training` measures 1.6% and `lizard` 7.8%
  between titles. Native behaviour code belongs in `titles/<t>/` unless a measurement says otherwise.
* **Renaming `spiderman_port` or making title 2 build by default.** Every gate command and doc
  reference breaks, and title 2's intentionally incomplete derived runtime would abort the default
  product run.

---

## 4. Order of work, with the gate for each step

**Step 0 — the refactor gate, already captured.** `python3 tools/gate.py boot --seconds 90 --
./scratch/bin/spiderman_port scratch/bin/spiderman/SLUS_008.75` (headless, `PSXPORT_NOAUDIO=1`) on the
binary built 2026-08-12 11:01 produced, in `scratch/logs/s2plan/baseline.log`:

```
[rseam] render seam installed at 0x80061308
[module] module placement watcher installed (loader 0x8001B990, alloc 0x800651C8, free 0x800654E8)
[scene] FIRST scene identity: name='....' code=0xFFFFFFD0 (call #1, frame 3)
[rseam] submitFrame override REACHED — call #1 at frame 3, ra=80061218, leg=psx_render
[scene] CHANGED scene identity: name='dem1' code=0x9901 (call #2, frame 2275)
[rseam] submitFrame calls=512 frame=3382 leg=psx scene='dem1' sceneChanges=2
[scene] CHANGED scene identity: name='l1a1' code=0x0101 (call #705, frame 3898)
[rseam] submitFrame calls=1024 frame=4537 leg=psx scene='l1a1' sceneChanges=3
recomp-MISS count: 0     exit=130 (wall-clock timeout)  device losses: 0
```

Steps 1–5 are **pure refactors of title 1**, so this sequence is an exact-match gate: any change to the
scene/call/frame sequence or a non-zero `recomp-MISS` is a regression, not noise. (Frame numbers may
drift with host timing; call numbers and the scene sequence must not.)

| step | work | verification |
|---|---|---|
| **1** | Add `tools/check_title_literals.py` with today's counts as baseline. No code moves. | `python3 tools/check_title_literals.py` exits 0 on HEAD; `--selftest` yields 2 findings on the dirty synthetic and 0 on the clean one; adding `0x80061308` to `game/render/frame_census.cpp` in a throwaway edit makes it FAIL "NEW". No rebuild needed. |
| **2** | Introduce the shared `SpiderRuntime` base and `titles/spider1/Spider1Runtime`; move title literals and substrate thunks **subsystem by subsystem**, cheapest first: scene identity → render seam → module loader → GPU environment → CD stream → sync → diagnostics → remaining legacy facts. Each operation becomes a narrow virtual/runtime-owned interface, not one replacement binding bag. Shrink the baseline in the same commit as each move. | After **each** subsystem: `cmake --build build --target spiderman_port -j$(nproc)`, then the Step-0 gate. Lint baseline must be strictly smaller each time (a STALE failure means a move landed without its baseline edit). End state: Rule A and Rule B baselines both **0**, `Spider1Runtime` derives through the shared lineage runtime, and no legacy config/hooks adapter remains. |
| **3** | `generated/` → `generated/spider1/`; update the 9 live path references. | `PSXPORT_FORCE_RECOMP=1 python3 tools/ensure_recomp.py` regenerates the same **135** files under the new path and the same `.recomp.hash` content; rebuild; Step-0 gate. |
| **4** | `titles/<t>/title.json` (boot-exe name, disc env var, module list, link base); teach `ensure_recomp.py --title` (14 hardcodes), `redump_ram.py` (3), `run.sh` (9). | `python3 tools/ensure_recomp.py --title spider1` reproduces the identical hash from step 3 (byte-compare `generated/spider1/.recomp.hash`). |
| **5** | cmake split: `PSXPORT_TITLES` + `titles/<t>/port.cmake`, default `spider1`. | Fresh `cmake -S . -B build-fresh && cmake --build build-fresh --target spiderman_port` produces a binary that passes the Step-0 gate; `psxport_smoke` still builds; `-DPSXPORT_TITLES=spider1` (the default) is what a bare clone uses. |
| **6** | **First Enter Electro content — provisioning only.** Add `titles/spider2/title.json`, an empty `recomp_seeds.json` (empty on purpose, same reasoning as title 1's), and `EnterElectroRuntime`, whose only established identity is `SLUS_013.78` and whose first unresolved operation explicitly aborts naming its frontier step; add `.env` key `PSXPORT_SPIDERMAN2_DISC`. | `ensure_recomp.py --title spider2` extracts `SLUS_013.78` and relocates **28/28** modules (already proven offline this session: 28 clean, 0 failures) and emit.py produces a substrate; `cmake --build build --target spider2_port` links. Then: **running `spider2_port` must ABORT** at that explicit unresolved runtime operation. That abort is the pass — a boot would mean something was faked. A `SLUS_008.75` disc must be rejected as a serial mismatch. |
| **7** | RE title 2 for real, starting where title 1 started: the crt0 group (`tools/redump_ram.py --title spider2` then the framework's `disasm.py` at the PS-EXE entry). File title-2 frontier steps in `docs/re-frontier.md` with a `spider2:` prefix. | Each field cited with the instruction it came from, per this repo's standing rule. **No address copied from title 1** — the shingle metric is address-independent and cannot license one. |

Nothing before step 6 touches Enter Electro at all, and every step before it is verifiable on the
binary that exists today.

---

## 5. Cost, measured

| item | measured cost |
|---|---|
| shared `game/` today | 18 files / 3,460 lines; **10 files already title-clean** |
| literals to relocate | **69** live game-address literals in 8 files (raw grep says 373 — 304 are comments) |
| substrate symbols to move behind title runtime ownership | **20** distinct `gen_func_*` in **60** references; **19** `engine_set_override_main` sites |
| new shared code | one small inherited lineage runtime base plus narrow subsystem interfaces as required |
| new per-title code | `title.json`, one derived runtime, `recomp_seeds.json`, and `port.cmake`; no title configuration bag |
| cmake edits | 2 live lines in `cmake/spiderman_port.cmake` + 1 `foreach` in `CMakeLists.txt` |
| path edits | 9 live `generated/` references; 33 title-name occurrences in 8 tool/script files |
| new lint | ~250 lines + selftest; repo's **first** `add_test` |
| substrate footprint | 135 files / 16 MB per title (title 2 adds a second, unavoidable) |
| verification cost per step | one incremental build + one 90 s headless run |

The work is dominated by step 2, and step 2 is cheap for a mechanical reason worth stating: all 69
literals except `diag_overrides.cpp`'s 19 call sites are already gathered in `static constexpr`
declarations at the top of their files, so the move is a declaration-to-struct-field edit, not a
scattered hunt.

---

## 6. Two documentation defects found while writing this (fix them, they cost sessions)

1. **`docs/codemap.md` "Where is X" says the `RE_FRONTIER_ROADMAP` env var is REQUIRED. It is not, and
   has not been since 2026-08-11** — `CLAUDE.md` §1 already records the fix. Verified both ways this
   session: bare `python3 tools/re_frontier.py next` and the `RE_FRONTIER_ROADMAP=docs/re-frontier.md`
   form print identical output, and `check` reports 30 entries parsed from the correct path. The stale
   codemap row propagated into an agent brief this session, which is exactly the cost a stale note has.
2. **`tools/whatis.py` does not exist in this repo** (`ls`: no such file). The registry entry points
   that do exist are `tools/info.py`, `tools/catalog.py`, `tools/re_frontier.py`, `tools/codemap.py`
   — and `tools/codemap.py` is also absent; `docs/codemap.md` is maintained by hand here.
