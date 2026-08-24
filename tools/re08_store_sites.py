#!/usr/bin/env python3
"""re08_store_sites.py — WHERE does the guest store projected screen-XY vertices, and which of those
stores carry the recompiler's native-depth tap? (RE-08 diagnostic.)

The recompiler (external/psxport/tools/recomp/emit.py) taps two forms at RECOMP TIME and prints only
a TOTAL ("native-depth: N mfc2->sw vertex store(s) tapped"). At RUN time "0 records" and "this game
has no 3D" are the same observation, so this tool answers the static half with denominators:

  * every `swc2` site whose rt is a GTE SCREEN-XY register (DR12/13/14/15) — each becomes one
    gte_store_xy() call in the substrate;
  * every `swc2` of a NON-screen-XY cop2 register (e.g. the SZ FIFO) — kept untapped, counted, so a
    game that stores depth directly is visible instead of silently uncovered;
  * the mfc2->sw vertex pairs and lw->sw depth carries from emit.vertex_pz_stores() — the EXACT
    analysis the emitter runs, imported rather than re-derived;
  * stores skipped because they sit in a branch delay slot (the emitter counts but cannot place
    them) — listed, never dropped quietly.

NEGATIVE DESIGN. This tool REFUSES instead of printing an empty table when its corpus is missing,
and it prints what it walked on every run: modules scanned, functions parsed out of the dispatch
tables, instructions decoded, bytes inside function bodies that did NOT decode as code. A function
list of zero, or a module image of zero bytes, is an error, not an answer.

Corpus = the SAME images the substrate was built from: the boot executable's text segment plus the
30 runtime-loaded code modules under scratch/overlays/spiderman1/ at their link base. Function boundaries come
from the generated dispatch tables (generated/shard_disp.c + generated/ov_*_disp.c), i.e. from what
was actually compiled — not from jal-target guessing.

Usage:
    python3 tools/re08_store_sites.py                 # summary + per-function table on stdout
    python3 tools/re08_store_sites.py --csv scratch/logs/re08_sites.csv

Exit: 0 = corpus complete; 2 = REFUSED (missing inputs).
"""

import argparse
import glob
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PSXPORT_RECOMP = os.path.join(ROOT, "external", "psxport", "tools", "recomp")
sys.path.insert(0, PSXPORT_RECOMP)

import decode as D  # framework recompiler decoder
import emit  # framework recompiler analysis (vertex_pz_stores)
from decode import decode

EXE_DEFAULT = os.path.join(ROOT, "scratch/bin/spiderman/SLUS_008.75")
OVERLAYS_DIR = os.path.join(ROOT, "scratch/overlays/spiderman1")
GENERATED_DIR = os.path.join(ROOT, "generated")

XY_REGS = set(emit.GTE_SCREEN_XY_REGS)


def die(msg):
    print(f"re08-store-sites: REFUSED — {msg}", file=sys.stderr)
    sys.exit(2)


def parse_dispatch_text(text):
    """Function starts from dispatch-TU TEXT: `case 0x%08Xu:` in the address->fn switch."""
    funcs = set()
    for line in text.splitlines():
        m = re.match(r"\s*case 0x([0-9A-F]{8})u:", line)
        if m:
            funcs.add(int(m.group(1), 16))
    return funcs


def parse_dispatch_funcs(path):
    return parse_dispatch_text(Path(path).read_text(encoding="utf-8"))


def load_modules():
    """[(name, base, bytes, funcs)] for MAIN + every overlay module."""
    exe_path = os.environ.get("RE08_EXE", EXE_DEFAULT)
    if not os.path.isfile(exe_path):
        die(
            f"{exe_path} not found — run tools/redump_ram.py's source step first "
            f"(tools/ensure_recomp.py extracts it)"
        )
    data = Path(exe_path).read_bytes()
    if data[:8] != b"PS-X EXE":
        die(f"{exe_path} is not a PS-X EXE")
    _pc, _gp, load, size = struct.unpack_from("<4I", data, 0x10)
    main_funcs = parse_dispatch_funcs(os.path.join(GENERATED_DIR, "shard_disp.c"))
    if not main_funcs:
        die("generated/shard_disp.c yielded 0 function addresses — wrong corpus?")
    mods = [("MAIN", load, data[0x800 : 0x800 + size], main_funcs)]

    disp_files = sorted(glob.glob(os.path.join(GENERATED_DIR, "ov_*_disp.c")))
    bins = {
        os.path.splitext(os.path.basename(p))[0]: p
        for p in glob.glob(os.path.join(OVERLAYS_DIR, "*.bin"))
    }
    if len(bins) != len(disp_files):
        die(
            f"{len(bins)} overlay image(s) under scratch/overlays/spiderman1 but "
            f"{len(disp_files)} ov_*_disp.c dispatch tables — corpora disagree"
        )
    for path in disp_files:
        name = re.match(r".*ov_(.+)_disp\.c$", path).group(1)
        bin_path = bins[name]
        img = Path(bin_path).read_bytes()
        reloc = os.path.splitext(bin_path)[0] + ".reloc.json"
        base = json.loads(Path(reloc).read_text(encoding="utf-8"))["link_base"]
        funcs = parse_dispatch_funcs(path)
        if not funcs:
            die(f"{path} yielded 0 function addresses")
        mods.append((name.upper(), base, img, funcs))
    return mods


def scan_module(name, base, img, funcs):
    """One module: decode every function body and classify the vertex-store sites."""
    # The dispatch tables print LINK-SPACE addresses (masked 0x1FFFFFFF); images are keyed the
    # same way so MAIN (0x8001xxxx) and overlays (0x800C65EC) share one comparison.
    img_base = base & 0x1FFFFFFF
    ordered = sorted(
        {f & 0x1FFFFFFF for f in funcs if img_base <= f < img_base + len(img)}
    )
    collapsed = len(funcs) - len(ordered)
    if collapsed:
        print(
            f"// [{name}] {collapsed} dispatch address(es) duplicated or outside the image — "
            "collapsed/excluded here, not silently"
        )
    nxt = {
        f: (ordered[i + 1] if i + 1 < len(ordered) else img_base + len(img))
        for i, f in enumerate(ordered)
    }
    res = {"funcs": 0, "words": 0, "undecoded": 0, "skipped_ds": 0}
    rows = []
    for fn in ordered:
        lo, hi = fn, nxt[fn]
        ins = {}
        for a in range(lo, hi, 4):
            word = struct.unpack_from("<I", img, a - img_base)[0]
            i = decode(a, word)
            ins[a] = i
            res["words"] += 1
            if i.kind == D.UNKNOWN:
                res["undecoded"] += 1
        mfc2_holds, vertex_stores, _src_holds, copy_stores, skipped = (
            emit.vertex_pz_stores(ins, lo, hi)
        )
        n_xy = n_other = 0
        for i in ins.values():
            if i.kind == D.GTE_STORE:  # swc2
                if i.rt in XY_REGS:
                    n_xy += 1
                else:
                    n_other += 1
        res["funcs"] += 1
        res["skipped_ds"] += skipped
        if n_xy or vertex_stores or n_other:
            rows.append(
                {
                    "fn": fn,
                    "module": name,
                    "swc2_xy": n_xy,
                    "swc2_other": n_other,
                    "mfc2_vertex": len(vertex_stores),
                    "mfc2_hold": len(mfc2_holds),
                    "copy_carry": len(copy_stores),
                    "skip_delay_slot": skipped,
                }
            )
    return res, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--csv", help="also write the full per-function table here")
    args = ap.parse_args()

    mods = load_modules()
    print(
        f"// corpus: {len(mods)} module(s) "
        f"({', '.join(n for n, *_ in mods[:3])}"
        f"{', …' if len(mods) > 3 else ''})"
    )

    all_rows = []
    tot = {
        "funcs": 0,
        "words": 0,
        "undecoded": 0,
        "xy": 0,
        "other_swc2": 0,
        "mfc2_vertex": 0,
        "mfc2_hold": 0,
        "copy_carry": 0,
        "skip": 0,
    }
    for name, base, img, funcs in mods:
        res, rows = scan_module(name, base, img, funcs)
        all_rows.extend(rows)
        n_xy = sum(r["swc2_xy"] for r in rows if r["module"] == name)
        n_oth = sum(r["swc2_other"] for r in rows if r["module"] == name)
        n_mv = sum(r["mfc2_vertex"] for r in rows if r["module"] == name)
        n_mh = sum(r["mfc2_hold"] for r in rows if r["module"] == name)
        n_cc = sum(r["copy_carry"] for r in rows if r["module"] == name)
        n_sk = sum(r["skip_delay_slot"] for r in rows if r["module"] == name)
        print(
            f"// [{name}] functions={res['funcs']} words_decoded={res['words']} "
            f"undecoded={res['undecoded']} | swc2_screenXY={n_xy} swc2_otherCop2={n_oth} "
            f"| mfc2->sw_vertex_taps={n_mv} (holds {n_mh}) copy_carries={n_cc} "
            f"delay-slot_skipped={n_sk}"
        )
        tot["funcs"] += res["funcs"]
        tot["words"] += res["words"]
        tot["undecoded"] += res["undecoded"]
        tot["xy"] += n_xy
        tot["other_swc2"] += n_oth
        tot["mfc2_vertex"] += n_mv
        tot["mfc2_hold"] += n_mh
        tot["copy_carry"] += n_cc
        tot["skip"] += n_sk

    print(
        f"// TOTAL: functions={tot['funcs']} words={tot['words']} undecoded={tot['undecoded']}"
    )
    print(
        f"// TOTAL: swc2 screen-XY stores (gte_store_xy sites) = {tot['xy']}; "
        f"swc2 other-cop2 stores (UNTAPPED) = {tot['other_swc2']}"
    )
    print(
        f"// TOTAL: mfc2->sw vertex taps (hold+record pairs) = {tot['mfc2_vertex']} "
        f"across {tot['mfc2_hold']} hold site(s); copy carries = {tot['copy_carry']}; "
        f"delay-slot skips = {tot['skip']}"
    )
    if tot["xy"] == 0 and tot["mfc2_vertex"] == 0:
        print(
            "// NEGATIVE RESULT IS REAL ONLY AGAINST THIS DENOMINATOR: "
            f"{tot['funcs']} function(s), {tot['words']} instruction(s) decoded. "
            "If that denominator is small, the corpus — not the game — is the problem."
        )

    top = sorted(
        all_rows,
        key=lambda r: -(r["swc2_xy"] * 2 + r["mfc2_vertex"] * 3 + r["copy_carry"]),
    )[:20]
    print(
        "// TOP function(s) by vertex-store activity "
        "(weight: swc2_xy×2 + mfc2_vertex×3 + copy_carry):"
    )
    for r in top:
        print(
            "  0x{:08X} {:<9} swc2_xy={} mfc2_vertex={} copy_carry={}".format(
                r["fn"], r["module"], r["swc2_xy"], r["mfc2_vertex"], r["copy_carry"]
            )
        )

    if args.csv:
        with open(args.csv, "w") as f:
            f.write(
                "function,module,swc2_xy,swc2_other,mfc2_vertex,mfc2_hold,copy_carry,"
                "skip_delay_slot\n"
            )
            for r in sorted(all_rows, key=lambda r: (r["module"], r["fn"])):
                f.write(
                    "0x{fn:08X},{module},{swc2_xy},{swc2_other},{mfc2_vertex},"
                    "{mfc2_hold},{copy_carry},{skip_delay_slot}\n".format(**r)
                )
        print(f"// per-function table -> {args.csv}")


if __name__ == "__main__":
    main()
