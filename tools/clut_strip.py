#!/usr/bin/env python3
"""clut_strip.py — what is IN the CLUT strip (VRAM x=512..767, y=0..79) of a raw VRAM dump?

Issue 0007: every textured prim in the 3D scene points its CLUT into that rectangle, and the
rectangle reads 100% 0x3333 (pale green) on the defect dumps. This tool answers "is it real
palette data or the poison value" for ANY dump, with its denominator, so nobody re-derives it
in a shell one-liner again.

    python3 tools/clut_strip.py <vram.bin> [more.bin ...] [--x0 512 --x1 768 --y0 0 --y1 80]

Output per file: halfwords scanned, distinct values, the top values with counts, the fraction that
is 0x3333, and the first row's first 16 halfwords (so a real palette is visible as such).
A file of the wrong size is REFUSED, not scored.
"""
import sys, struct
from collections import Counter

W, H = 1024, 512

def main():
    argv = sys.argv[1:]
    box = {'x0': 512, 'x1': 768, 'y0': 0, 'y1': 80}
    files = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a.startswith('--'):
            box[a[2:]] = int(argv[i + 1]); i += 2
        else:
            files.append(a); i += 1
    if not files:
        print(__doc__); return 2
    rc = 0
    for path in files:
        try:
            d = open(path, 'rb').read()
        except OSError as e:
            print("REFUSING %s: %s — scanned NOTHING" % (path, e)); rc = 1; continue
        if len(d) != W * H * 2:
            print("REFUSING %s: %d bytes, expected %d — scanned NOTHING"
                  % (path, len(d), W * H * 2)); rc = 1; continue
        px = struct.unpack('<%dH' % (W * H), d)
        c = Counter()
        for y in range(box['y0'], box['y1']):
            base = y * W
            c.update(px[base + box['x0']: base + box['x1']])
        n = sum(c.values())
        print("== %s  box x=%d..%d y=%d..%d" % (path, box['x0'], box['x1'], box['y0'], box['y1']))
        print("   scanned %d halfwords, %d distinct" % (n, len(c)))
        print("   0x3333: %d/%d = %.1f%%" % (c.get(0x3333, 0), n, 100.0 * c.get(0x3333, 0) / n))
        print("   top: " + " ".join("%04X:%d" % (v, k) for v, k in c.most_common(8)))
        r0 = px[box['y0'] * W + box['x0']: box['y0'] * W + box['x0'] + 16]
        print("   row%d x%d..: %s" % (box['y0'], box['x0'], " ".join("%04X" % v for v in r0)))
        # per-row distinct, so a strip that is real in ONE row is not averaged away
        rows = []
        for y in range(box['y0'], box['y1']):
            base = y * W
            rows.append(len(set(px[base + box['x0']: base + box['x1']])))
        nz = [(box['y0'] + i, v) for i, v in enumerate(rows) if v > 1]
        print("   rows with >1 distinct value: %d of %d%s" % (len(nz), len(rows),
              ("  -> " + " ".join("y%d:%d" % t for t in nz[:20])) if nz else ""))
    return rc

if __name__ == '__main__':
    sys.exit(main())
