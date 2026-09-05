---
id: 7
title: the 3D world renders geometry with no textures — flat/gouraud only
status: resolved
symptom: gameplay 3D scene is monochrome green: character, floor, walls all flat-shaded with no texture detail; only 369 distinct colours in a full-screen 3D frame
tags: render,3d,texture,gameplay,unverified-cause
created: 2026-08-05
updated: 2026-08-06
---

FOUND 2026-08-05 by driving the port past the front-end for the first time and LOOKING at a picture.
Nobody had done that before: until this session no instrument in the project could show the presented
picture (INST-18/INST-20), and the codemap asserted the port stopped at name entry, so nothing beyond
the menu had ever been captured.

HOW TO REPRODUCE (headless, ~200 s, no window needed):

    PSXPORT_NOWINDOW=1 PSXPORT_NOAUDIO=1 PSXPORT_FORCE_BUTTONS=4000 \
    PSXPORT_SHOT_AT=10000 PSXPORT_PRESENT_SHOT_AT=10000 \
    PSXPORT_WATCHDOG=120 timeout 200 ./run.sh
    python3 ../spyro/tools/ppm_look.py scratch/screenshots/{shot,present}_10000.ppm

PSXPORT_FORCE_BUTTONS pulses CROSS (8 frames on / 24 off), which walks the front-end deterministically.

WHAT IS ON SCREEN at present 10000: a third-person 3D scene — Spider-Man in an interior, floor,
walls, ceiling fixtures, a HUD box bottom-right with an orange arrow. The GEOMETRY and the CAMERA
look right. The SHADING does not: every surface is a smooth gradient with no texture detail
anywhere, and the whole scene is mapped into green/yellow. The character is uniformly pale green
(he should be red and blue).

THE NUMBER THAT MATTERS: 369 distinct colours in a full-screen 3D frame. A textured PSX scene has
thousands (this port's own main menu at present 3900 has 1048, and its intro FMV frames 8825-11395).
369 is what pure flat/gouraud shading with NO texture sampling looks like.

IT IS NOT A GLOBAL FILTER, so 'everything is tinted green' is the wrong description: a blue object
on the right stays blue and the HUD box stays brown with an orange arrow. Some primitives carry
correct colour; the world surfaces and the character carry none.

RULED OUT — the present stage / the new present-image-sink composite (so NOT the fade, the letterbox
or the 24bpp decode). The guest-VRAM shot at the same present is THE SAME PICTURE: shot_10000
512x240 98.7% non-black / 369 colours vs present_10000 960x720 61.7% / 369 colours, the coverage gap
being exactly the widescreen letterbox (512:240 fills 450 of 720 rows = 62.5%; 98.7 x 0.625 = 61.7).
Identical colour count means the composite added and removed nothing. The defect is UPSTREAM of the
present stage, in whatever fills VRAM.

NOT YET INVESTIGATED, and deliberately not guessed at: whether this is missing texture-page uploads,
a CLUT/texture-window problem, the native raster ignoring texture bits, or the guest never issuing
textured primitives at all. RE-08 (Render: GTE tap -> native depth) is open and the codemap notes
native depth coverage is only a few percent, so the textured/3D path is known to be immature — but
no measurement here says which of those it is. Do not close this by picking one.

SECOND, SEPARATE OBSERVATION from the same run, filed here only so it is not lost: present 4500 is
the New York skyline (a 2D backdrop) with heavy blocky corruption — torn horizontal bands of
displaced pixels across the buildings. 468 colours. Different symptom, probably a different cause;
worth its own entry once someone looks at it.

VERDICT IS THE USER'S. This is a measurement, not a closed diagnosis: the port reaches 3D gameplay
and draws recognisable geometry, and the surfaces are untextured. Whether that is the expected state
of this port today or a regression is not something our own numbers can settle.

### Note (2026-08-05)
NARROWED 2026-08-05, and one reading taken during the follow-up was WRONG — recorded because the way
it was wrong is a reusable trap.

THE FALSE NEGATIVE FIRST. PSXPORT_GPU_TRACE prints `batch tri= tex= semi=` and it read 0/0/0 at
EVERY sampled present in the 3D scene. Read naively that says "no primitive reaches the native
rasterizer" — dramatic, and wrong. `batch` is the LIVE accumulator (s_tri_n/s_tex_n/s_semi_n)
sampled at the TOP of GpuVkState::present(): AFTER the previous frame_end reset it (gpu_vk.cpp:1569)
and BEFORE this frame's drawing. On a game that presents at the top of its frame loop it is
LEGITIMATELY 0 every time. The trace line now also prints `drawn tri= tex= semi=` (s_dbg_*_c — what
render_geom actually rasterised, the counters gpu_vk_stats and the debug server already report), so
that sample point can no longer produce the lie.

WHAT THE CORRECTED INSTRUMENT SAYS: textured primitives DO reach the native rasterizer and ARE drawn.
Sampled present 11600:

    batch tri=0 tex=969 semi=204 | drawn tri=0 tex=969 semi=204

969 textured + 204 semi-transparent, and tri=0 — essentially EVERYTHING the native raster draws in
this scene is submitted as TEXTURED. Corroborated at the GP0 level: PSXPORT_DEBUG=gpu reports 924
prims / 10114 gp0words per drawing frame (~11 words/prim = textured shaded polys), alternating with
0-prim frames (the game draws at 30 Hz).

REFUTED, so nobody re-derives them:
  * "the guest never issues textured primitives" — it issues ~924/frame and they are textured.
  * "the primitives never reach the native rasterizer" — 969 tex + 204 semi drawn in one present.
  * "the native raster is not running in the 3D scene" — it is.

WHICH LEAVES, still not distinguished: texture SAMPLING produces no detail. Narrowed to texture-page
/ CLUT content or addressing — the atlas not uploaded for this scene, texture page / CLUT
coordinates resolving wrong, or the texture window applied wrong. 369 distinct colours over a
full-screen frame that submits 969 textured prims is consistent with sampling a flat or empty region.

CAVEAT ON THE DENOMINATOR, stated rather than glossed: GPU_TRACE samples every 200th present, and
only one of six sampled presents in the tail showed non-zero draw counts. 200 is even, so the sample
lands on a fixed parity against a 30 Hz draw cadence — the sampling is biased and the true
per-present distribution is NOT established. Do not read "1 of 6" as a duty cycle.

### Note (2026-08-05)
FORK SPLIT 2026-08-05: the texture atlas IS uploaded and IS full of real texture data. So "the atlas
was never uploaded for this scene" is REFUTED, and the remaining cause is ADDRESSING or SAMPLING.

MEASURED. Full CPU-side VRAM dumped during the 3D scene and tiled 64x64, scored by distinct 16-bit
values (tools/vram_tiles.py, written for this so the analysis stops being re-derived by hand — issue
0006 did the same thing manually):

    PSXPORT_NOWINDOW=1 PSXPORT_NOAUDIO=1 PSXPORT_FORCE_BUTTONS=4000 \
    PSXPORT_VRAMDUMP="11900:scratch/raw/vram3d.bin" PSXPORT_WATCHDOG=120 timeout 220 ./run.sh
    python3 tools/vram_tiles.py scratch/raw/vram3d.bin

    rich tiles (>16 distinct): 60 of 128 scanned
      x<512  (display half): 0
      x>=512 (atlas half)  : 60      <- 700..2786 distinct values per tile

WHY THAT SETTLES IT: GpuVkState::present() is handed the CPU-side s_vram as `src` and uploads it as
BOTH the render target seed and the texture-source snapshot (upload_vram -> s_vram_tex + s_vram_snap,
which is what render_geom binds as the sampler). So the very buffer whose atlas half is provably rich
IS the texture source the native raster samples from. The data is there and the sampler can reach it.

Combined with the previous note (969 textured prims per frame ARE submitted and rasterised), the
cause is now bounded to: the texture PAGE / CLUT coordinates the prims carry, the texture window, or
the sampling arithmetic in the shader — i.e. the prims are addressing the atlas wrongly, not missing
it. Still not distinguished between those, and still not to be closed by picking one.

DENOMINATOR AND BLIND SPOT, stated: the display half of that dump reads BLANK (1-2 distinct values
per tile), and that is EXPECTED, not a finding — with the VK backend on, the composite is built in
the GPU texture and never written back to CPU s_vram (issue 0006, INST-18). A CPU-side VRAM dump can
speak about the ATLAS half only. tools/vram_tiles.py prints that caveat itself so the next reader
cannot mistake the blank half for "the port renders nothing" — which is exactly the false conclusion
issue 0006 was opened to record.

NEXT MEASUREMENT (not done): compare the texture-page / CLUT coordinates the submitted prims actually
carry against where the rich atlas tiles physically are. PSXPORT_PRIMDUMP exists (gpu_native.cpp
prim_dump_close_if_done) and is the likely instrument. If the prims point outside the rich region, it
is addressing; if they point into it, it is the sampling/CLUT decode.

### Note (2026-08-05)
ROOT CAUSE FOUND 2026-08-05. The textures are fine. THE PALETTES ARE NOT: every CLUT the scene uses
is the constant 0x3333, and 0x3333 in PSX 1555 is RGB(152,200,96) — pale yellow-green. That single
value IS the symptom.

    python3 -c "v=0x3333; print((v&31)<<3, ((v>>5)&31)<<3, ((v>>10)&31)<<3)"   ->  152 200 96

THE CHAIN, each link measured:
 1. ALL 1765 textured prims in a 6-frame window are CLUT-INDEXED — 1230 4bpp, 535 8bpp, ZERO
    15bpp-direct. So every textured surface's colour comes through a palette lookup, with no
    direct-colour path to mask the fault.
 2. ALL 1765 point their CLUT into VRAM x=512..767, y=0..79.
 3. That rectangle is 100.0% the single value 0x3333. Independently confirmed on FOUR separate VRAM
    dumps (three by an agent, one by me: 20480/20480 halfwords, 1 distinct value). 0x3333 occupies
    21919 of 524288 VRAM words (4.2%) and is confined to exactly that rectangle.
 4. ALL 1765 prims carry raw=0, i.e. MODULATE. So the shader computes
    vertex_colour x RGB(152,200,96) for every textured pixel in the scene — a smooth gouraud
    gradient tinted pale green, with no texture detail. That is precisely the reported picture, and
    it is why the frame holds only 369 distinct colours.
 5. AND IT EXPLAINS THE ANOMALY THAT DID NOT FIT ANY EARLIER STORY: the blue prop and the brown HUD
    box keep correct colour because they are among the 358 UNTEXTURED prims in the same window.
    Untextured prims never touch a CLUT, so they are the only things on screen unaffected.

WHY THE ADDRESSING WAS NEVER THE PROBLEM (all three original candidates are now dead):
 (A) tpage/CLUT coords — 0 of 1765 prims have texpage x<512; their addressed texels are RICH (up to
     11027 distinct words in one prim's uv box; 1543/1765 address >16). The coordinates land on real
     texture data AND on the strip the guest itself re-uploads. They are correct.
 (B) texture window — 913 of 1765 prims carry a ZERO window (no mask, no offset) and are just as
     flat as the 852 that carry a real one. A pass-through window cannot cause a defect.
 (C) sampling/CLUT-decode arithmetic — an INDEPENDENT Python decode sharing no code with
     tritex.frag, fed the same prims and the same VRAM, reproduces the same impoverished result
     (30 distinct colours over 2.69M texels sampled). The port's arithmetic is faithfully decoding
     what its VRAM says. Separately, a static read found the 4bpp/8bpp unpack and the window formula
     character-identical to the nocash spec, and NO vertex-colour fallback: tritex.frag does
     `if (texel == 0u) discard;`.

DECISIVE POSITIVE CONTROL, which is what makes this a root cause and not a correlation: feeding the
SAME 597-prim set through the SAME decode against a VRAM dump whose CLUT strip holds REAL palettes
yields median 6 / max 11 distinct texels per prim and only 14.6% flat, versus 597/597 flat on the
defect dump. Same coordinates, same code, different palette data, different answer.

WHO WRITES 0x3333 — THE GUEST DOES, from guest RAM that is already 0x33-filled.
PSXPORT_TEXWATCH="512,0,768,80" (18923 hits in an agent's run, 29419 in mine) shows the guest
re-uploading the whole CLUT strip every drawing frame via GP0(0xA0), and the SOURCE BYTES are
already wrong in RAM:
    [texwatch] f133 A0 dest=(640,0) 16x1 src=0x801461A4 srcbytes: 33 44 44 44 44 33 33 33 00 33 33 33
    [texwatch] f15427 A0 dest=(512,0) 256x12 src=0x801E4060 srcbytes: 33 33 33 33 33 33 33 33 ...
No FILL and no 0x80 VRAM->VRAM copy touches the strip — and that negative is now worth something,
because TEXWATCH previously watched only A0 and 80copy and was BLIND to both GP0(0x02) fills and the
native upload path. Those two coverage holes were closed before this negative was taken; without
that, "nothing else wrote here" would have been a confident silent lie.

SO THE FAULT IS UPSTREAM OF THE GPU ENTIRELY — it is an asset/palette LOAD problem, not a renderer
problem. The renderer is faithfully drawing the data it was given. Nothing in gpu_native.cpp,
gpu_vk.cpp or the shaders needs to change for this.

STRONGEST LEAD FOR THAT UPSTREAM BUG, observed but NOT chased: one CLUT upload descriptor reads
src=0x801FFD20 for 256x68 halfwords = 34816 bytes, ending at 0x80208520 — PAST THE 2 MB RAM END
(core.h: uint8_t ram[0x200000], i.e. valid through 0x801FFFFF). That upload cannot be reading a
valid source from where it points. Whether the descriptor is wrong, or the palette DMA/decompression
that should have filled that buffer never ran, is the next question. Start there.

NOT FIXED, NOT CLOSED. This is a root-cause identification backed by measurement; the fix is
upstream and unwritten, and per PROTOCOL the user closes the bug, not our own numbers.

### Note (2026-08-05)
UPSTREAM CHASE, 2026-08-05 — partial, and the honest boundary is stated at the end.

WHO FILLS THE PALETTE BUFFER: guest code, at guest frame 3, as a WORD FILL of 0x33333333.
PSXPORT_WWATCH=801461A4,801461C4 (96 hits over the run) on the CLUT source buffer TEXWATCH named:

    [wwatch] f3   store [801461A4]=33333333 ... ra=800653D4
    [wwatch] f134 store [801461C0]=00000044 ... ra=80064E20   (byte stores, later)

RULED OUT — the PORT is not the source of the pattern:
  * psxport's native heap HLE (hle.cpp heapAlloc/heapFree, A0:0x33/0x34) does NOT poison memory —
    grepped, there is no fill of any kind in either body. The 0x33/A0:0x33 coincidence is just the
    BIOS malloc function number.
  * the framework nowhere fills guest RAM with 0x33 (grepped the PSX runtime).
So the 0x33 is the GUEST's own write, most plausibly a memset(buf, 0x33, n) placeholder (the BIOS
memset is HLE'd at hle.cpp:368, A0:0x2B) that a later real palette load was supposed to overwrite.
The 0x44 byte stores at f134 show SOMETHING writes the buffer later — but not real palette data.

STILL OPEN: which load should have filled that buffer, and why it does not. Two shapes remain — the
palette load never runs, or it runs and targets a different address. Not distinguished.

TWO INSTRUMENTS CAUGHT LYING ON THE WAY, both worth more than the progress above:

 1. PSXPORT_RAMDUMP_FRAME IS STRUCTURALLY INERT ON THIS PORT, AND SAYS NOTHING. It fires from the
    NATIVE frame loop (native_boot.cpp:555). spider1 does not run that loop — it dispatches the
    guest's own main() on the recompiled substrate ("[boot] Phase 0: dispatching guest main()
    0x8002C354"). A 400 s run with PSXPORT_RAMDUMP_FRAME=11900 produced NO file and NO log line of
    any kind. "No dump" and "this knob does nothing here" are indistinguishable, which is this
    project's recurring failure shape. Do not use it on spider1 until it either works or refuses out
    loud. Recorded as instruments.md INST-22.

 2. PSXPORT_WWATCH's `pc` IS NOT TRUSTWORTHY IN RECOMPILED CODE — the address and value are.
    It attributed the fill to pc=0x80064FA0. That address disassembles to `sll $a1, $a1, 2` — not a
    store at all, and the containing function decompiles (Ghidra) to heap free-list coalescing.
    Core::pc is not updated per-instruction inside recompiled bodies, so the reported pc is wherever
    it was last set, not the storing instruction. The WATCH ITSELF IS GOOD: address, value, width
    and frame are real and reproducible. Only the attribution is fiction, and it is exactly the
    field an RE session would act on. Recorded as INST-23.
    (This is why the RE-first rule matters: hand-walking from that pc would have produced a fifth
    wrong attribution of the kind tools/ghidra_query.py's own header documents.)

### Note (2026-08-05)
=============================================================================================
CORRECTION + ROOT CAUSE, 2026-08-05. MY EARLIER HEADLINE CLAIM WAS WRONG, AND THE WAY IT WAS
WRONG IS THE MOST REUSABLE LESSON IN THIS ISSUE.
=============================================================================================

WHAT I WROTE: "that rectangle is 100.0% the single value 0x3333, confirmed on FOUR INDEPENDENT
VRAM dumps". Every dump really did read 100.0%. The generalisation was still false.

THE STRIP ALTERNATES. It is BIMODAL with a period of roughly 64 presents: either 61 distinct
values (REAL palettes) or exactly 1 (100% 0x3333). Never anything between. Verified by me across
every dump on disk:

    vram_11600 / 11604 / 11900 / alt10000 / pal2020      61 distinct, 0x3333 =   0.0%
    vram3d / 3d_a / 3d10000 / coordlens / lens / lens9800
      / menu3900 / pal9612 / pay / alt10032                1 distinct, 0x3333 = 100.0%

alt10000 vs alt10032 — thirty-two presents apart, same run — are the two halves of the cycle.

MY FOUR DUMPS WERE NOT INDEPENDENT IN THE WAY THAT MATTERED. They were independent in COUNT and
identical in PHASE: all four were taken at present indices that happen to land in the wiped half.
Four confirmations of the same systematic error read exactly like four confirmations of a fact.
**When sampling a signal, independence means independence in PHASE, not in number.** Recorded as
instruments.md INST-24.

THE ACTUAL ROOT CAUSE — A MISSING FRAMEWORK FEATURE, not a guest bug:

  **psxport does not implement GP0(0xC0), VRAM->CPU readback.** Before today it was recognised only
  as a 3-word FIFO header (gpu_native.cpp:1260) and then fell through to gp0_exec() and was silently
  ignored — no pixels returned, no diagnostic.

  The guest performs a SAVE / modify / RESTORE round-trip over the palette strip (the RE lens names
  0x80069D44 and 0x8006A154, a save-greyscale-restore). The SAVE half is GP0(0xC0): read the live
  palettes out of VRAM into a RAM buffer. Because the port never returns any pixels, that buffer is
  left holding whatever it already held — the guest allocator's 0x33333333 poison fill, which is
  where the 0x33 comes from and why no palette load appeared to be missing. The RESTORE half is
  GP0(0xA0), which faithfully uploads that poison back over the whole strip.

  So: the palette load RUNS, it LANDS EXACTLY WHERE THE PRIMS POINT, and it is then overwritten by
  the port's own unimplemented readback. Both branches of the fork I posed ("never runs" vs "lands
  elsewhere") were wrong, and three lenses said so independently.

CONFIRMED IN CODE BY ME: a search for `StoreImage` in the pre-migration PSX runtime returned nothing, and the 0xC0 arm
of the GP0 dispatcher did not exist at that revision (its `gpu_native.cpp` had
op==0xC0 only in the FIFO-length table). The handler now present in the working tree — which reports
every readback with its rect and destination address, so that silence is evidence of absence — was
added during this investigation and is NOT yet committed to psxport.

WHY THIS ALSO EXPLAINS EVERY EARLIER OBSERVATION:
  * the strip being poison in every dump we ever took — sampling phase, above;
  * the guest "filling" the buffer with 0x33333333 at frame 3 — that is the ALLOCATOR's poison in a
    buffer the guest expects the GPU to fill, not a placeholder the guest wrote as data;
  * why the textures are rich and correctly addressed — nothing was ever wrong with them;
  * why untextured prims keep correct colour — they never sample a CLUT.

NOT YET DONE: implementing GP0(0xC0). That is the fix, it is a framework change, and it is unwritten.
The blast radius is wider than this issue — ANY guest that saves and restores a VRAM region is
affected on ALL THREE ports, and the failure is silent by construction.

### Note (2026-08-05)
FULLER PICTURE, 2026-08-05 (second workflow). Three corrections/extensions to the entry above, one of
which could otherwise produce a FALSE "fixed".

1. IT IS NOT ONE MISSING PATH, IT IS THREE. All verified in code by me:
     * GP0(0xC0)  — length-parsed as a 3-word header, then dropped. No pixels produced.
     * DMA2 device->RAM — gpu_native.cpp:2075, the loop body is
         for (i..count) { if (to_gpu) { ...gpu_gp0... } addr += 4; }
       so with to_gpu==0 it advances the address and writes NOTHING to guest RAM.
     * GPUREAD  — mem.cpp:487, `if (p == 0x1F801810) return 0;   // GPUREAD (VRAM-store path: minimal)`
   A fix that implements only GP0(0xC0) leaves two silent holes. Whichever path this game uses,
   all three should end up honest — either implemented, or refusing out loud.

2. THE GUEST CODE IS IDENTIFIED FROM THE BINARY'S OWN STRINGS, which is the strongest form of
   identification available (spider1/CLAUDE.md: the most useful ID in this port so far came from a
   diagnostic string the binary emits, not from pattern-matching control flow):
     FUN_80081C50 -> FUN_80081a0c(s_LoadImage_80095e4c, ...)   == LoadImage
     FUN_80081CB0 -> FUN_80081a0c(s_StoreImage_80095e58, ...)  == StoreImage
     FUN_80069D44 == the SAVE + luminance-convert half (mallocs 0x8800 and 0x1800 — exactly the
                     256x68x2 and 256x12x2 upload chunk sizes — then per row: StoreImage(256x1) ->
                     convert 256 entries in place -> LoadImage(row))
     FUN_8006A154 == the RESTORE half (LoadImage 256x68, LoadImage 256x12, free both)
   Host backtrace confirms the live chain (guest pc is fiction per INST-23; the HOST stack is not):
     gpu_gp0 <- gen_func_80082EF4 <- gen_func_800834C4 <- gen_func_80081C50 (LoadImage)
             <- gen_func_8006A154 <- ... <- gen_func_8002C354 (guest main)
   MEASURED VOLUME: 29760 GP0(0xC0) readbacks in one run, 29520 of them overlapping the CLUT strip,
   at rects (512,12) 256x68, (512,0) 256x12, and (512,y) 256x1 for every y in 0..79. The counter is
   printed every 1000 frames EVEN WHEN ZERO, so silence would have been evidence.

3. **THE CAVEAT THAT MATTERS MOST — DO NOT EXPECT COLOUR BACK, AND DO NOT CLAIM IT AS THE GATE.**
   This is a GREYSCALE FILTER: the guest deliberately reads the palette page, luminance-converts it,
   and writes it back. In the NON-poison half of the cycle every value in the strip is already
   greyscale in 1555 (r==g==b): 0x0421, 0x8421, 0xFBDE(30,30,30), 0x7BDE, 0x14A5, 0x8C63.
   So implementing the readback correctly should make the world properly GREY, not colourful.
   The level's TRUE colour palettes DO load correctly much earlier (f133-f135, e.g.
   dest=(512,13) 256x1 distinct=8+: 9000 800A AC00 BC00), and the filter overwrites them.
   THE REMAINING QUESTION, now the top open one: WHY is a greyscale filter running during ordinary
   gameplay at all? Candidates, none established: a pause/menu effect stuck on; a damage or
   cutscene flash whose exit condition never fires; a game mode the port lands in by accident; or a
   filter that is SUPPOSED to run and whose result the game later blends. Do not implement the
   readback and declare the picture fixed — verify what the filter is FOR first.
   (A further tell that the current "rich" phase is itself corrupt, not merely grey: the per-row
   distinct count decays monotonically down the strip — y0:46, y1:44, y2:42 ... y19:11 — the
   signature of a stack buffer being re-converted rather than a row genuinely read back from VRAM.)

INSTRUMENT ADDED, and it closes INST-22's gap: PSXPORT_GRAMDUMP="frame:path" dumps the full 2 MB of
guest RAM at a GPU frame, from the gpu path — so it works on spider1, which never runs the native
frame loop that PSXPORT_RAMDUMP_FRAME depends on. It found the save buffers still holding heap
poison at exactly the malloc'd sizes: 0x801E4648 len 34816 (0x8800) and 0x801ECE50 len 6144 (0x1800).

### Note (2026-08-05)
=============================================================================================
GP0(0xC0) IMPLEMENTED. The poison is gone from the CLUT strip at every phase sampled, and the
world is textured. FRAMEWORK CHANGE, not a spider1 one — patch at
coord/patches/gpu-vram-readback.diff, claim at coord/claims/gpu-vram-readback/.
=============================================================================================

WHAT WAS ACTUALLY MISSING, both halves. The previous note named GP0(0xC0) but not the drain: psxport
had TWO dead paths, not one.
  * `mem.cpp` GPUREAD (0x1F801810 read) was a hard `return 0`.
  * `gpu_dma2_block`'s `to_gpu == 0` arm advanced its address pointer and did NOTHING else, so a
    VRAM->CPU DMA "completed", CHCR's busy bit cleared, the guest's DrawSync passed, and the
    destination buffer kept what it already held. That is the silence.

THE FIX: GP0(0xC0) arms a readback cursor (s_rd*) that mirrors the A0 upload cursor exactly, and both
drains call ONE `GpuState::gpu_read_word()`. The two parameter words are decoded by a shared
`vram_xfer_rect()` used by A0 and C0 alike, so the directions cannot drift apart; wrap is `vram()`'s
existing &1023/&511, matched rather than invented.

WHICH DRAIN THIS GAME USES — settled by ABLATION, run in BOTH directions rather than reasoned about:
    GPUREAD live, DMA2 drain disabled : strip at present 10032 = 1 distinct, 0x3333 100.0%  (bug back)
    DMA2 drain live, GPUREAD disabled : strip at present 10032 = 62 distinct, 0x3333 0.0%   (fixed)
So spider1 drains readbacks EXCLUSIVELY through DMA channel 2. GPUREAD is implemented and covered by
the hermetic test, but this game never reads it.

NEGATIVE CONTROL, stated because this issue's history is a chain of measurements that could not have
shown the failure. Same instrument (tools/clut_strip.py, 20480 halfwords scanned, denominator
printed, wrong-sized dump refused), same headless mode, MY run, on a build with both drains reverted
to HEAD behaviour:
    before, present 10000 : 61 distinct, 0x3333 =   0.0%
    before, present 10032 :  1 distinct, 0x3333 = 100.0%     <- the failing answer
AFTER, FIVE PHASES sampled because four same-phase dumps once produced a confident false conclusion
here (INST-24):
    10000: 62 distinct | 10009: 62 | 10016: 62 | 10032: 3246 | 10048: 62   — 0.0% poison at ALL FIVE
The phase that was 100% poison is the one that now reads 3246 distinct values. CAVEAT ON THAT
DENOMINATOR: four of the five give an IDENTICAL histogram, because after the fix the auto-drive parks
in the pause menu and the strip stops cycling — they are not five independent samples of the old
alternation. The load-bearing comparison is the SAME INDEX: present 10032, 100.0% poison before,
0.0% after.

THE VK WRITE-BACK LIMITATION IS REAL AND DOES NOT BITE HERE, and that is a measurement, not an
assumption. s_vram is the readback's source of truth; under the VK backend the RENDERED picture lives
in the GPU texture and is never written back (issue 0006 / INST-18), so a readback of a natively
rasterised region would return stale data. The implementation counts exactly that case (s_c0_stale,
reported on the per-frame texwatch line). One run: 28946 readbacks, 0 of them overlapping the display
area. Every readback this game makes is of guest-uploaded CLUT/texture content, so all 28946 are
exact. If a future scene reads back the framebuffer, the counter says so out loud instead of serving
a plausible wrong answer.

THE PICTURE: present shot 960x720 at 10000, 372 distinct colours before -> 528 after. The before
frame is this issue's pale-green untextured world; the after frame has brick, windows, water, HUD
glyphs and Spider-Man in red/blue. NOT A CLEAN A/B, said plainly: once the game gets its palettes
back the CROSS-pulse auto-drive walks differently, so the after frame is a different scene (a pause
overlay over gameplay) rather than the same one re-rendered. NOT VERIFIED: an unpaused 3D gameplay
frame after the fix — the auto-drive parked in the pause menu across 10000..11600.

VERDICT IS STILL THE USER'S. The palette-poison mechanism this issue root-caused is fixed and
measured gone; whether the 3D world now LOOKS right in a window is not something these numbers
settle.

### Resolution (2026-08-06)
CLOSED 2026-08-06. The cause was found, the fix is in, and the picture is right.

THE FIX: psxport now implements VRAM->CPU readback — and it was TWO dead paths, not one:
  * mem.cpp GPUREAD (0x1F801810) was a hard `return 0;`
  * gpu_dma2_block's to_gpu==0 arm advanced its address pointer and wrote NOTHING
GP0(0xC0) itself was only a FIFO-length entry. The transfer therefore LOOKED complete to the guest
(busy bit cleared, DrawSync passed) while returning zero bytes — silent by construction, which is
why the symptom presented as "the textures are broken" for so long.
The guest's save/modify/restore over the palette strip then restored its allocator's 0x33333333
poison over the live CLUTs. Every textured prim is CLUT-indexed and raw=0 (modulate), so every
textured pixel became vertex_colour x RGB(152,200,96) — the pale green.

WHICH DRAIN THIS GAME USES WAS ESTABLISHED BY ABLATION, RUN IN BOTH DIRECTIONS rather than reasoned:
  GPUREAD live, DMA2 drain disabled -> strip at present 10032: 1 distinct, 0x3333 = 100% (bug BACK)
  DMA2 drain live, GPUREAD disabled -> strip at present 10032: 62 distinct, 0x3333 = 0%  (fixed)
spider1 drains exclusively through DMA2. Both paths are implemented anyway, because both are
reachable framework paths and the sibling ports are unmeasured.

NEGATIVE CONTROL: the hermetic test went 0/5 -> 5/5, and its headline failure is the game bug in
miniature ("poison == 0: got 32" — all 32 destination words still held 0x33333333 after the save).
On real data, a build with the drains reverted reproduces 100% poison at the same present index.

THE PICTURE, same present index, verified by me: 369 distinct colours of uniform pale green ->
1581 colours at 99.7% non-black, Spider-Man in red and blue on a New York rooftop with a working
HUD. (The 4:3 half of that came from issue 0008, fixed alongside.)

A PREDICTION OF MINE THAT WAS WRONG, kept because it was load-bearing for a whole tick: I argued the
world would come back correctly GREY rather than colourful, because the strip's non-poison phase is
itself greyscale (r==g==b) and the guest is running a luminance filter. What I missed is that the
filter is the PAUSE-SCREEN DIM, and its RESTORE half is what puts the colour back — the thing the
missing readback had broken. The greyscale was never the resting state; it was half a round-trip we
had truncated.

STILL OPEN, and deliberately NOT closed with this: the city-skyline screen (present 4500) shows
heavy blocky corruption. Different symptom, unexamined since, and it needs its own entry if it
survives — do not assume this fix touched it.
