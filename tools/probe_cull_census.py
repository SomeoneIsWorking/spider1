#!/usr/bin/env python3
"""Census SLUS_008.75's horizontal culling owners, from the instruction words themselves.

WHY AN INSTRUMENT AND NOT A NOTE. A widening frustum only shows more world if nothing discards it
first, and on this title the discarding happens in guest code this repository does not own. The
honest way to answer "are the new margins covered" is therefore to enumerate every site that can
discard, not to read two functions and generalize. Two sibling titles each found SEVEN culling
owners by scanning for the idiom where a targeted search had found two, so the scan is the method.

IT REUSES THE REPOSITORY'S DECODER. Every instruction here comes from `probe_mips_image.decode`,
which is this repository's one MIPS word decoder and which already names the GTE's four
register-transfer forms. This file does not re-derive opcode or field positions — which is the point
of the reuse: the first version of this census hand-rolled its own extraction and got the GTE's
`ctc2` encoding wrong three separate times (see the note on `projection_transfers`). The one thing
the shared decoder deliberately leaves raw is the GTE *command* field of the arithmetic forms, and
that interpretation is stated here where it can be checked.

THE IDIOM IS THE RECORD, NOT A LITERAL. Read out of the executable: the 6-bit visibility outcode at
0x8007C2AC and 0x8007B9CC wraps a projected vertex against SIX words reached through the pointer
cell at 0x800B5918, which the projection publication (0x80075D0C) refreshes every call:

    0x800B5918 + 0  horizontal far      bits 0,1 of the outcode (vs screen X)
    0x800B5918 + 2  vertical near      bits 2,3       (vs screen Y)
    0x800B5918 + 4  horizontal near    bits 0,1
    0x800B5918 + 6  vertical far       bits 2,3
    0x800B5918 + 8  depth lower        bits 4,5       (vs GTE IR1/SZ, NOT a screen axis)
    0x800B5918 + 10 depth upper        bits 4,5

So the cell is the single choke point every projection-aware cull reaches through, and this tool
counts the references to it by DECODING EVERY INSTRUCTION WORD. Two reference forms exist and both
are counted: the `gp`-relative `lw rt, 0x1124(gp)`, and the `lui`/`lw` pair that forms 0x800B5918
absolutely.

IT ALSO COUNTS THE THING THAT WOULD DEFEAT THE WIDENING. A cull that compares against a retail
4:3-derived LITERAL instead of the record is a site that widening the record cannot reach. Those are
reported as a count of that instruction FORM, and this tool deliberately does NOT call them culling
owners: deciding that needs the comparison's dataflow, which is not a word-match question. The
count is neither a list of defects nor a ceiling on them.

THE DENOMINATOR IS ALWAYS PRINTED, and so is the blind spot: words outside the declared resident
text, words the shared decoder refuses, and the fact that a MIPS branch delay slot is decoded as its
own word, so every hit is "a word that decodes as this form" and not "an instruction executed here".

    uv run --frozen python tools/probe_cull_census.py
    uv run --frozen python tools/probe_cull_census.py --self-test
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probe_mips_image import Instruction, decode  # noqa: E402  (path set above)

RAM = Path(__file__).resolve().parents[1] / "scratch" / "bin" / "spiderman" / "ram.bin"

# The resident text this port's own GuestProgramImage declares for SLUS_008.75
# (spider1_runtime.h: .residentText = {0x00010000, 0x000C65D4}). Using the declared region rather
# than "the whole file" keeps the denominator honest: the 2 MiB dump also holds bss and heap, which
# are not code and must not be counted as "scanned and found nothing".
REGION_START = 0x80010000
REGION_END = 0x800C65D4

RECORD_CELL = 0x800B5918
RECORD_CELL_GP_OFFSET = 0x1124
GLOBAL_POINTER = 0x800B47F4
REG_GP = 28
OP_LW = 0x23

# The record's six window words, as offsets from the record pointer the cell holds.
WINDOW_FIELDS = {
    0: "horizontal far",
    2: "vertical near",
    4: "horizontal near",
    6: "vertical far",
    8: "depth lower",
    10: "depth upper",
}
HORIZONTAL_FIELDS = (0, 4)
DEPTH_FIELDS = (8, 10)

# GTE control registers 24/25/26 are OFX/OFY/H: the projection, and the only registers whose
# transfer a guest projection widening has to move.
PROJECTION_REGS = (24, 25, 26)
PROJECTION_REG_NAMES = {24: "CR24 OFX", 25: "CR25 OFY", 26: "CR26 H"}

# The GTE's 12-bit command field is bits 0..11, which OVERLAPS the control register number in bits
# 15..11. That is why the shared decoder reads the register out of `rd`. RTPS is command 0x001; the
# shared decoder leaves the arithmetic forms as `cop2` with the raw encoding, so this is the single
# interpretation this file adds, and it is the only place it touches an encoding.
GTE_COMMAND_MASK = 0xFFF
GTE_COMMAND_CTC2 = 0x000
GTE_COMMAND_RTPS = 0x001

# The 4:3-derived literals a cull owner might compare a projected coordinate against instead of
# reading the record. 320/368/384/512 are the PSX display and projection widths this title uses, 240
# its line count, 256 the retail OFX, and 428/492/684 the widened results the framework's plan rule
# produces from them (a stale widened literal left in the code is the defect this catches).
CANDIDATE_LITERALS = (240, 256, 320, 368, 384, 428, 492, 512, 684)


def decode_region(image: bytes) -> tuple[list[tuple[int, Instruction]], int]:
    """(decoded instructions, words the shared decoder refused) over the declared resident text."""
    base = REGION_START - 0x80000000
    count = (REGION_END - REGION_START) // 4
    flat = struct.unpack_from(f"<{count}I", image, base)
    decoded: list[tuple[int, Instruction]] = []
    refused = 0
    for index, word in enumerate(flat):
        instruction = decode(REGION_START + 4 * index, word)
        if instruction is None:
            refused += 1
            continue
        decoded.append((REGION_START + 4 * index, instruction))
    return decoded, refused


def gp_relative_cell_reads(decoded: list[tuple[int, Instruction]]) -> list[int]:
    """`lw rt, 0x1124(gp)` — the form most of the references use."""
    hits = []
    for address, instruction in decoded:
        op, rs, _, _, _, _ = instruction.fields
        if op == OP_LW and rs == REG_GP and (instruction.word & 0xFFFF) == RECORD_CELL_GP_OFFSET:
            hits.append(address)
    return hits


def absolute_cell_reads(decoded: list[tuple[int, Instruction]]) -> tuple[list[int], int]:
    """`lui rX, hi` followed by `lw rY, lo(rX)` forming 0x800B5918.

    A high half is tracked across `lui`, `nop` and `lw` only; any other word drops it, because this
    tool has no dataflow and must not claim a pairing it cannot see. Every rejection is COUNTED and
    returned, so a site lost to that conservatism is visible in the report instead of silently
    absent. The Ghidra cross-check printed alongside is what settles the total.
    """
    hits: list[int] = []
    pending: dict[int, int] = {}
    rejected = 0
    for address, instruction in decoded:
        _, rs, rt, _, _, _ = instruction.fields
        if instruction.mnemonic == "lui":
            pending[rt] = ((instruction.word & 0xFFFF) << 16) & 0xFFFFFFFF
            continue
        if instruction.mnemonic == "lw":
            if rs in pending:
                if (pending[rs] + (instruction.word & 0xFFFF)) & 0xFFFFFFFF == RECORD_CELL:
                    hits.append(address)
                else:
                    rejected += 1
            continue
        if instruction.mnemonic == "nop":
            continue
        for reg in (rt, rs):
            if reg in pending:
                del pending[reg]
                rejected += 1
    return hits, rejected


def projection_transfers(decoded: list[tuple[int, Instruction]]) -> tuple[list[tuple[int, int]],
                                                                       list[tuple[int, int]]]:
    """CR24/CR25/CR26 transfers: (ctc2 writes, mfc2 reads).

    A ctc2 WRITE asserts a projection centre and an mfc2 READ consumes one, so the two are counted
    apart: conflating them is what made an earlier version of this tool report 68 sites when 20 of
    them were reads.

    The shared decoder is what makes the distinction reliable. It names the GTE's four
    register-transfer forms directly, and it puts the control register in `rd` — which matters,
    because on this GTE the 12-bit command field is bits 0..11 and OVERLAPS the register number in
    bits 15..11. Verified against the two leaves themselves:
        0x8008BF14  ctc2 a0, $26  SetGeomScreen -> CR26 (H)
        0x8008BF2C  ctc2 a0, $24  SetGeomOffset -> CR24 (OFX)
        0x8008BF30  ctc2 a1, $25                -> CR25 (OFY)
        0x8007C5F8  mfc2 t9, $24  a READ of OFX, not a write
    An earlier hand-rolled version of this function reported ZERO across the whole resident text,
    then a plausible 38 while silently skipping every write whose register number has bit 0 set. A
    decoder that finds nothing where the image is known to contain the thing is wrong, not the
    image; a decoder that finds a plausible number is not thereby right either. Reusing the
    repository's decoder is the fix, and the two leaves are quoted so it can be checked in one step.
    """
    writes: list[tuple[int, int]] = []
    reads: list[tuple[int, int]] = []
    for address, instruction in decoded:
        if instruction.mnemonic == "ctc2":
            bucket = writes
        elif instruction.mnemonic == "mfc2":
            bucket = reads
        else:
            continue
        register = instruction.fields[3]
        if register in PROJECTION_REGS:
            bucket.append((address, register))
    return writes, reads


def rtps_calls(decoded: list[tuple[int, Instruction]]) -> list[int]:
    """RTPS, the only instruction that produces SX/SY/SZ/IR0..3.

    Counted as the denominator for "how much of this image could own a horizontal cull at all": a
    cull compares a coordinate a projection produced, so a function with no RTPS and no record read
    has no screen coordinate of its own to cull with. The shared decoder leaves the GTE arithmetic
    forms as `cop2`, so this is the one command-code reading this file adds.
    """
    return [
        address
        for address, instruction in decoded
        if instruction.mnemonic == "cop2"
        and (instruction.word & GTE_COMMAND_MASK) == GTE_COMMAND_RTPS
    ]


def literal_candidates(decoded: list[tuple[int, Instruction]]) -> list[tuple[int, int]]:
    """A 4:3-derived literal loaded by addiu/ori — a FORM, not a classification."""
    hits = []
    for address, instruction in decoded:
        if instruction.mnemonic in ("addiu", "ori") and (instruction.word & 0xFFFF) in CANDIDATE_LITERALS:
            hits.append((address, instruction.word & 0xFFFF))
    return hits


def run(image: bytes) -> int:
    decoded, refused = decode_region(image)
    total_words = (REGION_END - REGION_START) // 4
    covered = REGION_END - REGION_START
    print(f"// scanned {total_words} instruction word(s) in [0x{REGION_START:08X}, "
          f"0x{REGION_END:08X}) = {covered} of {len(image)} byte(s) in the RAM image "
          f"({100.0 * covered / len(image):.1f}%)")
    print(f"// {len(decoded)} word(s) decoded by probe_mips_image, {refused} refused by it as an "
          f"encoding it does not name. gp = 0x{GLOBAL_POINTER:08X}, so the record cell is "
          f"gp+0x{RECORD_CELL_GP_OFFSET:04X}.")
    print(f"// BLIND SPOT: the {len(image) - covered} byte(s) outside the declared resident text are "
          f"not code and were not scanned; a branch delay slot is decoded as its own word, so each "
          f"hit is 'a word that decodes as this form', not 'an instruction executed here'.")

    gp_hits = gp_relative_cell_reads(decoded)
    abs_hits, rejected = absolute_cell_reads(decoded)
    total_hits = sorted(set(gp_hits) | set(abs_hits))
    print(f"// viewport-record cell 0x{RECORD_CELL:08X}: {len(gp_hits)} gp-relative read(s), "
          f"{len(abs_hits)} absolute read(s), {len(total_hits)} distinct site(s)")
    print(f"// POSSIBLE LOSS: {rejected} absolute pairing(s) rejected as unverifiable without "
          f"dataflow. A count below the cross-check is explained by this number, not by an absent "
          f"site.")
    print("// CROSS-CHECK: Ghidra's reference model independently reports 27 references to this cell "
          "across 16 functions plus 3 sites it places in no function. It is a LOWER BOUND, not a "
          "rival answer: it creates no reference for a gp-relative read inside a function it "
          "defines, which is why this scan finds more.")
    for address in total_hits:
        print(f"  0x{address:08X}  record read")

    gte_writes, gte_reads = projection_transfers(decoded)
    print(f"// CR24/CR25/CR26: {len(gte_writes)} ctc2 WRITE(s) and {len(gte_reads)} mfc2 READ(s). A "
          f"write ASSERTS a projection centre and a read CONSUMES one, so they are counted apart. "
          f"0x8008BF2C/0x8008BF30 are SetGeomOffset itself and 0x8008BF14 is SetGeomScreen; "
          f"0x8007C0B4 writes ZERO to CR24 and is a GTE matrix reset, not a retail OFX.")
    for address, reg in gte_writes:
        print(f"  0x{address:08X}  ctc2 WRITE -> {PROJECTION_REG_NAMES[reg]}")
    for address, reg in gte_reads:
        print(f"  0x{address:08X}  mfc2 READ  -> {PROJECTION_REG_NAMES[reg]}")

    rtps = rtps_calls(decoded)
    print(f"// RTPS: {len(rtps)} site(s). A horizontal cull compares a screen coordinate, so this is "
          f"the denominator for how much of the image could own one at all.")
    literals = literal_candidates(decoded)
    print(f"// 4:3-derived literal loaded by addiu/ori: {len(literals)} site(s) of that FORM. This is "
          f"a count of an instruction shape, NOT a count of culling owners and NOT a ceiling on "
          f"them: most are ordinary arithmetic on a coordinate that happens to be 512 or 240. "
          f"Deciding whether any of them bounds a projected coordinate needs the comparison's "
          f"dataflow, which this tool does not compute. So a cull against a retail literal is "
          f"neither confirmed nor excluded here.")
    print(f"// record window fields: horizontal {HORIZONTAL_FIELDS} (the axis a widening moves), "
          f"depth {DEPTH_FIELDS} (a different axis, compared against GTE IR1/SZ)")
    for offset, name in WINDOW_FIELDS.items():
        print(f"  +{offset:<3} {name}")
    return 0


def self_test() -> int:
    """The negative first: a hand-built instruction stream must produce exactly the sites planted.

    Without this, a scan that matched nothing would look like a census result, and a scan that
    matched everything would look like a busy image. The words are encoded by hand from the MIPS-I
    layouts, because a self-test that reuses the decoder's own expression would agree with any bug
    in it.
    """
    cases: list[tuple[str, list[int], dict[str, list[int]]]] = [
        (
            "one gp-relative read, planted",
            [0x8F821124],  # lw $v0, 0x1124(gp)
            {"gp": [0x80000000]},
        ),
        (
            "a neighbouring gp offset is NOT the cell",
            [0x8F821120, 0x8F821128],
            {"gp": []},
        ),
        (
            "the same offset from a base that is not gp is NOT the cell",
            [0x8F821124 - (REG_GP << 21) + (2 << 21)],  # lw $v0, 0x1124($v1)
            {"gp": []},
        ),
        (
            "an absolute lui+lw pair, adjacent",
            [0x3C04800B, 0x8C825918],  # lui $a0,0x800b ; lw $v0,0x5918($a0)
            {"abs": [0x80000004]},
        ),
        (
            "an absolute pair with a stale high half between the two words",
            [0x3C04800B, 0x00802021, 0x8C825918],  # ... or $a1,$a0,0x0000 redefines $a0
            {"abs": []},
        ),
        (
            "a lui that forms a different address is not the cell",
            [0x3C04800C, 0x8C825918],
            {"abs": []},
        ),
        (
            "ctc2 into CR24/CR25/CR26 is counted as a WRITE; CR27 is not",
            [0x48C0C000, 0x48C0C800, 0x48C0D000, 0x48C0D800],
            {"ctc2": [0x80000000, 0x80000004, 0x80000008], "mfc2": []},
        ),
        (
            "the same shape with rs=0 is mfc2, a READ",
            [0x4819C000, 0x480CC000],
            {"ctc2": [], "mfc2": [0x80000000, 0x80000004]},
        ),
        (
            "rtps is a cop2 arithmetic form, not a transfer",
            [0x4A180001, 0x4A486012],  # rtps, nclip
            {"ctc2": [], "mfc2": [], "rtps": [0x80000000]},
        ),
        (
            "a 4:3-derived immediate in addiu and ori is a candidate; 0x2E9 is not",
            [0x24060200, 0x34030170, 0x240302E9],  # addiu $a2,$0,512 / ori $v1,$0,368
            {"lit": [0x80000000, 0x80000004]},
        ),
    ]
    failures = 0
    for name, stream, expected in cases:
        decoded: list[tuple[int, Instruction]] = []
        for index, word in enumerate(stream):
            instruction = decode(0x80000000 + 4 * index, word)
            if instruction is not None:
                decoded.append((0x80000000 + 4 * index, instruction))
        actual = {
            "gp": gp_relative_cell_reads(decoded),
            "abs": absolute_cell_reads(decoded)[0],
            "ctc2": [a for a, _ in projection_transfers(decoded)[0]],
            "mfc2": [a for a, _ in projection_transfers(decoded)[1]],
            "rtps": rtps_calls(decoded),
            "lit": [a for a, _ in literal_candidates(decoded)],
        }
        for key, want in expected.items():
            got = actual[key]
            status = "ok" if got == want else "MISMATCH"
            if got != want:
                failures += 1
            print(f"  {status}: {name} [{key}] want {[hex(x) for x in want]} "
                  f"got {[hex(x) for x in got]}")
    print(f"self-test: {len(cases)} case(s), {failures} mismatch(es)")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    if not RAM.is_file():
        print(f"probe_cull_census: REFUSED — no RAM image at {RAM}. Provision the title first; a "
              f"census with no corpus is not a result.")
        return 1
    return run(RAM.read_bytes())


if __name__ == "__main__":
    sys.exit(main())
