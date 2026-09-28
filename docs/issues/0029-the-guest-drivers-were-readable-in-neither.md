# 0029 — the guest drivers were readable in neither direction: 1,652 lines of address soup

**Status:** resolved 2026-09-28 by a behaviour-preserving extraction. Kept because the operator's
directive was to make the code readable first and to record what that pass found, and because the
evidence for "it changed nothing" is the interesting half.

## The problem, measured

The two largest files in this repository were both guest operations, and both were transcripts rather
than descriptions:

| file | lines before | what a reader could not do |
|---|---|---|
| `titles/spiderman1/spider1_mode_driver.cpp` | 912 | 88 bare `constexpr uint32_t` addresses at the top, 20+ inline literals in the bodies, a 13-arm state switch in which five arms were the same three-line guard written five times, a 90-line `switch` whose nine arms were the retail jump table, and two ~140-line retail mode functions inline in a 900-line class |
| `titles/spiderman1/spider1_frame_driver.cpp` | 740 | 27 bare addresses, the VSync(0) return-value formula written out at THREE call sites, ten `core.r[29] + NN` register spills as an unrolled list, four near-identical presentation commits each carrying its own copy of the "one fence per host step" check, and the whole host-stepped-fiber lifecycle spread across six members and five methods interleaved with pad service and widescreen latching |

Underneath both, six facts were duplicated rather than owned:

* the "wait a field only if this frame's work crossed none" comparison, three times;
* the "drain fields until the GPU reports done" loop, three times;
* the "service the submitted frame's field once" handshake, three times;
* the libetc VSync(0) return-value formula, three times;
* `kPadService` and `kPadRead`, one guest function, two names, two files;
* `kAlternateUpdate` and `kUiUpdate`, likewise.

## What was done

Eleven cohesive owners, each one concept, and the two files now hold only what is theirs:

| file | class or namespace | the one concept |
|---|---|---|
| `spider1_guest_layout.h` | `spider::spider1` constants | every named guest fact the title's drivers touch — one owner, declared once, at the name |
| `spider1_mode_decisions.h` | `spider` free functions | the rules the modes decide by, as pure functions of what they observed |
| `spider1_mode_host.h` | `Spider1ModeHost` | what a mode may ask the frame owner to do, and nothing else |
| `spider1_guest_call.h` | `spider1CallGuest` | the mode drivers' synchronous-guest-call convention |
| `spider1_guest_stack_frame.h` | `GuestStackFrame` | a bounded guest stack window for one call chain |
| `spider1_mode_frame_boundary.*` | `spider1ModeDrainDrawFields`, `spider1ModeWaitUnadvancedField`, `spider1ModeCompleteFrameHandshake` | the display-field boundary the three retail modes share |
| `spider1_menu_mode.*` | `Spider1MenuMode` | the title menu: object lifetime, per-field UI script, exit result |
| `spider1_alternate_mode.*` | `Spider1AlternateMode` | the attract mode: object lifetime, per-field UI script, repeat probe |
| `spider1_invalid_selector_input.*` | `Spider1InvalidSelectorInput` | the invalid-selector press-or-timeout gate |
| `spider1_transition_wipe.*` | `Spider1TransitionWipe` | the authored 3D transition, its row loop, and its channel arithmetic |
| `spider1_field_clock.*` | `Spider1FieldClock` | the field clock libetc VSync(0) is defined in terms of |
| `spider1_gpu_reset.*` | `Spider1GpuReset` | ResetGraph's GPU reset sequencing, without its VSync |
| `spider1_cd_initialization.*` | `Spider1CdInitialization` | the CdInit body this port owes: callback slots and the interrupt arm |
| `spider1_host_stepped_fiber.*` | `Spider1HostSteppedFiber` | the finite fiber the boot prefix and the mode steps run on |

## The evidence that it changed nothing

1. **The whole guest boundary is identical, mechanically.** 132 synchronous guest calls before, 132
   after; 127 distinct `(entry, return address)` pairs before and after; an equal multiset, so every
   entry address, every `jal` return address the port resumes at, and every call COUNT is unchanged.
   The one call whose entry is data rather than a constant — the menu's indirect vtable ending — has its
   return address `0x80016388` checked in both trees. Reproduce with
   `python3 scratch/mutants/compare_guest_calls.py`; it reads the pre-refactor files out of git and
   resolves every constant to its value, so a rename cannot hide a difference.
2. **Eighteen decision boundaries, eighteen mutants.** Every extracted decision has a planted change
   that turns a suite red, shown red, and reverted; see the table in the session report and
   `scratch/mutants/run_mutants.py`. Eighteen of eighteen turned red.
3. **One mutant found a real bug — mine, introduced by the extraction and caught before it shipped.**
   `spider1ModeFrameNeedsFieldWait` was written as `count != start`, the INVERSE of the retail
   `count == start`, so every mode would have waited a field it should not have. The first run of
   `spider1_mode_rules` failed on it. It is fixed, and the mutant table keeps it as a permanent case.
   This is the whole argument for extracting the decision rather than leaving the comparison inline:
   the inversion was invisible in a 900-line file and unmissable in a named function with a test.
4. **The gate.** 28 of 28 pre-existing CTests green before and after, plus two new suites
   (`spider1_mode_rules`, `spider1_guest_boundary_owners`, 685 checks between them). The two pin rows
   are red for an unrelated reason: the framework checkout has an untracked doc in it from another arm,
   which `psxport_sync.py --check` correctly refuses. See the session report.

## Line counts, before and after

| file | before | after |
|---|---|---|
| `spider1_mode_driver.cpp` | 912 | **559** |
| `spider1_frame_driver.cpp` | 740 | **573** |
| the two, together | 1,652 | **1,132** |

Neither file was on a legacy size list: both sat under the gate's 1,200-line default, so there was no
list to come off. They now carry **shrink-only caps at exactly 573 and 559**, registered in
`CMakeLists.txt`, so the debt cannot regrow. The gate fails a capped file that shrank while keeping its
cap, so each cap has to be lowered again in the same change that makes its file smaller.

## What was deliberately NOT extracted, and why

* **The primary mode** (`initializePrimary`, `stepPrimaryWarmup`, `stepPrimary`, `finishPrimary`, ~100
  lines) stayed in the mode driver. It is the largest and most entangled block, it drives the state
  machine's most-used transition, and splitting it would have meant threading a fence-decision return
  value back through the dispatcher for no readability gain a reader would notice.
* **The three bootstrap service boundaries** (`serviceBootstrapVsync`, `serviceBootstrapMovieVsync`,
  `serviceBootstrapStreamWait`) stayed in the frame driver. They are the pre-main field owner, they are
  almost entirely guard conditions and one field delivery each, and they are the code most likely to be
  touched by the in-flight CD work. Moving them during a black-frame investigation is the wrong time.
* **`stepFrame`'s presentation half** stayed. What was extracted from it is the fiber; the rest is pad
  service, widescreen latching, the two plan applications, and the fence check, in the order they must
  happen. Reordering that is a behaviour change.

## Related

`0027` (found by this pass, not fixed) and `0028` (the eight facts this pass named honestly, and the two
duplicate guest-function names it collapsed).
