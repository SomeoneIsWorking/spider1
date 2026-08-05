#!/usr/bin/env python3
"""UNVALIDATED — NEVER RUN AGAINST REAL DATA. Read this before trusting a single number it prints.

This tool was written for issue 0007 and could not be exercised: the run that would have fed it
(PSXPORT_POLYDUMP=11601) landed on an idle frame and produced 0 polys, because this game draws at
30 Hz and half its frames are empty. So its decode has never been checked against the port's, and
its silence has never been shown to mean anything. Arm it on a frame where `drawn tex=` is non-zero,
confirm it reports a non-zero compared-prim count, and delete this banner once it has been seen to
produce BOTH a match and a mismatch. Until then treat any output as unverified.

Prim-for-prim diff: what the GUEST asked for (raw GP0 words, decoded here by the PSX spec)
vs what the PORT's decoder produced (PSXPORT_POLYDUMP lines) for the SAME frame.

A mismatch localises the fault to gpu_native.cpp's GP0 decode (candidate A, port side).
Agreement refutes it and pushes the fault downstream (window application / sampling).

NEGATIVE DISCIPLINE: prints the denominator on every outcome — how many prims each side saw and
how many were compared. Zero compared prims is reported as a FAILURE to compare, never as "match".
"""
import sys, os, re, struct

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

def guest_polys(path):
    """Every polygon in OT order: (op, textured, clut_xy or None, page_xy or None, verts)."""
    words = struct.unpack('<%dI' % (os.path.getsize(path)//4), open(path,'rb').read())
    out = []
    e1 = 0
    i = 0
    while i < len(words):
        w = words[i]; op = w >> 24
        if op == 0xE1: e1 = w & 0xFFFF; i += 1; continue
        if 0x20 <= op <= 0x3F:
            n = poly_len(op)
            if i + n > len(words): break
            gouraud, textured = op & 0x10, op & 4
            nv = 4 if (op & 8) else 3
            k = 1; clut = None; tpage = e1; verts = []
            for v in range(nv):
                if gouraud and v > 0: k += 1
                xy = words[i+k]; k += 1
                x = xy & 0x7FF; x = x - 0x800 if x >= 0x400 else x
                y = (xy >> 16) & 0x7FF; y = y - 0x800 if y >= 0x400 else y
                verts.append((x, y))
                if textured:
                    uvw = words[i+k]; k += 1
                    if v == 0: clut = (uvw >> 16) & 0xFFFF
                    if v == 1: tpage = (uvw >> 16) & 0xFFFF
            cxy = ((clut & 0x3F)*16, (clut >> 6) & 0x1FF) if textured else (0, 0)
            pxy = ((tpage & 0xF)*64, ((tpage >> 4) & 1)*256)
            out.append((op, 1 if textured else 0, cxy, pxy, verts))
            i += n; continue
        if 0x40 <= op <= 0x5F and (op & 8):
            j = i + 1
            while j < len(words) and (words[j] & 0xF000F000) != 0x50005000: j += 1
            i = j + 1; continue
        i += cmd_len(w)
    return out

PD = re.compile(r"f(\d+) node=\w+ op=([0-9A-F]{2}) tex=(\d) gou=(\d) clut=\((\d+),(\d+)\) tp=\((\d+),(\d+)\).*?V\[\((-?\d+),(-?\d+)\)")

def port_polys(logpath, frame):
    out = []
    for line in open(logpath, errors='replace'):
        if '[polydump]' not in line: continue
        m = PD.search(line)
        if not m: continue
        if int(m.group(1)) != frame: continue
        out.append((int(m.group(2), 16), int(m.group(3)), (int(m.group(5)), int(m.group(6))),
                    (int(m.group(7)), int(m.group(8))), (int(m.group(9)), int(m.group(10)))))
    return out

def main(gp0, log, frame):
    if not os.path.exists(gp0):
        print(f"FATAL: {gp0} missing — compared NOTHING", file=sys.stderr); return 2
    g = guest_polys(gp0)
    p = port_polys(log, frame)
    print(f"guest raw stream: {len(g)} polygons ({sum(x[1] for x in g)} textured)")
    print(f"port POLYDUMP f{frame}: {len(p)} polygons ({sum(x[1] for x in p)} textured)")
    if not g or not p:
        print("FAILURE TO COMPARE: one side has zero polygons. This is NOT a match — it means the "
              "capture did not line up (wrong frame, or POLYDUMP's 2000-line cap, or an idle frame).",
              file=sys.stderr)
        return 1
    n = min(len(g), len(p))
    bad = 0
    for i in range(n):
        go, gt, gc, gp_, gv = g[i]
        po, pt, pc, pp, pv = p[i]
        if (go, gt, gc, gp_) != (po, pt, pc, pp):
            bad += 1
            if bad <= 15:
                print(f"  MISMATCH #{i}: guest op={go:02X} tex={gt} clut={gc} page={gp_} v0={gv[0]}")
                print(f"                port  op={po:02X} tex={pt} clut={pc} page={pp} v0={pv}")
    print()
    print(f"compared {n} prims (positionally, OT order); {bad} disagree on (op, textured, clut, page), "
          f"{n-bad} agree.")
    if len(g) != len(p):
        print(f"NOTE: prim COUNTS differ ({len(g)} vs {len(p)}) — POLYDUMP caps at 2000 lines and only "
              f"logs polys, so a difference here is not by itself a decode bug.")
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("usage: gp0_vs_polydump.py <gp0raw_fN.u32> <run.log> <N>", file=sys.stderr); sys.exit(2)
    sys.exit(main(sys.argv[1], sys.argv[2], int(sys.argv[3])))
