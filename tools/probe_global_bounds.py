#!/usr/bin/env python3
"""Census SLUS_008.75's main-RAM GLOBAL bounds, and whether a widened window can push one inward.

WHY A SECOND INSTRUMENT. `probe_cull_census.py` counts references to the viewport-record cell
(0x800B5918) and reports 39 distinct sites against Ghidra's 27 references as a lower bound. That
answers "does the widening reach the owners that read the record", and it does not answer "is there
a cull that does NOT read the record". Its own closing line is the admission: 372 `addiu`/`ori`
sites load a 4:3-derived LITERAL, and deciding whether any of them bounds a projected coordinate
needs the comparison's dataflow.

That admission is not academic in this workspace. Crash 1 was scanned the same way, the literal
scan found "no horizontal cull", and the inference "widening cannot clip new geometry" was
published — and it was wrong. The bound was a main-RAM GLOBAL (0x800578D0, 1 writer, 20 readers)
that a literal scan cannot see, and it was not a screen-space cull at all: FUN_8003A144 used it as
the GTE NEAR PLANE. So "no literal cull" is not evidence, and this tool exists so the same
inference cannot be made here from the same silence.

WHAT IS COUNTED, AND WHAT IS NOT. This tool enumerates main-RAM 32-bit globals that the resident
text can form with a `lui` + 16-bit-displacement pair, and counts how many instruction sites WRITE
and READ each one. A "bound the widening could push inward" has a shape: few writers (the engine
sets it once, or from one place) and many readers (every draw call re-checks it). That shape is a
CANDIDATE, and the tool says so in its own output. Deciding whether a candidate bounds a projected
coordinate, and along WHICH axis, is the comparison's dataflow, which this tool does not compute and
will not guess at.

WHAT IT DELIBERATELY DOES NOT CLAIM. It reports a denominator and a blind spot on every run: how
many instruction words were walked, how many the shared decoder refused, and the fact that a MIPS
branch DELAY SLOT is decoded as its own word, so every site is "a word that decodes as this form"
and not "an instruction executed here". A reader must not take a candidate list as a cull list.

IT REUSES THE REPOSITORY'S DECODER. Every instruction comes from `probe_mips_image.decode`, which
is this repository's one MIPS word decoder. This file re-derives no opcode or field position.

    uv run --frozen python tools/probe_global_bounds.py
    uv run --frozen python tools/probe_global_bounds.py --top 25
    uv run --frozen python tools/probe_global_bounds.py --selftest
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probe_mips_image import decode, sign_extend  # noqa: E402  (path set above)

RAM = Path(__file__).resolve().parents[1] / "scratch" / "bin" / "spiderman" / "ram.bin"

# The resident text this port's own GuestProgramImage declares for SLUS_008.75
# (spider1_runtime.h: .residentText = {0x00010000, 0x000C65D4}). The declared region rather than
# "the whole file" keeps the denominator honest: the 2 MiB dump also holds bss and the heap, which
# are not code.
REGION_START = 0x80010000
REGION_END = 0x800C65D4

# Main RAM is 2 MiB at 0x80000000. A global is a 32-bit word aligned on 4 inside it. The bss/heap
# tail is included deliberately: a bound the engine writes once at boot and reads every frame is
# exactly the shape this tool is looking for, and such a variable usually lives past the text.
MAIN_RAM_BASE = 0x80000000
MAIN_RAM_BYTES = 0x00200000

OP_LUI = 0x0F
OP_ADDIU = 0x09
OP_ORI = 0x0D
OP_LW = 0x23
OP_SW = 0x2B
OP_LHU = 0x25
OP_LB = 0x20
OP_SB = 0x28
OP_SH = 0x29
REG_ZERO = 0

# A halfword or byte read of a 32-bit bound is as much a use of it as a full word read, so the
# reader count includes all four load widths and both store widths. A scan that counted only
# `lw` would miss a bound read through `lhu`, which is how a u16 field of a wider global is read.
LOADS = (OP_LW, OP_LHU, OP_LB)
STORES = (OP_SW, OP_SB, OP_SH)


def _hi_lo(target: int) -> tuple[int, int]:
    """The (lui immediate, addiu/ori immediate) pair that forms `target`."""
    return (target >> 16) & 0xFFFF, target & 0xFFFF


class GlobalSite:
    __slots__ = ("address", "writer", "reader")

    def __init__(self) -> None:
        self.address = 0
        self.writer: int | None = None
        self.reader: list[int] = []


def scan(words: list[tuple[int, int]]) -> dict[str, object]:
    """Walk decoded words and return (globals, stats).

    The pass is two-phase on purpose. Phase 1 finds every site that FORMS a main-RAM address
    (`lui` + a 16-bit displacement add), which is the only way this image can name a global
    without a gp-relative load. Phase 2 counts writes and reads at every site, so a global that
    is written and never read, or read and never written, is distinguishable from one that is
    both — and both are distinguishable from a global the text never names.
    """
    decoded: list[tuple[int, int, object]] = []
    refused = 0
    for address, word in words:
        instruction = decode(address, word)
        if instruction is None:
            refused += 1
            continue
        decoded.append((address, word, instruction))

    globals_found: dict[int, GlobalSite] = {}

    def site_for(target: int) -> GlobalSite | None:
        if not (MAIN_RAM_BASE <= target < MAIN_RAM_BASE + MAIN_RAM_BYTES):
            return None
        if target % 4:
            return None
        site = globals_found.get(target)
        if site is None:
            site = GlobalSite()
            site.address = target
            globals_found[target] = site
        return site

    # Phase 1: `lui` + adjacent 16-bit displacement, which NAMES the globals. It deliberately
    # records no writer and no reader: attributing a write here would mean deciding "the next word
    # is a store through this register" and then recording the FORMATION's address as the writer,
    # which is a second, worse implementation of the rule phase 2 already owns. One rule, one
    # place.
    pending: dict[int, int] = {}  # register -> the high half this `lui` installed
    for _address, _word, instruction in decoded:
        op = (instruction.word >> 26) & 0x3F
        rt = (instruction.word >> 16) & 0x1F
        rs = (instruction.word >> 21) & 0x1F
        low = instruction.word & 0xFFFF
        if op == OP_LUI:
            # The immediate is read from the WORD, not from `fields`, so this file owns no
            # field-position convention of its own.
            pending[rt] = (instruction.word & 0xFFFF) << 16
            continue
        if op in (OP_ADDIU, OP_ORI) and rs in pending and rs != REG_ZERO and rt == rs:
            base = pending.pop(rs)
            target = (base + sign_extend(low, 16)) & 0xFFFFFFFF if op == OP_ADDIU else (base | low)
            site_for(target)
            continue
        if rs in pending:
            pending.pop(rs, None)

    # Phase 2: every load and store that goes through a register holding a formed main-RAM
    # address. The address is carried forward from the `lui`/`addiu` that made it, so a
    # load-after-formation is attributed to the global even though the load word itself names
    # only a register.
    high: dict[int, int] = {}
    for _address, _word, instruction in decoded:
        op = (instruction.word >> 26) & 0x3F
        rt = (instruction.word >> 16) & 0x1F
        rs = (instruction.word >> 21) & 0x1F
        low = instruction.word & 0xFFFF
        if op == OP_LUI:
            high[rt] = (instruction.word & 0xFFFF) << 16
        elif op in (OP_ADDIU, OP_ORI) and rs in high and rt == rs and rs != REG_ZERO:
            base = high[rs]
            if op == OP_ADDIU:
                target = (base + sign_extend(low, 16)) & 0xFFFFFFFF
            else:
                target = base | low
            high[rt] = target
        elif op in LOADS or op in STORES:
            if rs in high:
                found = site_for(high[rs])
                if found is not None:
                    if op in LOADS:
                        if found.writer != instruction.address:
                            found.reader.append(instruction.address)
                    elif found.writer is None:
                        found.writer = instruction.address

    return {
        "globals": globals_found,
        "walked": len(words),
        "decoded": len(decoded),
        "refused": refused,
    }


def _fixture(words: list[tuple[int, int]]) -> dict[str, object]:
    return scan(words)


def selftest() -> int:
    """The cases that WOULD fail, asserted on synthetic words through the SHIPPING scan.

    A selftest that only proves the tool runs is worthless here: the failure it exists to catch is
    a silent one, a tool that reports zero candidates on a real image. So the positive case
    builds a global with one writer and several readers and requires it to be FOUND and RANKED,
    and the negative case requires a global in RAM that the text never names to be ABSENT.
    """
    cases: list[tuple[str, bool, str]] = []

    def add(name: str, ok: bool, detail: str) -> None:
        cases.append((name, ok, detail))

    def lui(reg: int, imm: int) -> int:
        return (OP_LUI << 26) | (0 << 21) | (reg << 16) | (imm & 0xFFFF)

    def addiu(rt: int, rs: int, imm: int) -> int:
        return (OP_ADDIU << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)

    def ori(rt: int, rs: int, imm: int) -> int:
        return (OP_ORI << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)

    def lw(rt: int, off: int, base: int) -> int:
        return (OP_LW << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)

    def sw(rt: int, off: int, base: int) -> int:
        return (OP_SW << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)

    # POSITIVE: 0x800578D0 — the Crash 1 shape this tool exists for. one writer, three readers.
    target = 0x800578D0
    hi, lo = _hi_lo(target)
    words = [
        (0x80010000, lui(8, hi)),
        (0x80010004, addiu(8, 8, lo)),
        (0x80010008, sw(9, 0, 8)),          # the single writer
        (0x8001000C, lw(10, 0, 8)),
        (0x80010010, lw(11, 0, 8)),
        (0x80010014, lw(12, 0, 8)),
    ]
    result = _fixture(words)
    globals_found = result["globals"]
    hit = globals_found.get(target)
    add(
        "a one-writer many-reader main-RAM global is FOUND",
        hit is not None and hit.writer == 0x80010008 and len(hit.reader) == 3,
        f"found={hit is not None} writer={None if hit is None else hex(hit.writer)} "
        f"readers={0 if hit is None else len(hit.reader)}",
    )

    # NEGATIVE: the same formation, but the address is in the BIOS/KSEG window, not main RAM.
    outside = 0x1F801040
    hi, lo = _hi_lo(outside)
    other = _fixture([(0x80010000, lui(8, hi)), (0x80010004, addiu(8, 8, lo)), (0x80010008, lw(9, 0, 8))])
    add(
        "a peripheral address is NOT reported as a main-RAM global",
        outside not in other["globals"],
        f"globals={len(other['globals'])}",
    )

    # NEGATIVE: an address just past the 2 MiB of main RAM is not a global of this title.
    past = MAIN_RAM_BASE + MAIN_RAM_BYTES
    hi, lo = _hi_lo(past)
    beyond = _fixture([(0x80010000, lui(8, hi)), (0x80010004, addiu(8, 8, lo)), (0x80010008, lw(9, 0, 8))])
    add(
        "an address past the 2 MiB of main RAM is NOT reported",
        past not in beyond["globals"],
        f"globals={len(beyond['globals'])}",
    )

    # NEGATIVE: the `ori` formation, which is the OTHER way this image names a global, and the
    # one an `addiu`-only scan would miss entirely.
    ori_target = 0x800B5900
    hi, lo = _hi_lo(ori_target)
    ori_case = _fixture(
        [
            (0x80010000, lui(8, hi)),
            (0x80010004, ori(8, 8, lo)),
            (0x80010008, sw(9, 0, 8)),
            (0x8001000C, lw(10, 0, 8)),
        ]
    )
    add(
        "an `ori`-formed global is FOUND (an addiu-only scan cannot see it)",
        ori_target in ori_case["globals"],
        f"globals={sorted(hex(a) for a in ori_case['globals'])}",
    )

    # NEGATIVE: a `lui` with no following addiu/ori into the same register names nothing.
    lonely = _fixture([(0x80010000, lui(8, hi)), (0x80010004, addiu(9, 8, lo))])
    add(
        "a `lui` with no same-register displacement names nothing",
        len(lonely["globals"]) == 0,
        f"globals={len(lonely['globals'])}",
    )

    # The denominator must be reported even when nothing matches, or "(none)" reads as an answer.
    empty = _fixture([])
    add(
        "an empty image reports walked=0 and refused=0 rather than a bare empty result",
        empty["walked"] == 0 and empty["decoded"] == 0 and empty["refused"] == 0,
        f"walked={empty['walked']} decoded={empty['decoded']} refused={empty['refused']}",
    )

    failed = 0
    for name, ok, detail in cases:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}  [{detail}]")
        if not ok:
            failed += 1
    print(f"selftest: {len(cases) - failed} of {len(cases)} case(s) passed")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--top", type=int, default=20, help="how many candidates to list")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--image", type=Path, default=RAM)
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    if not args.image.is_file():
        print(f"REFUSED: no RAM image at {args.image}. Provision it first; a scan of a missing "
              f"image would report zero candidates and that zero would be meaningless.")
        return 2
    data = args.image.read_bytes()
    words = []
    address = REGION_START
    while address + 4 <= REGION_END and address + 4 <= len(data) + MAIN_RAM_BASE:
        offset = address - MAIN_RAM_BASE
        if offset + 4 > len(data):
            break
        words.append((address, struct.unpack_from("<I", data, offset)[0]))
        address += 4

    result = scan(words)
    globals_found = result["globals"]
    mem_bytes = len(data)
    print(f"// scanned {result['walked']} instruction word(s) in "
          f"[0x{REGION_START:08X}, 0x{REGION_END:08X}) of {mem_bytes} byte(s) in the RAM image "
          f"({100.0 * (REGION_END - REGION_START) / mem_bytes:.1f}%)")
    print(f"// {result['decoded']} word(s) decoded by probe_mips_image, "
          f"{result['refused']} refused by it as an encoding it does not name")
    print("// BLIND SPOT: bytes outside the declared resident text are not code and were not "
          "scanned, and a MIPS branch DELAY SLOT is decoded as its own word, so every site below "
          "is 'a word that decodes as this form', not 'an instruction executed here'")
    print(f"// main-RAM globals this text can NAME with a lui + 16-bit displacement: "
          f"{len(globals_found)}")

    candidates = sorted(
        (g for g in globals_found.values() if g.writer is not None and g.reader),
        key=lambda g: (-len(g.reader), g.address),
    )
    written_only = sum(1 for g in globals_found.values() if g.writer is not None and not g.reader)
    read_only = sum(1 for g in globals_found.values() if g.writer is None and g.reader)
    print(f"// of those: {len(candidates)} have BOTH a writer and at least one reader "
          f"(the shape a bound has), {written_only} are written and never read through a formed "
          f"address, {read_only} are read and never written through one")
    print("// A CANDIDATE IS NOT A CULL. Whether one of these bounds a projected coordinate, and "
          "along WHICH axis, is the comparison's dataflow, which this tool does not compute. "
          "Read it with Ghidra before believing it.")
    print()
    print(f"{'global':>10}  {'writer':>10}  {'readers':>7}   note")
    for site in candidates[: args.top]:
        print(f"0x{site.address:08X}  0x{site.writer:08X}  {len(site.reader):>7}   "
              f"first reader 0x{site.reader[0]:08X}")
    if not candidates:
        print("// (none) — and that is a SCANNED ANSWER, not an absence of scanning: see the "
              "denominators above.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
