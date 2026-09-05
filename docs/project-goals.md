# Project goals — spider1

These goals record durable product intent. Current coverage is independent in
`docs/project-state.md`; atomic work is in `docs/issues/`.

## G001 — Two authenticated Neversoft Spider-Man PC products

Deliver Spider-Man and Spider-Man 2: Enter Electro as distinct native PC targets in one
same-engine-lineage repository.

Why it matters: the games share Neversoft architecture, but executable identity and title facts must
never leak across products.

Success conditions:

- A supplied disc selects exactly one product by authenticated boot serial, size, PS-X EXE identity,
  and SHA-256.
- Each title has its own authenticated runtime image and derived native policy.
- Shared lineage code contains no title address, emitted guest body, or title-specific dispatch.
- All guest behavior not replaced natively executes on demand through Lightrec from the player's
  original executable or runtime-loaded module.
- Both titles reach playable game-owned execution through their own measured boundaries.

Constraints and non-goals:

- Human-readable game names are labels, never selectors.
- Enter Electro does not inherit Spider-Man rendering or gameplay policy before its own RE proves it.
- The repository does not distribute copyrighted executables, discs, or assets.

Related state: S001, S002, S003, S014.

## G002 — Native widescreen and true temporal presentation for both titles

Replace each title's guest-produced picture with its own native producers that support a wider
projection and true per-object temporal interpolation.

Why it matters: post-GTE/OT output has already discarded world depth and pre-quantisation motion, so
it cannot produce correct widescreen, native depth, or interpolated 60fps.

Success conditions:

- Complete picture-producing scenes in both titles run without an offline-generated render body.
- Native geometry comes from pre-GTE game state and joins the framework depth/render queues.
- Each title publishes widescreen projection from its own active viewport/camera contract.
- Animated objects interpolate stable authored poses between game frames without replaying guest
  logic or interpolating guest packets.
- Guest-frame fallback debt is absent from the shipping Spider-Man path.

Constraints and non-goals:

- Retail GTE/OT output may remain a diagnostic oracle, never native-producer input.
- Enter Electro exposes neither Spider-Man's native renderer nor 60fps option until it owns its own
  equivalent title-derived product; the current absence is a gap, not a permanent exception.
- The 60fps outcome is true interpolation; frame duplication or retiming guest output is not success.

Related state: S005, S006, S007, S008, S009, S010, S016.

## G003 — RE-driven native subsystem ownership

Move platform and selected game behavior to cohesive native owners while executing every remaining
guest instruction through a runtime Lightrec cache.

Why it matters: readable game-state ownership is the route to a maintainable port; emulating more
hardware or fabricating state only hides the missing owner.

Success conditions:

- Each native boundary is derived from executable evidence and has a falsifying test or instrument.
- A normal guest call honors image-aware native overrides; a scoped original call bypasses only its
  current override and executes through Lightrec.
- Executable-memory writes and runtime module replacement invalidate every affected translated block.
- The gameplay product exposes no interpreter mode or selector; backend fallback is accepted only
  after a classified JIT refusal and remains reason-coded, bounded, and measured.
- Offline guest-code emission, generated corpora, and generated-symbol dispatch are absent from a
  fresh product build.
- Platform, rendering, audio, input, storage, and diagnostics remain separate cohesive owners.
- Approved debt is explicit, bounded, and mechanically excluded from capabilities it cannot support.

Constraints and non-goals:

- No magic guest addresses, guessed constants, swallowed failures, or test-only reimplementations.
- An interpreter-only mode is allowed only in a separately built test/diagnostic target.
- psxport remains game-agnostic; Spider-specific behavior stays in this repository.

Related state: S004, S005, S006, S007, S010, S012, S013.

## G004 — Portable, verifiable player delivery

Make a fresh clone provision, build, and launch the selected product through the default launcher,
with durable checks that distinguish verified behavior from unmeasured behavior.

Why it matters: a warm maintainer checkout or hand-driven run is not a shipping interface.

Success conditions:

- `./run.sh` enters one frozen Python environment and launches the selected native/Lightrec product
  directly from authenticated user-supplied game files.
- Missing native dependencies produce exact user-run platform package commands.
- GCC, Clang, and AppleClang remain accepted player compilers; maintainer evidence uses Clang.
- Hermetic tests, format, clang-tidy, structure, registry, and bounded product gates cover their
  stated denominators and refuse when they cannot assert them.
- Representative interactive gameplay is verified on each released host architecture before that
  host is described as supported.

Constraints and non-goals:

- Ghidra and other maintainer RE tools are not player prerequisites.
- A green focused test does not imply runtime pixels, gameplay, audio, or pacing outside its scope.

Related state: S001, S011, S015.
