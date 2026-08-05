#!/usr/bin/env python3
"""Cross-reference a PSXPORT_PRIMDUMP CSV against a PSXPORT_VRAMDUMP raw VRAM image.

Answers, with denominators, the question issue 0007 is stuck on: do the textured primitives the
guest submits POINT AT the rich texture atlas, or somewhere else?

For every distinct (texpage x, texpage y, CLUT x, CLUT y, colour mode) tuple it reports
  * how many prims carry it,
  * how RICH the texels those prims actually address are (distinct 16-bit values in the exact
    u/v sub-rectangle they sample, not a 64x64 tile approximation),
  * how RICH the CLUT they decode through is (distinct entries among the 16 / 256 at clut x,y).

A prim whose TEXELS are rich but whose CLUT is uniform renders as one flat colour modulated by the
vertex colour — visually identical to "untextured gouraud", which is the symptom under investigation.
So both halves are measured; reporting only one of them could not tell those cases apart.

NEGATIVE OUTPUT IS EXPLICIT: if the CSV holds no textured rows at all, or the VRAM image is the
wrong size, this exits NON-ZERO saying what it scanned. "(nothing)" with exit 0 is never printed.

    python3 tools/prim_texref.py scratch/logs/prims_f11900.csv scratch/raw/vram_coordlens.bin
"""
import csv
import os
import sys
from collections import Counter, defaultdict

VRAM_W, VRAM_H = 1024, 512
MODE_NAME = {0: "4bpp-CLUT", 1: "8bpp-CLUT", 2: "15bpp-direct", 3: "untextured"}


def load_vram(path):
    raw = open(path, "rb").read()
    want = VRAM_W * VRAM_H * 2
    if len(raw) != want:
        sys.exit("FATAL: %s is %d bytes, expected %d (1024x512 u16). Scanned NOTHING."
                 % (path, len(raw), want))
    import array
    a = array.array("H")
    a.frombytes(raw)
    if sys.byteorder == "big":
        a.byteswap()
    return a


def px(v, x, y):
    return v[(y & (VRAM_H - 1)) * VRAM_W + (x & (VRAM_W - 1))]


def texel_word_x(mode, tpx, u):
    """VRAM word column holding texel u of a page based at tpx, for the given colour mode."""
    if mode == 0:
        return tpx + (u >> 2)
    if mode == 1:
        return tpx + (u >> 1)
    return tpx + u


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    csv_path, vram_path = sys.argv[1], sys.argv[2]
    if not os.path.exists(csv_path):
        sys.exit("FATAL: %s does not exist. Scanned NOTHING." % csv_path)
    v = load_vram(vram_path)

    rows = list(csv.DictReader(open(csv_path)))
    if not rows:
        sys.exit("FATAL: %s has no data rows. Scanned NOTHING." % csv_path)

    n_all = len(rows)
    n_tex = 0
    # tuple -> (count, set of (word_x, y) texel words touched, u range, v range, semi count, raw count)
    agg = defaultdict(lambda: {"n": 0, "words": set(), "umin": 999, "umax": -999,
                               "vmin": 999, "vmax": -999, "semi": 0, "raw": 0,
                               "is3d": Counter(), "kind": Counter(), "op": Counter(),
                               "tw": Counter()})
    degenerate_uv = 0
    for r in rows:
        if int(r["tex"]) != 1:
            continue
        n_tex += 1
        mode = int(r["mode"])
        tpx, tpy = int(r["tpx"]), int(r["tpy"])
        cx, cy = int(r["clutx"]), int(r["cluty"])
        key = (tpx, tpy, cx, cy, mode)
        a = agg[key]
        a["n"] += 1
        a["semi"] += int(r["semi"])
        a["raw"] += int(r["raw"])
        a["is3d"][r["is3d"]] += 1
        a["kind"][r["kind"]] += 1
        a["op"][r["op"]] += 1
        a["tw"][(r["twmx"], r["twmy"], r["twox"], r["twoy"])] += 1
        umin, umax = int(r["umin"]), int(r["umax"])
        vmin, vmax = int(r["vmin"]), int(r["vmax"])
        if umin == umax and vmin == vmax:
            degenerate_uv += 1
        a["umin"] = min(a["umin"], umin); a["umax"] = max(a["umax"], umax)
        a["vmin"] = min(a["vmin"], vmin); a["vmax"] = max(a["vmax"], vmax)
        # exact texel words this prim's uv box addresses (cap the box so a pathological prim
        # cannot blow up; 256x256 is a whole page, the maximum a PSX prim can reach anyway)
        for vv in range(vmin, min(vmax, vmin + 255) + 1):
            for uu in range(umin, min(umax, umin + 255) + 1):
                a["words"].add((texel_word_x(mode, tpx, uu), tpy + vv))

    print("SCANNED: %s — %d prim rows, %d textured (tex=1), %d untextured/other"
          % (csv_path, n_all, n_tex, n_all - n_tex))
    print("VRAM   : %s (1024x512x16)" % vram_path)
    if n_tex == 0:
        sys.exit("\nFATAL: zero textured rows in %d scanned. Nothing to cross-reference — this is a\n"
                 "measurement of an EMPTY set, not a finding about texture addressing." % n_all)
    print("degenerate UV (umin==umax AND vmin==vmax): %d of %d textured prims" % (degenerate_uv, n_tex))
    print()

    hdr = ("%-6s %-6s %-6s %-6s %-12s %6s  %-13s %-11s  %-9s %-9s"
           % ("tpx", "tpy", "clutx", "cluty", "mode", "prims", "texel-distinct", "clut-distinct",
              "u-range", "v-range"))
    print(hdr)
    print("-" * len(hdr))

    tot_prims_rich_tex = tot_prims_rich_clut = 0
    tot_prims_tp_outside = 0
    for key, a in sorted(agg.items(), key=lambda kv: -kv[1]["n"]):
        tpx, tpy, cx, cy, mode = key
        vals = {px(v, x, y) for (x, y) in a["words"]}
        tex_distinct = len(vals)
        if mode == 0:
            clut_vals = {px(v, cx + i, cy) for i in range(16)}
        elif mode == 1:
            clut_vals = {px(v, cx + i, cy) for i in range(256)}
        else:
            clut_vals = set()  # 15bpp direct: no CLUT
        clut_distinct = len(clut_vals)
        if tex_distinct > 16:
            tot_prims_rich_tex += a["n"]
        if mode == 2 or clut_distinct > 2:
            tot_prims_rich_clut += a["n"]
        if tpx < 512:
            tot_prims_tp_outside += a["n"]
        print("%-6d %-6d %-6d %-6d %-12s %6d  %-13d %-11s  %-9s %-9s"
              % (tpx, tpy, cx, cy, MODE_NAME.get(mode, str(mode)), a["n"],
                 tex_distinct, ("n/a" if mode == 2 else str(clut_distinct)),
                 "%d..%d" % (a["umin"], a["umax"]), "%d..%d" % (a["vmin"], a["vmax"])))

    print()
    print("TOTALS over %d textured prims / %d distinct (tp,clut,mode) tuples:" % (n_tex, len(agg)))
    print("  texpage base x <512 (outside the atlas half): %d prims" % tot_prims_tp_outside)
    print("  addressed texels are RICH (>16 distinct words): %d prims" % tot_prims_rich_tex)
    print("  CLUT is NON-FLAT (>2 distinct entries, or 15bpp): %d prims" % tot_prims_rich_clut)
    print("  => prims that sample rich texels through a FLAT CLUT: %d"
          % (tot_prims_rich_tex - min(tot_prims_rich_tex, tot_prims_rich_clut)
             if tot_prims_rich_tex >= tot_prims_rich_clut else 0))
    print()
    print("BLIND SPOTS of this tool, stated so a clean result is not over-read:")
    print("  * the VRAM image is ONE frame; a prim from a different frame in the CSV is checked")
    print("    against that snapshot, so a mid-window atlas re-upload would be invisible here.")
    print("  * it reads the CPU-side s_vram the guest wrote. It says nothing about whether the GPU")
    print("    sampler/shader then decodes those texels correctly (issue 0007 candidate C).")
    print("  * the texture WINDOW is reported per tuple but NOT applied to the uv box, so a prim")
    print("    whose window remaps u/v is scored on its pre-window coordinates.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
