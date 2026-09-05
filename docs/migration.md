# Native/Lightrec migration

This is the only Spider product-execution plan. It follows the canonical shared dynarec methodology
and replaces the offline-generated-C plan; there is no
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

The exact consumer boundary is PSXPort `eb5f23a8b3506f8853b3cfadcedc024cd90818a0` with its required
Lightrec runtime ABI at `b1457137c31cedff5f440d59da29401d021ba2da`. `psxport.pin`, the hosted
checkout, and the CMake dependency refusal must move together; a different or dirty dependency is
not compatible evidence.

There is no interpreter gameplay mode or selector. The maintained backend may use only classified,
bounded automatic block fallback after the JIT explicitly refuses a block; fallback reports its
reason, guest PC, calls, and instruction denominator and must remain below the release threshold. A
separately selected interpreter remains test/diagnostic-only. Static analysis may retain symbols and
address metadata; it must not emit executable guest function bodies.

## Ordered migration

1. **Break-first retirement — complete in the working tree.** The offline generator, emitted corpus,
   seed manifests, generated dispatch registration, static-only tests, selector/build rules, and
   generated movie-fiber source are absent. The launcher provisions only the authenticated PS-X EXE,
   and no old gameplay product remains buildable or selectable.
2. **Shared executor prerequisite — implemented for Linux x86-64.** psxport links its immutable
   maintained Lightrec fork and exposes a per-`Core` executor, image-aware native dispatch, scoped
   original calls, invalidation, typed exits, and exact translation/fallback counters. Its synthetic
   runtime contract proves translated/native/original control flow and self-modifying-code
   invalidation. The exact runtime also enforces a typed per-execution fallback-block limit, with
   positive one-block admission and zero-limit refusal before interpreter execution. Multi-`Core`,
   aggregate fallback-share, complete executable-writer, and non-Linux host qualification gaps
   remain owned by psxport.
3. **Spider runtime dispatch — wired to the boundary.** The product enters the authenticated crt0 via
   `dispatchGuest`; `Spider1MovieExecution` resumes unchanged retail `FUN_8002AA0C` through
   `callOriginal`. Native override promotion must use image generation plus address identity.
4. **First discriminator: `dem1`.** Execute nonzero Lightrec blocks from the authenticated
   `SLUS_008.75`, preserve the existing native CD/input/audio/frame/render-seam owners, complete both
   logos, and reach early `dem1`. Suspend and resume the unchanged retail movie body at the three
   authenticated field exits.
5. **Representative gameplay gate.** Drive a bounded interactive route beyond the boot/demo
   checkpoint. Compare timing, interrupts, memory, and relevant device state with an independent
   emulator or separate test oracle; verify native and original calls, module load/unload
   invalidation, correctness, and frame-time budgets on each released host architecture. Inspect the
   gameplay configuration to prove that no interpreter mode is selectable and that all automatic
   fallback is classified, bounded, and measured.
6. **Second title.** Continue Enter Electro from `0x80031F54` only after Spider-Man's declared
   compatibility and performance gates pass. Reuse framework mechanics, never Spider-Man addresses or
   unmeasured title behavior.

## Completion boundary

The destructive half of the migration and the asset-free Lightrec link are complete. Reaching `dem1`
will prove that the replacement executor is wired to real Spider-Man code, but migration completion
still requires representative gameplay, cache/override conformance, bounded fallback, and host
performance evidence.
