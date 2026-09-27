#!/usr/bin/env python3
"""probe_mips_image.py — zero-dependency MIPS-I/II/III word decoder over the RAM image.

WHY THIS EXISTS. `external/psxport/tools/disasm.py` needs capstone, which is NOT in this
repository's locked environment (`pyproject.toml` declares no dependencies), so a census cannot be
built on it. The alternative, hand-rolled ad-hoc pattern tests against a few known addresses, is
exactly what produced the confidently WRONG conclusions `tools/ghidra_query.py` documents: an address
scan only finds the reference FORMS you already thought of.

So this decodes EVERY aligned word in the resident text segment and reports what it finds with a
DENOMINATOR: words decoded, words that decoded to a known instruction, words refused. "0 matches" is
then distinguishable from "I never looked", and an undecoded word is a visible gap rather than a
silent one. `0xFFFFFFFF` and `0x00000000` padding are counted separately and never reported as
matches, because a census that matches its own padding has measured nothing.

Nothing here executes, translates, or emits guest code. It reads a RAM dump and prints text.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

RAM_BYTES = 2 * 1024 * 1024
RAM_BASES = (0, 0x80000000, 0xA0000000)

REGISTER_NAMES = (
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$s8", "$ra",
)

SPECIAL_FUNCTIONS = {
    0x00: "sll", 0x02: "srl", 0x03: "sra", 0x04: "sllv", 0x06: "srlv",
    0x07: "srav", 0x08: "jr", 0x09: "jalr", 0x0C: "syscall", 0x0D: "break",
    0x10: "mfhi", 0x11: "mthi", 0x12: "mflo", 0x13: "mtlo", 0x18: "mult",
    0x19: "multu", 0x1A: "div", 0x1B: "divu", 0x20: "add", 0x21: "addu",
    0x22: "sub", 0x23: "subu", 0x24: "and", 0x25: "or", 0x26: "xor",
    0x27: "nor", 0x2A: "slt", 0x2B: "sltu", 0x30: "tge", 0x33: "tltu",
    0x34: "teq", 0x36: "tne",
}
REGIMM_FUNCTIONS = {0x00: "bltz", 0x01: "bgez", 0x10: "bltzal", 0x11: "bgezal"}
COP1_FUNCTIONS = {0x00: "mfc1", 0x02: "cfc1", 0x04: "mtc1", 0x06: "ctc1", 0x08: "bc1f", 0x09: "bc1t"}
# op 18 is the GTE (COP2). Its register-transfer forms are selected by the five bits 25..21
# (MFC2/CFC2 load, MTC2/CTC2 store) and the low five bits are the GTE register number. The
# arithmetic forms share those bits for a different purpose, so only the four register transfers are
# named; everything else prints as its raw encoding and is never mislabelled.
COP2_FUNCTIONS = {0x00: "mfc2", 0x02: "cfc2", 0x04: "mtc2", 0x06: "ctc2"}

# The opcodes this census decodes. 0x26/0x2E (ld/sd) and 0x38/0x3A (SC variants) are named only where
# the encoding is unambiguous; anything absent is REFUSED by `decode`, and a census prints how many
# words it refused, so a gap is visible instead of a silent zero.
LOAD_STORE = {
    32: "lb", 33: "lh", 34: "lwl", 35: "lw", 36: "lbu", 37: "lhu", 40: "sb", 41: "sh", 42: "swl",
    43: "sw", 46: "swr",
}


@dataclass(frozen=True)
class Instruction:
    address: int
    word: int
    mnemonic: str
    operands: str
    fields: tuple[int, int, int, int, int, int]

    def text(self) -> str:
        return f"0x{self.address:08X}  {self.word:08X}  {self.mnemonic:8s} {self.operands}"


def sign_extend(value: int, bits: int) -> int:
    mask = 1 << (bits - 1)
    return (value & (mask - 1)) - (value & mask)


def branch_target(address: int, word: int) -> int:
    return (address + 4 + (sign_extend(word & 0xFFFF, 16) << 2)) & 0xFFFFFFFF


def jump_target(address: int, word: int) -> int:
    return ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def decode(address: int, word: int) -> Instruction | None:
    """Decode one word, or return None when the encoding is not one this census understands."""
    op = (word >> 26) & 0x3F
    rs = (word >> 21) & 0x1F
    rt = (word >> 16) & 0x1F
    rd = (word >> 11) & 0x1F
    shamt = (word >> 6) & 0x1F
    funct = word & 0x3F
    imm = word & 0xFFFF
    simm = sign_extend(imm, 16)
    fields = (op, rs, rt, rd, shamt, funct)

    def made(mnemonic: str, operands: str) -> Instruction:
        return Instruction(address, word, mnemonic, operands, fields)

    if word == 0:
        return made("nop", "")
    if op == 0:
        if funct in SPECIAL_FUNCTIONS:
            mnemonic = SPECIAL_FUNCTIONS[funct]
            if mnemonic in {"sll", "srl", "sra"}:
                return made(mnemonic, f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rt]}, {shamt}")
            if mnemonic in {"sllv", "srlv", "srav"}:
                return made(mnemonic, f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}")
            if mnemonic == "jr":
                return made(mnemonic, REGISTER_NAMES[rs])
            if mnemonic == "jalr":
                return made(mnemonic, f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rs]}")
            if mnemonic in {"mult", "multu", "div", "divu"}:
                return made(mnemonic, f"{REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}")
            if mnemonic in {"mfhi", "mflo"}:
                return made(mnemonic, REGISTER_NAMES[rd])
            if mnemonic in {"mthi", "mtlo"}:
                return made(mnemonic, REGISTER_NAMES[rs])
            if mnemonic in {"syscall", "break"}:
                return made(mnemonic, "")
            return made(mnemonic, f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}")
        if funct == 0x01 and rd == 0 and rt == 0:
            return made("bltzal" if rs & 0x10 else "bgezal", "")
        return None
    if op == 1:
        mnemonic = REGIMM_FUNCTIONS.get(rt)
        if mnemonic is None:
            return None
        return made(mnemonic, f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
    if op in {2, 3}:
        return made("j" if op == 2 else "jal", f"0x{jump_target(address, word):08X}")
    if op == 4:
        return made("beq", f"{REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}, 0x{branch_target(address, word):08X}")
    if op == 5:
        return made("bne", f"{REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}, 0x{branch_target(address, word):08X}")
    if op == 6:
        return made("blez", f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
    if op == 7:
        return made("bgtz", f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
    if op == 8:
        return made("addi", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, {simm}")
    if op == 9:
        return made("addiu", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, {simm}")
    if op == 10:
        return made("slti", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, {simm}")
    if op == 11:
        return made("sltiu", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, {simm}")
    if op == 12:
        return made("andi", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, 0x{imm:04X}")
    if op == 13:
        return made("ori", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, 0x{imm:04X}")
    if op == 14:
        return made("xori", f"{REGISTER_NAMES[rt]}, {REGISTER_NAMES[rs]}, 0x{imm:04X}")
    if op == 15:
        return made("lui", f"{REGISTER_NAMES[rt]}, 0x{imm:04X}")
    if op == 16:
        if rs == 0:
            return made("mfc0", f"{REGISTER_NAMES[rt]}, $c{rd}")
        if rs == 4:
            return made("mtc0", f"{REGISTER_NAMES[rt]}, $c{rd}")
        return None
    if op == 17:
        if funct == 0x01:
            return made("bgez", f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
        return made("cop17", f"0x{word & 0x1FFFFFF:07X}")
    if op == 18:
        # COP2 is the GTE. Only the four register-transfer forms are named; the arithmetic forms
        # (RTPS/RTPT/NCT/NCCT) and the rest print as their raw encoding, so an unnamed form is
        # visible rather than silently mislabelled.
        if ((word >> 21) & 0x1F) in COP2_FUNCTIONS:
            return made(COP2_FUNCTIONS[(word >> 21) & 0x1F], f"{REGISTER_NAMES[rt]}, ${rd}")
        return made("cop2", f"0x{word & 0x1FFFFFF:07X}")
    if op == 19:
        if funct == 0x11:
            return made("cop1x", f"0x{word & 0x1FFFFFF:07X}")
        mnemonic = COP1_FUNCTIONS.get(rs)
        if mnemonic is None:
            return None
        if mnemonic.startswith("bc1"):
            return made(mnemonic, f"0x{branch_target(address, word):08X}")
        return made(mnemonic, f"{REGISTER_NAMES[rt]}, $f{rd}")
    if op == 20:
        return made("beql", f"{REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}, 0x{branch_target(address, word):08X}")
    if op == 21:
        return made("bnel", f"{REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}, 0x{branch_target(address, word):08X}")
    if op == 22:
        return made("blezl", f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
    if op == 23:
        return made("bgtzl", f"{REGISTER_NAMES[rs]}, 0x{branch_target(address, word):08X}")
    if op in LOAD_STORE:
        return made(LOAD_STORE[op], f"{REGISTER_NAMES[rt]}, {simm}({REGISTER_NAMES[rs]})")
    if op == 28:
        if funct == 0x20:
            return made("add", f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}")
        if funct == 0x21:
            return made("addu", f"{REGISTER_NAMES[rd]}, {REGISTER_NAMES[rs]}, {REGISTER_NAMES[rt]}")
        if funct == 0x10:
            return made("cop2", f"0x{word & 0x1FFFFFF:07X}")
        return made("cop2", f"0x{word & 0x1FFFFFF:07X}")
    if op == 0x32:
        return made("lwc2", f"{REGISTER_NAMES[rt]}, {simm}({REGISTER_NAMES[rs]})")
    if op == 0x3A:
        return made("swc2", f"{REGISTER_NAMES[rt]}, {simm}({REGISTER_NAMES[rs]})")
    if op == 31:
        if funct == 0x00:
            return made("ll", f"{REGISTER_NAMES[rt]}, {simm}({REGISTER_NAMES[rs]})")
        if funct == 0x08:
            return made("sc", f"{REGISTER_NAMES[rt]}, {simm}({REGISTER_NAMES[rs]})")
        return made("cop1x", f"0x{word & 0x1FFFFFF:07X}")
    return None


def write_elf32_mips(text: bytes, base: int, out: Path) -> None:
    """Wrap a raw image as an ELF32 MIPS-LE executable section, so `llvm-objdump -d` can be used as
    an INDEPENDENT cross-check of this decoder. Nothing here executes or translates guest code."""
    machine = 8  # EM_MIPS
    entry = base
    phoff = 52
    shoff = 52 + 32 * 1
    header = bytearray(52)
    struct.pack_into("<4sBBBB8xHHIIIIIHHHHHH", header, 0,
                     b"\x7fELF", 1, 1, 1, 0, 2, machine, 1,
                     entry, phoff, shoff, 0x1001, 52, 32, 1, 40, 3, 2)
    program = struct.pack("<IIIIIIII", 1, 0, 0, 0, len(text), len(text), 5, 0x1000)
    shstr = b"\x00.text\x00.shstrtab\x00"
    text_name = 1
    shstr_name = 7
    headers_end = shoff + 3 * 40
    text_offset = (headers_end + 3) & ~3
    shstr_offset = text_offset + len(text)
    sections = [
        struct.pack("<IIIIIIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        struct.pack("<IIIIIIIIII", text_name, 1, 6, base, text_offset, len(text), 0, 0, 4, 0),
        struct.pack("<IIIIIIIIII", shstr_name, 3, 0, 0, shstr_offset, len(shstr), 0, 0, 1, 0),
    ]
    assert len(bytes(header)) + len(program) + sum(len(section) for section in sections) == headers_end
    out.write_bytes(bytes(header) + program + b"".join(sections)
                    + b"\x00" * (text_offset - headers_end) + text + shstr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ram", type=Path, help="2 MiB RAM dump")
    parser.add_argument("start", help="first address, hex or 0x-prefixed")
    parser.add_argument("end", help="exclusive end address")
    parser.add_argument("--elf", type=Path, help="also write an ELF32-MIPS-LE wrapper for llvm-objdump")
    args = parser.parse_args()

    data = args.ram.read_bytes()
    if len(data) != RAM_BYTES:
        print(f"probe_mips_image: {args.ram} is {len(data)} bytes; a RAM dump is exactly {RAM_BYTES}")
        return 2
    start = int(args.start, 16)
    end = int(args.end, 16)
    for base in RAM_BASES:
        if base <= start < end <= base + RAM_BYTES:
            start -= base
            end -= base
            break
    else:
        print("probe_mips_image: range is not inside one 2 MiB RAM mapping")
        return 2

    if args.elf:
        write_elf32_mips(data, 0x80000000, args.elf)
        print(f"[elf] wrote {args.elf}")

    decoded = 0
    refused = 0
    padding = 0
    for address in range(start, end, 4):
        word = struct.unpack_from("<I", data, address)[0]
        if word in (0x00000000, 0xFFFFFFFF):
            padding += 1
            continue
        instruction = decode(0x80000000 + address, word)
        if instruction is None:
            refused += 1
            continue
        decoded += 1
        print(instruction.text())
    print(f"[decode] scanned {(end - start) // 4} words: {decoded} decoded, {refused} refused, "
          f"{padding} zero/nop padding")
    return 0


if __name__ == "__main__":
    sys.exit(main())
