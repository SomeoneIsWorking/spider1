#!/usr/bin/env python3
"""check_reloc_model.py — does the BASE-RELATIVE emission model hold on every real module?

The recompiler can emit a runtime-loaded module base-relative (so the guest allocator places it
wherever it likes, as the console does) only if the .rel stream's relocation sites have the shapes
the emitter assumes. This checks those assumptions against all 30 CD.WAD modules — the raw,
UNrelocated images, straight out of the archive, so nothing this script says depends on a base.

THE ASSUMPTIONS, one check each (see external/psxport/tools/recomp/emit.py, module_delta):

  A1  every R_MIPS_HI16 site is a `lui`.                 -> the emitter adds delta to its result
  A2  every R_MIPS_LO16 site is an instruction with a 16-bit immediate that is an address low half
      (addiu/ori/lw/sw/lb/lbu/lh/lhu/lwl/lwr/swl/swr/sb/sh), and its base/source register traces
      back — through register-to-register moves — to a HI16-relocated `lui`.
                                                          -> the emitter emits LO16 sites UNCHANGED
  A3  every R_MIPS_26 site is a `j` or `jal`.             -> the emitter adds delta to the target
  A4  no offset is named twice by the stream, and no R_MIPS_32 site is also an instruction site
      named by a HI16/LO16/26 entry.                      -> one rule per word
  A5  the stream terminates cleanly and consumes the whole .rel file.

WHAT A NEGATIVE MEANS HERE. Every check prints its DENOMINATOR (how many sites it examined), so
"0 violations" is distinguishable from "I never looked". A2's backward scan is the one check with a
real blind spot and it says so: it is not flow-sensitive, so it establishes that a relocated `lui`
for the base register exists EARLIER in the image, not that it dominates the use. Sites it cannot
pair are reported as UNPAIRED (a thing to look at), counted separately from a hard violation.

This checker answers a NECESSARY condition, not a sufficient one. The sufficient check is
`--reloc-selfcheck` in emit.py: emit every module from images relocated at two different bases and
require the generated C to be byte-identical. Any base dependence this file's shape rules miss shows
up there as a diff.

Self-test: `--selftest` feeds synthetic modules that MUST trip each check, so a clean run on the
real data cannot be the checker silently doing nothing.

Usage:
  python3 tools/check_reloc_model.py [scratch/wad/CD.HED scratch/wad/CD.WAD]
  python3 tools/check_reloc_model.py --selftest
Exit: 0 all assumptions hold; 1 a violation or a missing corpus.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import extract_modules  # noqa: E402  — parse_index, the RE'd CD.HED walk

R_32, R_HI16, R_LO16, R_26 = 0, 1, 2, 3
TYPE_NAME = {R_32: "R_MIPS_32", R_HI16: "R_MIPS_HI16", R_LO16: "R_MIPS_LO16", R_26: "R_MIPS_26"}

# A2's backward scan is UNBOUNDED within the module image. It has to be: the measured maximum
# distance between a HI16 `lui` and one of its LO16 partners in this game is 803 instructions — the
# compiler hoists a module-data pointer into a callee-saved register at function entry and the
# partners are spread over the whole body. A 64-instruction window reported 52 false UNPAIRED sites.
# The scan also follows register-to-register MOVES (`addu rd,rs,$zero` and friends), because the
# hoisted pointer is frequently copied between callee-saved registers before use.
MOVE_RTYPE = {0x21, 0x20, 0x25}   # addu, add, or  — with rt == $zero these are `move rd, rs`

# opcode (bits 31..26) -> mnemonic, for the instructions a relocation site can legally be.
OPC = {0x0F: "lui", 0x09: "addiu", 0x0D: "ori", 0x08: "addi",
       0x23: "lw", 0x2B: "sw", 0x20: "lb", 0x24: "lbu", 0x21: "lh", 0x25: "lhu",
       0x22: "lwl", 0x26: "lwr", 0x2A: "swl", 0x2E: "swr", 0x28: "sb", 0x29: "sh",
       0x31: "lwc2", 0x39: "swc2",
       0x02: "j", 0x03: "jal"}
LO16_OK = {"addiu", "ori", "addi", "lw", "sw", "lb", "lbu", "lh", "lhu",
           "lwl", "lwr", "swl", "swr", "sb", "sh", "lwc2", "swc2"}


def decode(w):
    """Just enough of the MIPS I-format to answer this script's questions."""
    op = OPC.get((w >> 26) & 0x3F)
    return op, (w >> 21) & 0x1F, (w >> 16) & 0x1F      # mnemonic, rs, rt


def parse_rel(rel: bytes):
    """The .rel stream -> [(type, offset, addend_or_None)], in stream order.

    Mirrors extract_modules.relocate()'s walk exactly — offsets are ABSOLUTE (`w & ~3`), never
    cumulative. Re-deriving them as cumulative is a mistake that has already been made once on this
    project and it reported 3037 false offenders (docs/info/claims.md C012).
    """
    out, i = [], 0
    while True:
        if i + 4 > len(rel):
            raise ValueError("relocation stream ran off the end without a 0xFFFFFFFF terminator")
        (w,) = struct.unpack_from("<I", rel, i)
        i += 4
        if w == 0xFFFFFFFF:
            break
        t, off = w & 3, w & ~3
        addend = None
        if t == R_HI16:
            if i + 4 > len(rel):
                raise ValueError("HI16 relocation is missing its addend word")
            (addend,) = struct.unpack_from("<I", rel, i)
            i += 4
        out.append((t, off, addend))
    return out, i


class Report:
    def __init__(self):
        self.sites = {t: 0 for t in TYPE_NAME}     # denominators
        self.violations = []                       # (module, kind, detail) — hard failures
        self.unpaired = []                         # (module, offset) — A2 scan found no lui
        self.tail = []                             # (module, unread_bytes)
        self.modules = 0
        self.max_pair_dist = 0

    def check_module(self, name, img: bytes, rel: bytes):
        self.modules += 1
        sites, consumed = parse_rel(rel)
        if consumed != len(rel):
            self.tail.append((name, len(rel) - consumed))

        by_off = {}
        hi_lui_regs = {}       # offset -> rt of a HI16 lui, for A2's pairing scan
        for t, off, _a in sites:
            self.sites[t] += 1
            if off in by_off:
                self.violations.append((name, "A4", f"+0x{off:X} named twice "
                                                    f"({TYPE_NAME[by_off[off]]} and {TYPE_NAME[t]})"))
            by_off[off] = t
            if off + 4 > len(img):
                self.violations.append((name, "A4", f"+0x{off:X} is past the {len(img)}-byte image"))
                continue
            (w,) = struct.unpack_from("<I", img, off)
            op, _rs, rt = decode(w)
            if t == R_HI16:
                if op != "lui":
                    self.violations.append((name, "A1", f"+0x{off:X} is {op or 'undecoded'}, not lui"))
                else:
                    hi_lui_regs[off] = rt
            elif t == R_LO16:
                if op not in LO16_OK:
                    self.violations.append((name, "A2", f"+0x{off:X} is {op or 'undecoded'}, "
                                                        f"not an address-low-half instruction"))
            elif t == R_26:
                if op not in ("j", "jal"):
                    self.violations.append((name, "A3", f"+0x{off:X} is {op or 'undecoded'}, not j/jal"))

        # A2's pairing scan, run only over LO16 sites that passed the shape check.
        for t, off, _a in sites:
            if t != R_LO16 or off + 4 > len(img):
                continue
            (w,) = struct.unpack_from("<I", img, off)
            op, rs, _rt = decode(w)
            if op not in LO16_OK:
                continue
            want = {rs}
            found = False
            for b in range(off - 4, -4, -4):
                if b < 0:
                    break
                if hi_lui_regs.get(b) in want:
                    found = True
                    self.max_pair_dist = max(self.max_pair_dist, (off - b) // 4)
                    break
                (bw,) = struct.unpack_from("<I", img, b)
                opc, brs, brt, brd = (bw >> 26) & 0x3F, (bw >> 21) & 0x1F, (bw >> 16) & 0x1F, (bw >> 11) & 0x1F
                if opc == 0 and (bw & 0x3F) in MOVE_RTYPE and brt == 0 and brd in want:
                    want.add(brs)                      # `move rd, rs` — chase the source too
                elif opc == 0x09 and (bw & 0xFFFF) == 0 and brt in want:
                    want.add(brs)                      # `addiu rt, rs, 0` — the I-type move
            if not found:
                self.unpaired.append((name, off))

    def emit(self):
        total = sum(self.sites.values())
        print(f"[reloc-model] examined {self.modules} module(s), {total} relocation sites:")
        for t in sorted(TYPE_NAME):
            print(f"[reloc-model]   {TYPE_NAME[t]:<12} {self.sites[t]}")
        if self.modules == 0:
            print("[reloc-model] FAIL: examined NOTHING — there is no corpus here, so nothing was checked.")
            return 1
        print(f"[reloc-model] A1 lui-shape checked over {self.sites[R_HI16]} HI16 sites")
        print(f"[reloc-model] A2 low-half shape + lui pairing checked over {self.sites[R_LO16]} LO16 "
              f"sites (unbounded backward scan following register moves; furthest real pair found "
              f"{self.max_pair_dist} instructions apart — the scan is NOT flow-sensitive, so it "
              f"proves a relocated `lui` for that register EXISTS earlier, not that it dominates)")
        print(f"[reloc-model] A3 j/jal shape checked over {self.sites[R_26]} J26 sites")
        print(f"[reloc-model] A4 duplicate/out-of-range offsets checked over all {total} sites")
        print(f"[reloc-model] A5 stream termination checked over {self.modules} .rel files")
        print(f"[reloc-model] NOT CHECKED: whether an R_MIPS_32 site lands inside code the recompiler "
              f"decodes — that needs the emitter's function set, not the .rel stream "
              f"({self.sites[R_32]} type-0 sites unexamined for that property)")

        rc = 0
        if self.tail:
            rc = 1
            for name, n in self.tail:
                print(f"[reloc-model] A5 VIOLATION {name}: {n} bytes after the terminator")
        if self.unpaired:
            print(f"[reloc-model] A2 UNPAIRED: {len(self.unpaired)} LO16 site(s) with no HI16 `lui` "
                  f"anywhere earlier in the image feeding their base register:")
            for name, off in self.unpaired[:20]:
                print(f"[reloc-model]     {name} +0x{off:X}")
            if len(self.unpaired) > 20:
                print(f"[reloc-model]     ... and {len(self.unpaired) - 20} more")
            rc = 1
        if self.violations:
            print(f"[reloc-model] {len(self.violations)} VIOLATION(S):")
            for name, kind, detail in self.violations[:40]:
                print(f"[reloc-model]     {kind} {name}: {detail}")
            if len(self.violations) > 40:
                print(f"[reloc-model]     ... and {len(self.violations) - 40} more")
            rc = 1
        if rc == 0:
            print("[reloc-model] OK — every assumption above holds on every site examined.")
        return rc


def selftest():
    """Feed cases that MUST trip each check. A checker that cannot go red proves nothing."""
    def mod(words, relwords):
        img = b"".join(struct.pack("<I", w) for w in words)
        rel = b"".join(struct.pack("<I", w) for w in relwords) + b"\xff\xff\xff\xff"
        return img, rel

    LUI_V0 = 0x3C020000        # lui $v0, 0
    ADDIU_V0_V0 = 0x24420000   # addiu $v0, $v0, 0
    NOP = 0x00000000
    JAL = 0x0C000000
    ok = True

    # POSITIVE control: a well-formed hi/lo pair + a jal must pass all checks clean.
    r = Report()
    img, rel = mod([LUI_V0, ADDIU_V0_V0, JAL], [0 | R_HI16, 0, 4 | R_LO16, 8 | R_26])
    r.check_module("good", img, rel)
    if r.violations or r.unpaired:
        print(f"[selftest] FAIL: the well-formed case was flagged: {r.violations} {r.unpaired}")
        ok = False

    cases = [
        ("A1", mod([NOP], [0 | R_HI16, 0])),                       # HI16 on a non-lui
        ("A3", mod([NOP], [0 | R_26])),                            # J26 on a non-jump
        ("A2", mod([NOP], [0 | R_LO16])),                          # LO16 on a non-immediate op
        ("A4", mod([LUI_V0], [0 | R_HI16, 0, 0 | R_LO16])),        # same offset named twice
    ]
    for kind, (img, rel) in cases:
        r = Report()
        r.check_module("bad", img, rel)
        if not any(v[1] == kind for v in r.violations):
            print(f"[selftest] FAIL: {kind} did not fire on a case that must trip it: {r.violations}")
            ok = False

    # A2 UNPAIRED: a LO16 whose base register no HI16 lui defines.
    r = Report()
    img, rel = mod([ADDIU_V0_V0], [0 | R_LO16])
    r.check_module("orphan", img, rel)
    if not r.unpaired:
        print("[selftest] FAIL: an orphan LO16 was not reported UNPAIRED")
        ok = False

    print(f"[selftest] {'PASS — every check fires on a case that must trip it' if ok else 'FAILED'}")
    return 0 if ok else 1


def main():
    if "--selftest" in sys.argv:
        return selftest()
    hed = sys.argv[1] if len(sys.argv) > 2 else os.path.join(ROOT, "scratch/wad/CD.HED")
    wad = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "scratch/wad/CD.WAD")
    for p in (hed, wad):
        if not os.path.isfile(p):
            print(f"[reloc-model] FAIL: {p} does not exist — NOTHING was checked. Run "
                  f"tools/ensure_recomp.py first so the archive is extracted.")
            return 1
    index = extract_modules.parse_index(open(hed, "rb").read())
    stems = sorted({n[:-4] for n in index if n.endswith(".bin")} &
                   {n[:-4] for n in index if n.endswith(".rel")})
    if not stems:
        print(f"[reloc-model] FAIL: CD.HED has {len(index)} entries but no .bin/.rel pair — "
              f"NOTHING was checked.")
        return 1
    rep = Report()
    with open(wad, "rb") as f:
        def grab(name):
            off, size = index[name]
            f.seek(off)
            return f.read(size)
        for stem in stems:
            rep.check_module(stem, grab(f"{stem}.bin"), grab(f"{stem}.rel"))
    return rep.emit()


if __name__ == "__main__":
    sys.exit(main())
