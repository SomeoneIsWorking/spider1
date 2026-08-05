#!/usr/bin/env python3
"""Cross-check the texture state the GUEST asks for (raw GP0 stream, decoded independently by
gp0_decode.py's rules) against the VRAM the port actually holds at the same frame.

For every distinct (page, depth, clut) the frame's textured prims request, report:
  * how many distinct 16-bit values the requested PAGE region holds  (a page of 1 value = flat)
  * how many distinct 16-bit values the requested CLUT row holds     (a CLUT of 1 value = flat)
and the most common value of each, so "the texture is flat" is a number, not an impression.

NEGATIVE DISCIPLINE: if either file is missing this exits non-zero saying it checked NOTHING.
"""
import sys, os, struct, collections
import numpy as np

def decode_tp(h):
    return dict(x=(h & 0xF) * 64, y=((h >> 4) & 1) * 256, depth=(h >> 7) & 3)

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

def main(gp0path, vrampath):
    for p in (gp0path, vrampath):
        if not os.path.exists(p):
            print(f"FATAL: {p} missing — checked NOTHING.", file=sys.stderr); return 2
    vram = np.fromfile(vrampath, dtype='<u2')
    if vram.size != 1024 * 512:
        print(f"FATAL: {vrampath} is {vram.size} halfwords, expected {1024*512} — checked NOTHING.", file=sys.stderr)
        return 2
    vram = vram.reshape(512, 1024)
    data = open(gp0path, 'rb').read()
    words = list(struct.unpack('<%dI' % (len(data)//4), data[:len(data)//4*4]))

    states = collections.Counter()
    uvs_by_state = collections.defaultdict(list)
    i = 0
    while i < len(words):
        w = words[i]; op = w >> 24
        if 0x20 <= op <= 0x3F and (op & 4):
            n = poly_len(op)
            if i + n > len(words): break
            gouraud, nv = op & 0x10, (4 if (op & 8) else 3)
            k = 1; clut = tpage = None; uv = []
            for v in range(nv):
                if gouraud and v > 0: k += 1
                k += 1
                uvw = words[i+k]; k += 1
                uv.append((uvw & 0xFF, (uvw >> 8) & 0xFF))
                if v == 0: clut = (uvw >> 16) & 0xFFFF
                if v == 1: tpage = (uvw >> 16) & 0xFFFF
            d = decode_tp(tpage)
            key = (d['x'], d['y'], d['depth'], (clut & 0x3F)*16, (clut >> 6) & 0x1FF)
            states[key] += 1
            if len(uvs_by_state[key]) < 4: uvs_by_state[key].append(uv)
            i += n; continue
        if 0x40 <= op <= 0x5F and (op & 8):
            j = i + 1
            while j < len(words) and (words[j] & 0xF000F000) != 0x50005000: j += 1
            i = j + 1; continue
        i += cmd_len(w)

    if not states:
        print("NEGATIVE: zero textured polys in the raw stream — nothing to cross-check.")
        return 1
    print(f"{sum(states.values())} textured prims across {len(states)} distinct (page,depth,clut) states")
    print()
    hdr = f"{'page':>12} {'depth':>5} {'clut':>12} {'prims':>6} {'page_distinct':>13} {'page_top':>9} {'clut_distinct':>13} {'clut_top':>9}"
    print(hdr); print('-'*len(hdr))
    for (px, py, depth, cx, cy), n in states.most_common():
        page_w = 64 if depth == 0 else (128 if depth == 1 else 256)
        blk = vram[py:py+256, px:px+page_w]
        pv, pc = np.unique(blk, return_counts=True)
        ncl = 16 if depth == 0 else (256 if depth == 1 else 0)
        if ncl:
            crow = vram[cy, cx:cx+ncl]
            cv, cc = np.unique(crow, return_counts=True)
            cd, ct = len(cv), f"{int(cv[cc.argmax()]):04X}"
        else:
            cd, ct = 0, 'n/a'
        print(f"({px:4d},{py:3d}) {depth:>5} ({cx:4d},{cy:3d}) {n:6d} {len(pv):13d} {int(pv[pc.argmax()]):>9X} {cd:13d} {ct:>9}")
    print()
    print("interpretation key: page_distinct==1 -> the requested page is a flat block; "
          "clut_distinct==1 -> every index resolves to the SAME colour (flat-shaded look with "
          "gouraud modulation on top). Both >1 means the addressing is landing on real data and "
          "the fault is downstream (sampling/decode).")
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("usage: gp0_vram_check.py <gp0raw_fN.u32> <vram_N.bin>", file=sys.stderr); sys.exit(2)
    sys.exit(main(sys.argv[1], sys.argv[2]))
