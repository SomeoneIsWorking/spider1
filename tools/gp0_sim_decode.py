#!/usr/bin/env python3
"""THE DISCRIMINATOR. Take the frame's raw GP0 prims (guest ground truth) plus the port's own VRAM,
and decode every textured primitive's texels HERE, in Python, by the PSX spec — texture window,
page, bit depth, CLUT — with no code shared with the port. Then count the distinct colours that
decode produces.

  * If Python also gets a small colour count, the port's texture SAMPLING is reproducing what an
    independent spec decode of the SAME VRAM gets, and the missing detail is in VRAM CONTENT
    (upload/decompression/CLUT provisioning) — NOT in addressing or shader arithmetic.
  * If Python gets thousands of colours from the same VRAM and the same prims, the port's sampling
    or window application is wrong, and the difference localises it.

Both branches print their denominator: prims decoded, texels sampled, texels that decode to 0
(the PSX "transparent" value the shader discards).
"""
import sys, os, struct, collections
import numpy as np

def poly_len(op):
    nv = 4 if (op & 8) else 3
    n = 1 + nv * (1 + (1 if (op & 4) else 0))
    if op & 0x10: n += nv - 1
    return n

def cmd_len(w):
    op = w >> 24
    if 0x20 <= op <= 0x3F: return poly_len(op)
    if 0x60 <= op <= 0x7F:
        n = 2
        if op & 4: n += 1
        if ((op >> 3) & 3) == 0: n += 1
        return n
    if 0x40 <= op <= 0x5F: return 4 if (op & 0x10) else 3
    if op == 0x02: return 3
    if op == 0x80: return 4
    if op in (0xA0, 0xC0): return 3
    return 1

def main(gp0, vrampath):
    for p in (gp0, vrampath):
        if not os.path.exists(p):
            print(f"FATAL: {p} missing — decoded NOTHING", file=sys.stderr); return 2
    vram = np.fromfile(vrampath, dtype='<u2')
    if vram.size != 1024*512:
        print("FATAL: vram dump wrong size — decoded NOTHING", file=sys.stderr); return 2
    vram = vram.reshape(512, 1024)
    words = struct.unpack('<%dI' % (os.path.getsize(gp0)//4), open(gp0,'rb').read())

    tw = [0, 0, 0, 0]     # mx, my, ox, oy
    prims = 0; texels = 0; zeros = 0
    colours = collections.Counter()
    per_prim_distinct = []
    i = 0
    while i < len(words):
        w = words[i]; op = w >> 24
        if op == 0xE2:
            tw = [w & 31, (w >> 5) & 31, (w >> 10) & 31, (w >> 15) & 31]; i += 1; continue
        if 0x20 <= op <= 0x3F and (op & 4):
            n = poly_len(op)
            if i + n > len(words): break
            gouraud = op & 0x10; nv = 4 if (op & 8) else 3
            k = 1; clut = tpage = None; uv = []
            for v in range(nv):
                if gouraud and v > 0: k += 1
                k += 1
                uvw = words[i+k]; k += 1
                uv.append((uvw & 0xFF, (uvw >> 8) & 0xFF))
                if v == 0: clut = (uvw >> 16) & 0xFFFF
                if v == 1: tpage = (uvw >> 16) & 0xFFFF
            tpx, tpy, depth = (tpage & 0xF)*64, ((tpage >> 4) & 1)*256, (tpage >> 7) & 3
            cx, cy = (clut & 0x3F)*16, (clut >> 6) & 0x1FF
            u0 = min(p[0] for p in uv); u1 = max(p[0] for p in uv)
            v0 = min(p[1] for p in uv); v1 = max(p[1] for p in uv)
            seen = set()
            for vv in range(v0, v1+1):
                for uu in range(u0, u1+1):
                    U = (uu & ~(tw[0]*8)) | ((tw[2] & tw[0])*8)
                    V = (vv & ~(tw[1]*8)) | ((tw[3] & tw[1])*8)
                    if depth == 2:
                        t = int(vram[(tpy+V) & 511, (tpx+U) & 1023])
                    elif depth == 1:
                        word = int(vram[(tpy+V) & 511, (tpx+(U >> 1)) & 1023])
                        idx = (word >> ((U & 1)*8)) & 0xFF
                        t = int(vram[cy & 511, (cx+idx) & 1023])
                    else:
                        word = int(vram[(tpy+V) & 511, (tpx+(U >> 2)) & 1023])
                        idx = (word >> ((U & 3)*4)) & 0xF
                        t = int(vram[cy & 511, (cx+idx) & 1023])
                    texels += 1
                    if t == 0: zeros += 1
                    else: colours[t & 0x7FFF] += 1; seen.add(t & 0x7FFF)
            per_prim_distinct.append(len(seen))
            prims += 1
            i += n; continue
        if 0x40 <= op <= 0x5F and (op & 8):
            j = i + 1
            while j < len(words) and (words[j] & 0xF000F000) != 0x50005000: j += 1
            i = j + 1; continue
        i += cmd_len(w)

    if prims == 0:
        print("NEGATIVE: 0 textured prims in this stream — decoded nothing. Not a result about "
              "texturing; a result about the capture.")
        return 1
    ppd = np.array(per_prim_distinct)
    print(f"decoded {prims} textured prims, {texels} texels sampled")
    print(f"texels that decode to 0 (PSX-transparent, the shader discards these): {zeros} "
          f"({100.0*zeros/texels:.1f}%)")
    print(f"DISTINCT 15-bit COLOURS an independent spec decode gets from the port's own VRAM: {len(colours)}")
    print(f"per-prim distinct colours: min {ppd.min()} median {int(np.median(ppd))} "
          f"mean {ppd.mean():.1f} max {ppd.max()}; prims yielding <=2 colours: "
          f"{int((ppd <= 2).sum())}/{prims}")
    print("top texel colours (15-bit hex, count):",
          ' '.join(f"{c:04X}:{n}" for c, n in colours.most_common(10)))
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("usage: gp0_sim_decode.py <gp0raw_fN.u32> <vram.bin>", file=sys.stderr); sys.exit(2)
    sys.exit(main(sys.argv[1], sys.argv[2]))
