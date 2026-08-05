#!/usr/bin/env python3
"""prim_texel_probe.py — decide, on the CPU, what each SUBMITTED prim WOULD sample.

Issue 0007 is stuck on a three-way fork: are the prims' (A) tpage/CLUT coords wrong,
(B) is the texture window applied wrong, or (C) is the sampling/CLUT decode in the
shader wrong? This tool removes (A) and (B) from the fork by REPRODUCING the shader's
addressing arithmetic (tritex.frag: texture-window mask, 4bpp/8bpp CLUT lookup) on the
CPU, against a real VRAM dump and the real per-prim state captured by PSXPORT_PRIMDUMP.

If a prim's own tp/clut/uv/window resolve to MANY distinct texels on the CPU, then the
addressing the prim carries is fine and the picture being flat is downstream of it.
If they resolve to ONE texel, the addressing (or the region it points at) is the cause,
and the with-window vs without-window columns say which.

    python3 tools/prim_texel_probe.py <vram.bin> <prims_fN.csv>

DESIGNING THE NEGATIVE (per this project's rules): every run prints its denominators —
rows read, textured rows, rows probed, rows skipped and why — and refuses (exit 2) if
either input is missing or if it probes zero rows. "0 flat prims" is only meaningful
next to "out of N probed".
"""
import sys, os, csv, collections

VRAM_W, VRAM_H = 1024, 512

def die(msg):
    sys.stderr.write("prim_texel_probe: %s\n" % msg)
    sys.exit(2)

def main():
    if len(sys.argv) != 3:
        die("usage: prim_texel_probe.py <vram.bin> <prims_fN.csv>")
    vpath, cpath = sys.argv[1], sys.argv[2]
    for p in (vpath, cpath):
        if not os.path.exists(p):
            die("input does not exist: %s (searched NOTHING)" % p)
    raw = open(vpath, 'rb').read()
    want = VRAM_W * VRAM_H * 2
    if len(raw) != want:
        die("%s is %d bytes, expected %d (1024x512 u16)" % (vpath, len(raw), want))
    import array
    vram = array.array('H'); vram.frombytes(raw)
    if sys.byteorder != 'little': vram.byteswap()

    def at(x, y):
        return vram[(y & 511) * VRAM_W + (x & 1023)]

    rows = list(csv.DictReader(open(cpath)))
    n_rows = len(rows)
    n_tex = n_probe = 0
    skip_reason = collections.Counter()
    # per-prim distinct-texel counts, with and without the texture window
    hist_win = collections.Counter()
    hist_nowin = collections.Counter()
    flat_examples = []
    per_page = collections.defaultdict(lambda: [0, 0])   # (tp,clut,mode) -> [prims, sum distinct]

    for r in rows:
        try:
            tex = int(r['tex']); mode = int(r['mode'])
        except (KeyError, ValueError):
            skip_reason['unparsable row'] += 1
            continue
        if not tex:
            skip_reason['untextured (tex=0)'] += 1
            continue
        n_tex += 1
        if mode == 2:
            skip_reason['15bpp direct (mode=2) — not CLUT, still probed below'] += 0
        tpx, tpy = int(r['tpx']), int(r['tpy'])
        cx, cy = int(r['clutx']), int(r['cluty'])
        twmx, twmy, twox, twoy = int(r['twmx']), int(r['twmy']), int(r['twox']), int(r['twoy'])
        umin, umax = int(r['umin']), int(r['umax'])
        vmin, vmax = int(r['vmin']), int(r['vmax'])
        n_probe += 1

        def texel(u, v, use_window):
            if use_window:
                u = (u & ~(twmx * 8)) | ((twox & twmx) * 8)
                v = (v & ~(twmy * 8)) | ((twoy & twmy) * 8)
            if mode == 0:
                w = at(tpx + (u >> 2), tpy + v)
                return at(cx + ((w >> ((u & 3) * 4)) & 0xF), cy)
            if mode == 1:
                w = at(tpx + (u >> 1), tpy + v)
                return at(cx + ((w >> ((u & 1) * 8)) & 0xFF), cy)
            return at(tpx + u, tpy + v)

        # sample the prim's uv bounding box on a <=16x16 grid (a proxy for the covered
        # texel set: a superset in u/v, which can only OVER-state detail, never hide it)
        us = sorted(set([umin + (umax - umin) * i // 15 for i in range(16)]))
        vs = sorted(set([vmin + (vmax - vmin) * i // 15 for i in range(16)]))
        sw, snw = set(), set()
        for v in vs:
            for u in us:
                sw.add(texel(u, v, True))
                snw.add(texel(u, v, False))
        hist_win[len(sw)] += 1
        hist_nowin[len(snw)] += 1
        k = (tpx, tpy, cx, cy, mode)
        per_page[k][0] += 1; per_page[k][1] += len(sw)
        if len(sw) <= 2 and len(flat_examples) < 12:
            flat_examples.append((r['id'], tpx, tpy, cx, cy, mode,
                                  (twmx, twmy, twox, twoy), (umin, umax, vmin, vmax),
                                  len(sw), len(snw), sorted(sw)))

    print("INPUTS      vram=%s  prims=%s" % (vpath, cpath))
    print("DENOMINATOR rows=%d  textured=%d  probed=%d" % (n_rows, n_tex, n_probe))
    for k, v in skip_reason.items():
        if v: print("  skipped %-40s %d" % (k, v))
    if n_probe == 0:
        die("probed ZERO prims — this run measured nothing (check the CSV has tex/mode/tpx columns)")

    def summarise(name, hist):
        tot = sum(hist.values())
        flat = sum(c for k, c in hist.items() if k <= 2)
        rich = sum(c for k, c in hist.items() if k >= 16)
        vals = sorted(hist.items())
        med = None
        acc = 0
        for k, c in vals:
            acc += c
            if acc * 2 >= tot: med = k; break
        print("%-12s prims=%d  flat(<=2 distinct texels)=%d (%.1f%%)  rich(>=16)=%d (%.1f%%)  median=%s  max=%d"
              % (name, tot, flat, 100.0*flat/tot, rich, 100.0*rich/tot, med, max(hist)))

    print()
    print("DISTINCT TEXELS A PRIM WOULD SAMPLE (256 grid points over its own uv bbox):")
    summarise("with window", hist_win)
    summarise("no window", hist_nowin)
    print()
    print("If 'with window' is flat and 'no window' is rich -> the texture WINDOW collapses the fetch (B).")
    print("If BOTH are flat                                  -> the page/CLUT the prim carries is flat (A).")
    print("If BOTH are rich                                  -> addressing is FINE on the CPU; look downstream (C).")
    if flat_examples:
        print()
        print("FLAT PRIM EXAMPLES (<=2 distinct texels with window):")
        for e in flat_examples:
            print("  id=%s tp=(%d,%d) clut=(%d,%d) mode=%d tw=%s uv=%s  win=%d nowin=%d texels=%s"
                  % (e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9],
                     ["%04X" % t for t in e[10][:4]]))
    else:
        print()
        print("FLAT PRIM EXAMPLES: none — every one of the %d probed prims resolved >2 distinct texels." % n_probe)
    print()
    print("PER (tpage,clut,mode) — worst 10 by mean distinct texels:")
    ranked = sorted(per_page.items(), key=lambda kv: kv[1][1]/kv[1][0])
    for k, (n, s) in ranked[:10]:
        print("  tp=(%d,%d) clut=(%d,%d) mode=%d  prims=%-4d mean_distinct=%.1f" % (k[0], k[1], k[2], k[3], k[4], n, s/n))
    print("BEST 5:")
    for k, (n, s) in ranked[-5:]:
        print("  tp=(%d,%d) clut=(%d,%d) mode=%d  prims=%-4d mean_distinct=%.1f" % (k[0], k[1], k[2], k[3], k[4], n, s/n))
    print()
    print("BLIND SPOTS: (1) the uv BBOX is sampled, not the exact rasterised texel set — it can only")
    print("over-state detail, so a 'rich' verdict here does NOT prove every covered texel differs;")
    print("(2) the VRAM dump is a CPU-side s_vram snapshot at ITS OWN frame, so a page rewritten")
    print("between that frame and the prim frame would be misread; (3) this reproduces the SHADER's")
    print("arithmetic, so a defect in the shader itself is by construction invisible to it.")

main()
