# Native/Lightrec migration

This is the only Spider product-execution plan. It follows
`../../../shared/jit-common/docs/migration.md` and replaces the offline-generated-C plan; there is no
static compatibility mode.

## Preserved evidence

Migration changes the execution owner, not the recovered game facts.

- Spider-Man USA is `SLUS_008.75`; its PS-X EXE entry is `0x8008739C`, load address
  `0x80010000`, and text size `0xB6800`.
- Enter Electro USA is `SLUS_013.78`; its entry is `0x80093C68`, load address `0x80010000`, and
  text size `0xBF800`. Eight crt0 facts are independently derived, and its first game-owned call is
  `0x80031F54`.
- The retired Spider-Man product completed both real-disc logo movies, the 300-field post-logo wait,
  and reached early `dem1` under the native frame/service owners. It did not establish representative
  gameplay, complete native rendering, or Lightrec execution.
- The STR player `FUN_8002AA0C` reaches libetc VSync `0x80084BE0` from return PCs
  `0x8002AC8C`, `0x8002AE1C`, and `0x8002AFEC`. These are runtime suspension boundaries, not source
  rewrite sites.
- Thirty CD.WAD modules can coexist at guest-allocated bases. Address alone is insufficient for
  override/cache identity; image generation plus guest address is required.

## Target architecture

The launcher authenticates the selected executable and supplies the original bytes to a per-`Core`
psxport executor. Lightrec owns translation and executable code caching. psxport owns synchronization
with `Core`, service callbacks, bounded exits, complete image identity, native-override dispatch,
scoped original calls, and invalidation. Spider title code owns only measured title policy and native
behavior.

An interpreter may be linked only into a separate test/diagnostic target. The gameplay executable has
no interpreter object, selector, or fallback. Static analysis may retain symbols and address metadata;
it must not emit executable guest function bodies.

## Ordered migration

1. **Shared executor prerequisite.** Integrate the maintained pinned Lightrec revision into psxport as
   a per-`Core` executor. Prove one resident override, an override-bypassing original call, and two
   module images reusing one address, including positive and controlled-negative invalidation.
2. **Spider runtime dispatch.** Replace generated-symbol registration with title/image-aware runtime
   override registration. Route ordinary guest calls and scoped original calls through the executor;
   express frame, host-service, exception, and process exits as bounded executor results.
3. **First discriminator: `dem1`.** Execute nonzero Lightrec blocks from the authenticated
   `SLUS_008.75`, preserve the existing native CD/input/audio/frame/render-seam owners, complete both
   logos, and reach early `dem1`. Replace the generated movie-fiber derivative with suspension and
   resumption of the unchanged retail `FUN_8002AA0C` body at the three authenticated field exits.
4. **Representative gameplay gate.** Drive a bounded interactive route beyond the boot/demo
   checkpoint. Compare timing, interrupts, memory, and relevant device state with an independent
   emulator or separate test oracle; verify native and original calls, module load/unload
   invalidation, correctness, and frame-time budgets on each released host architecture. Inspect the
   gameplay link and public configuration to prove that no interpreter is present or selectable.
5. **Atomic retirement.** Only after step 4 passes, delete the generator, generated corpus,
   emission-only seed manifests, generated dispatcher and symbol tests, build/provisioning rules, and
   obsolete documentation. A fresh checkout must build and launch from the player's authenticated
   game files without offline translation or a pre-populated runtime cache.
6. **Second title.** Continue Enter Electro from `0x80031F54` only after Spider-Man's declared
   compatibility and performance gates pass. Reuse framework mechanics, never Spider-Man addresses or
   unmeasured title behavior.

## Completion boundary

Reaching `dem1` is evidence that the replacement executor is wired to real Spider-Man code. It is not
representative gameplay and cannot trigger static-path deletion. The migration completes only when the
native/Lightrec product is the sole launcher path and the old generated pipeline is absent.
