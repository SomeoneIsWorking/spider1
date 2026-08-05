#!/usr/bin/env python3
"""Decode a RAW GP0 word stream (PSXPORT_GP0RAW=<frame> -> scratch/logs/gp0raw_f<N>.u32)
against the PSX GPU spec, INDEPENDENTLY of the port's own decoder in gpu_native.cpp.

Purpose: tell "the game asks for the wrong texture page/CLUT/window" apart from "the port
decodes or samples a correct request wrongly". This file re-derives every field from the raw
halfwords; it shares no code with the port.

Spec used (nocash psx-spx, GPU section):
  GP0(E1) texpage : bit0-3  page X = n*64 halfwords
                    bit4    page Y = n*256 lines
                    bit5-6  semi-transparency mode
                    bit7-8  texture colour depth 0=4bpp 1=8bpp 2=15bpp
                    bit9    dither, bit10 draw-to-display, bit11 texture-disable
  GP0(E2) texwin  : bit0-4 mask X, bit5-9 mask Y, bit10-14 offset X, bit15-19 offset Y
                    (all in 8-pixel units)  ->  u = (u AND NOT(maskx*8)) OR ((offx AND maskx)*8)
  textured poly   : vertex0's high halfword = CLUT (bit0-5 X/16, bit6-14 Y)
                    vertex1's high halfword = texpage (same layout as E1 low halfword)
                    vertexN low halfword    = V<<8 | U

NEGATIVE OUTPUT IS EXPLICIT: if the file is missing, this exits non-zero saying it decoded
NOTHING. If zero textured polys are found, it says so with the denominator (how many polys of
each op it did see), because "no textured prims" and "the decoder never ran" must not look alike.
"""
import sys, os, struct, collections

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
    if 0x40 <= op <= 0x5F: return 4 if (op & 0x10) else 3   # poly-line handled separately
    if op == 0x02: return 3
    if op == 0x80: return 4
    if op in (0xA0, 0xC0): return 3
    return 1

def main(path):
    if not os.path.exists(path):
        print(f"FATAL: {path} does not exist — decoded NOTHING (0 words read). "
              f"Re-run with PSXPORT_GP0RAW=<frame>.", file=sys.stderr)
        return 2
    data = open(path, 'rb').read()
    words = list(struct.unpack('<%dI' % (len(data) // 4), data[:len(data)//4*4]))
    print(f"read {len(words)} GP0 command words from {path}")
    if not words:
        print("FATAL: file is EMPTY — 0 words, nothing to decode.", file=sys.stderr)
        return 2

    tp = dict(x=0, y=0, semi=0, depth=0, dither=0, texdis=0)
    tw = dict(mx=0, my=0, ox=0, oy=0)
    e1_hist = collections.Counter()
    e2_hist = collections.Counter()
    op_hist = collections.Counter()
    tex_state = collections.Counter()   # (tpx,tpy,depth,clutx,cluty,mx,my,ox,oy) -> nprims
    uv_min = [999, 999]; uv_max = [-1, -1]
    poly_tex = 0; poly_untex = 0
    unknown = 0
    first_rows = []

    def decode_tp(h):
        return dict(x=(h & 0xF) * 64, y=((h >> 4) & 1) * 256, semi=(h >> 5) & 3,
                    depth=(h >> 7) & 3, dither=(h >> 9) & 1, texdis=(h >> 11) & 1)

    i = 0
    while i < len(words):
        w = words[i]; op = w >> 24
        op_hist[op] += 1
        if op == 0xE1:
            e1_hist[w & 0xFFFF] += 1
            tp = decode_tp(w & 0xFFFF)
            i += 1; continue
        if op == 0xE2:
            e2_hist[w & 0xFFFFF] += 1
            tw = dict(mx=w & 31, my=(w >> 5) & 31, ox=(w >> 10) & 31, oy=(w >> 15) & 31)
            i += 1; continue
        if 0x20 <= op <= 0x3F:
            n = poly_len(op)
            if i + n > len(words): break
            gouraud, quad, textured = op & 0x10, op & 8, op & 4
            nv = 4 if quad else 3
            if not textured:
                poly_untex += 1; i += n; continue
            poly_tex += 1
            # walk the packet exactly as the hardware does
            k = 1; clut = None; tpage = None; uvs = []
            for v in range(nv):
                if gouraud and v > 0: k += 1          # colour word
                k += 1                                 # xy word
                uvw = words[i + k]; k += 1
                uvs.append((uvw & 0xFF, (uvw >> 8) & 0xFF))
                if v == 0: clut = (uvw >> 16) & 0xFFFF
                if v == 1: tpage = (uvw >> 16) & 0xFFFF
            ptp = decode_tp(tpage)
            key = (ptp['x'], ptp['y'], ptp['depth'], (clut & 0x3F) * 16, (clut >> 6) & 0x1FF,
                   tw['mx'], tw['my'], tw['ox'], tw['oy'])
            tex_state[key] += 1
            for (u, v_) in uvs:
                uv_min[0] = min(uv_min[0], u); uv_max[0] = max(uv_max[0], u)
                uv_min[1] = min(uv_min[1], v_); uv_max[1] = max(uv_max[1], v_)
            if len(first_rows) < 12:
                first_rows.append((op, tpage, clut, uvs, dict(tw)))
            i += n; continue
        if 0x40 <= op <= 0x5F and (op & 8):
            # poly-line: variable length, terminated by 0x55555555-style word
            j = i + 1
            while j < len(words) and (words[j] & 0xF000F000) != 0x50005000: j += 1
            i = j + 1; continue
        n = cmd_len(w)
        if op == 0xA0:
            # pixel words were NOT captured by the dumper, so the header is all we see
            pass
        i += n
    print()
    print(f"polys: textured={poly_tex} untextured={poly_untex}")
    if poly_tex == 0:
        print("NEGATIVE: ZERO textured polygons decoded. Op histogram of everything seen "
              "(so this cannot be confused with 'the decoder never ran'):")
        for o, c in sorted(op_hist.items()): print(f"   op {o:02X}: {c}")
        return 1
    print()
    print("GP0(E1) texpage words issued this frame (raw halfword -> decode, count):")
    for h, c in e1_hist.most_common(20):
        d = decode_tp(h)
        print(f"   E1 {h:04X} x{c:<5} -> page=({d['x']},{d['y']}) depth={d['depth']} "
              f"({['4bpp','8bpp','15bpp','res'][d['depth']]}) semi={d['semi']} dither={d['dither']} texdis={d['texdis']}")
    print(f"   ({len(e1_hist)} distinct E1 words total)")
    print()
    print("GP0(E2) texture-window words issued this frame:")
    if not e2_hist:
        print("   NONE — the guest issued 0 E2 commands this frame, so the window is whatever it "
              "was left at (mask=0 -> no wrap, the identity window).")
    for h, c in e2_hist.most_common(20):
        print(f"   E2 {h:05X} x{c:<5} -> maskX={h & 31} maskY={(h>>5)&31} offX={(h>>10)&31} offY={(h>>15)&31} "
              f"(texels: mask={( (h&31)*8, ((h>>5)&31)*8 )} off={(((h>>10)&31)*8, ((h>>15)&31)*8)})")
    print()
    print("per-prim texture state (tpage_x, tpage_y, depth, clut_x, clut_y, twmx,twmy,twox,twoy) -> nprims:")
    for k, c in tex_state.most_common(30):
        print(f"   page=({k[0]:4d},{k[1]:3d}) depth={k[2]} clut=({k[3]:4d},{k[4]:3d}) "
              f"win(mask {k[5]},{k[6]} off {k[7]},{k[8]}) -> {c} prims")
    print(f"   ({len(tex_state)} distinct states)")
    print()
    print(f"UV range over all textured prims: u {uv_min[0]}..{uv_max[0]}   v {uv_min[1]}..{uv_max[1]}")
    print()
    print("first textured prims, raw:")
    for (op, tpage, clut, uvs, twz) in first_rows:
        d = decode_tp(tpage)
        print(f"   op={op:02X} tpage_hw={tpage:04X}->page({d['x']},{d['y']}) depth={d['depth']} "
              f"clut_hw={clut:04X}->({(clut&0x3F)*16},{(clut>>6)&0x1FF}) uv={uvs} win={twz}")
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage: gp0_decode.py scratch/logs/gp0raw_f<N>.u32", file=sys.stderr); sys.exit(2)
    sys.exit(main(sys.argv[1]))
