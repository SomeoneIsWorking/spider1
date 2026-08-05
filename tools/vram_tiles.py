#!/usr/bin/env python3
"""vram_tiles.py — is there anything IN this VRAM dump, and WHERE?

WHY THIS EXISTS. "Are the textures actually uploaded?" has now been answered twice by hand — once in
issue 0006 and again in 0007 — by tiling a raw VRAM dump and counting distinct 16-bit values per
tile. Doing it inline in a shell one-liner each time is how the analysis gets re-derived instead of
consulted, so it is a tool.

WHAT IT SHOWS, and the distinction that matters on this port: PSX VRAM is one 1024x512 16-bit page
holding BOTH the framebuffers and the texture/CLUT atlas. A tile with 1-2 distinct values is blank; a
tile with hundreds or thousands holds real picture or texture data. The split by x=512 is the useful
summary here because this port's display sits at x<512 and its atlas at x>=512.

THE TRAP THIS TOOL MUST NOT HIDE, stated in its own output: a dump of the CPU-side s_vram is NOT this
port's rendered frame. With the VK backend on, the composite is built in the GPU texture and never
written back to s_vram, so the framebuffer half reading BLANK is the EXPECTED, correct result and
says nothing about whether the port renders. See issue 0006 and instruments.md INST-18/INST-20. The
atlas half is the half a CPU-side dump can speak about.

    python3 tools/vram_tiles.py scratch/raw/vram.bin [--tile 64]
"""
import sys, struct

W, H = 1024, 512

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    T = 64
    if '--tile' in sys.argv:
        T = int(sys.argv[sys.argv.index('--tile') + 1])
    path = args[0]
    d = open(path, 'rb').read()
    if len(d) != W * H * 2:
        # Refuse rather than score a truncated buffer: a short dump would produce a confidently
        # wrong "blank" for every tile past the end.
        print("REFUSING: %s is %d bytes, expected %d (%dx%d x u16). Nothing scored."
              % (path, len(d), W * H * 2, W, H))
        return 1
    px = struct.unpack('<%dH' % (W * H), d)
    grid = []
    for ty in range(H // T):
        row = []
        for tx in range(W // T):
            s = set()
            for y in range(ty * T, (ty + 1) * T):
                base = y * W
                for x in range(tx * T, (tx + 1) * T):
                    s.add(px[base + x])
            row.append(len(s))
        grid.append(row)
    print("%s — distinct 16-bit values per %dx%d tile" % (path, T, T))
    print("      " + " ".join("%5d" % (tx * T) for tx in range(W // T)))
    for ty, row in enumerate(grid):
        print("y=%3d " % (ty * T) + " ".join("%5d" % c for c in row))
    ntiles = (W // T) * (H // T)
    rich = [(ty * T, tx * T, c) for ty, row in enumerate(grid) for tx, c in enumerate(row) if c > 16]
    left = [r for r in rich if r[1] < 512]
    right = [r for r in rich if r[1] >= 512]
    print("\nrich tiles (>16 distinct): %d of %d scanned" % (len(rich), ntiles))
    print("  x<512  (display half): %d" % len(left))
    print("  x>=512 (atlas half)  : %d" % len(right))
    if not rich:
        print("\nNOTHING is rich anywhere. Either this dump is of a genuinely empty VRAM, or it was")
        print("taken before anything was uploaded. It does NOT by itself mean the port draws nothing.")
    if not left:
        print("\nNOTE: the display half is blank. On a VK-backed run that is EXPECTED and is NOT")
        print("evidence the port renders nothing — the composite lives in the GPU texture and is")
        print("never written back to CPU s_vram. See issue 0006, instruments.md INST-18/INST-20.")
    return 0

if __name__ == '__main__':
    sys.exit(main())
