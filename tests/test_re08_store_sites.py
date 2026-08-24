"""Both-answer tests for tools/re08_store_sites.py (RE-08 static store-site analysis).

The instrument must PROVE it fires: a synthetic module whose bytes contain one known
`mfc2->sw` vertex pair, one known `swc2` of a screen-XY register, one known `swc2` of a
NON-screen-XY cop2 register (the SZ FIFO — deliberately NOT a tap), and one plain `lw`->`sw`
carry pair. The assertions cover both classes:

  * POSITIVE — the tap forms are found, with exact per-function counts;
  * DISCRIMINATOR — the SZ store lands in the UNTAPPED bucket, not the XY bucket (the two
    answers must differ or the counter cannot be trusted);
  * NEGATIVE DESIGN — a function with no vertex stores yields no row while still counting
    toward the denominator, and the tool REFUSES (exit 2) on a missing corpus instead of
    printing an empty table.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "external" / "psxport" / "tools" / "recomp"))

import re08_store_sites as re08


def swc2(base_reg: int, rt: int, imm: int) -> int:
    return (0x3A << 26) | (base_reg << 21) | (rt << 16) | (imm & 0xFFFF)


def mfc2(rt: int, rd: int) -> int:
    return (0x12 << 26) | (0x00 << 21) | (rt << 16) | (rd << 11)


def lw(base_reg: int, rt: int, imm: int) -> int:
    return (0x23 << 26) | (base_reg << 21) | (rt << 16) | (imm & 0xFFFF)


def sw(base_reg: int, rt: int, imm: int) -> int:
    return (0x2B << 26) | (base_reg << 21) | (rt << 16) | (imm & 0xFFFF)


def nop() -> int:
    return 0


def build_image() -> tuple[int, bytes, set[int]]:
    """BASE .. fn_a: the four store forms; gap; fn_b: arithmetic only."""
    base = 0x00100000
    fn_a = base
    body_a = [
        nop(),                      # fn_a entry padding
        mfc2(8, 15),                # mfc2  t0, SXYP(15)   -> hold site (ZPAIR[15] = 19)
        sw(4, 8, 0),                # sw    t0, 0(a0)      -> VERTEX tap
        swc2(5, 14, 0),             # swc2  SXY2(14), 0(a1)-> screen-XY store
        swc2(6, 18, 0),             # swc2  SZ2(18), 0(a2) -> UNTAPPED other-cop2 store
        lw(7, 9, 0),                # lw    t1, 0(a3)
        sw(7, 9, 4),                # sw    t1, 4(a3)      -> COPY carry pair
        nop(),
    ]
    fn_b = base + 4 * len(body_a)
    body_b = [nop(), nop(), nop()]  # no GTE stores at all
    img = b"".join(w.to_bytes(4, "little") for w in body_a + body_b)
    return base, img, {fn_a, fn_b}


def main() -> int:
    checks = 0

    # --- dispatch-table parser ------------------------------------------------------------
    text = """
    case 0x80010000u: return 0;
    case 0x0007C4D8u: return 41;
    not-a-case 0x12345678u:
    """
    funcs = re08.parse_dispatch_text(text)
    assert funcs == {0x80010000, 0x0007C4D8}, funcs
    checks += 1

    # --- synthetic-module scan: positives AND the discriminator ---------------------------
    base, img, func_addrs = build_image()

    # scan_module(name, base, img, funcs) — call through the real path.
    res, rows = re08.scan_module("SYNTH", base, img, func_addrs)
    checks += 1
    assert res["funcs"] == 2, res  # denominator counts BOTH functions…
    assert len(rows) == 1, rows    # …but only the one WITH stores gets a row
    row = rows[0]
    assert row["fn"] == base & 0x1FFFFFFF, row  # link-space address of fn_a
    assert row["module"] == "SYNTH", row
    assert row["swc2_xy"] == 1, row        # the SXY2 swc2 IS a tap
    assert row["swc2_other"] == 1, row     # the SZ2 swc2 is COUNTED, not tapped, not dropped
    assert row["mfc2_vertex"] == 1, row    # the sw of t0 carries the held Z
    assert row["mfc2_hold"] == 1, row      # exactly one hold (at the mfc2)
    assert row["copy_carry"] == 1, row     # the plain lw->sw pair
    checks += 4

    # The discriminator, restated against the emitted substrate form: an XY-reg swc2 maps to
    # gte_store_xy; an SZ-reg swc2 must map to the PLAIN store. Re-checked here against the
    # same constants the emitter uses, so a constant drift breaks this test rather than the
    # analysis silently diverging from the emitter.
    assert re08.XY_REGS == frozenset({12, 13, 14, 15}) or set(re08.XY_REGS) == {
        12,
        13,
        14,
        15,
    }, re08.XY_REGS
    assert 18 not in set(re08.XY_REGS)
    checks += 2

    # --- refusal on missing corpus (the negative design) ----------------------------------
    env = dict(os.environ)
    env["RE08_EXE"] = str(ROOT / "scratch/logs/definitely-not-here.exe")
    proc = subprocess.run(
        [sys.executable, str(ROOT / "tools/re08_store_sites.py")],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert proc.returncode == 2, (proc.returncode, proc.stdout, proc.stderr)
    assert "REFUSED" in proc.stderr, proc.stderr
    checks += 2

    print(f"re08-store-sites selftest: {checks} check(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
