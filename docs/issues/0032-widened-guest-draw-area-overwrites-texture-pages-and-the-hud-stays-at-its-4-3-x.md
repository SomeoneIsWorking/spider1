---
id: 32
title: The widened 684-wide guest draw area overwrites the texture pages and CLUTs at VRAM x 512..683, and the 4:3-authored overlay stays at x 256
status: open
symptom: at 16:9 in the attract demo, black quads appear at the right edge (polygons sampling overwritten textures), and the "DEMO" caption stays centred on x 256 in the 684-wide frame
state_items: S008
tags: widescreen,render,vram,texture,hud,gte-path
created: 2026-10-09
updated: 2026-10-09
---

## Black quads

Observed: `dem3` at 16:9, frame 8000, polygons whose screen x reaches past 512 draw solid black
(guest packets with tpage 0x39/0xaa/0x29 and CLUT 0x121/0xac). Expected: the textured geometry of the 4:3
picture continuing into the margin.

Cause: `Spider1Widescreen::publishDrawEnvironment` (`titles/spiderman1/spider1_widescreen.cpp`) writes RECT.w = 684
into the guest draw environment, so the guest's E4 is x 683 for both frame buffers (0,0 and 0,256). The Gte
path rasterizes the guest's draws into the one VRAM image, and the retail layout keeps texture pages and CLUTs at
x 512..1023 beside the 512-wide buffers. The widened draws overwrite x 512..683 on rows 0..239 and 256..495.
The packets are not emitted black; they sample overwritten texels. Measured on the VRAM image at frame 8000
(`scratch/wide-edge/vram_4x3.ppm`, `vram_16x9.ppm`): x 512..683 differs in 99.8% of the pixels on rows 0..239 and
92.4% on rows 256..495, against 8.9% in the untouched control x 684..1023 rows 0..255. Emitters are the guest's
face writers (`FUN_8007C4D8` / `FUN_8007D978` under `FUN_8002BD5C`); the cause is the host-visible VRAM overlap, not an
emitter, a cull owner or a fog term.

Why it is not fixed in the title: no VRAM placement of a 684-wide buffer avoids the textures (free VRAM is only
x 640..1023, rows 256..511). Keeping RECT.w retail removes the corruption and also the right margin. The
margin must live outside VRAM, as the Record path's canvas does. Record path is not usable yet: the guest keeps the
retail centre 256 there, so the cull window would have to widen to the left of 0, and `FUN_8007C2AC` compares
unsigned halfwords (`PSXPORT_RENDER_PATH=record` aborts in `publishProjection`: OFX 342 vs plan centre 256).

Proper fix, in order: (1) a margin target for the Gte path in psxport (guest draws left of 0 or past the guest's own
E4 go to a canvas, never VRAM; texture reads stay on VRAM), (2) a title-declared guest x origin for that canvas, so
the world shifted by the margin (OFX 342, window 0..684) lands in the right columns, (3) then RECT.w stays retail.

## "DEMO" caption

Emitter: `FUN_80017AF4` (jal at `0x8002C120` in the render walk `FUN_8002BD5C`) calls `FUN_80019A90(0x100, 200,
PTR_DAT_80097898, 0, 0x1000)`, the font string writer, with x hard-coded to 256 and centred alignment. The same function
draws "game over", "really quit" and "insert controller" at x 256. Found by watching the stores into the glyph packets at
frame 8800 (`PSXPORT_WWATCH`, writer pc `0x80019B4C`) and reading the saved return address from the writer's frame
(`0x80017BBC`). The frame is claimed widened by `guestCoordinatesWidened`, so the host does not centre it, and the
caption sits at 229..287 of 684 columns.

What is missing: the Gte path decides 4:3-versus-final per frame (`wide_2d_guest_space`), with no per-call-site or
per-packet declaration, and Spider-Man has no 2D producer. The framework needs a seam where a title names a guest span
(the packets emitted between entry and return of `FUN_80017AF4`) as authored 4:3 inside a widened frame, which the host
then centres by the margin. The span attribution (`OtAttr`) records nothing on this path (`otattr` reports 0 spans).
A title-side shift of the emitted packets by the margin would work but is a second layout owner beside the host's.

Reproduction: `scratch/wide-edge/run.py --aspect 16x9 --at 8000|8800` through `heavy.py`.
