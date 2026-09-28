#!/usr/bin/env python3
"""re_cd_stream.py — recover SLUS_008.75's CD-stream service into named, byte-gated constants.

WHAT THIS IS. The black-picture frontier (docs/issues/0024, 0025, 0026, 0031) had three
sessions of confident measurement landing on a wrong owner, twice, and the workspace map itself
carried the wrong shape. This tool settles the shape from the executable's bytes and emits it as
the data `titles/spiderman1/spider1_cd_stream.*` names, so the recovery is TEXT that can be
diffed, reviewed and gated rather than a claim inside a prose issue.

THE SHAPE IT ESTABLISHES, and why the previous description was wrong in a way that mattered:

  * `0x8008C3E0` is NOT a CD-ROM interrupt handler. It is reached by exactly FOUR `jal` sites
    (0x8008CAAC, 0x8008CD2C, 0x8008D188, 0x8008DA58) and by no reference of any other form --
    not one `jalr` through the interrupt chain, not one stored pointer anywhere in the file.
    Its own body contains no `jalr` and no branch back into it. So nothing routes an IRQ to it.
  * It is therefore a POLLED service. Each of the four call sites is a `do { r = 0x8008C3E0(); }
    while (r)` poll loop whose loop-carried register is tested against bits 4 and 2 -- the two
    bits the routine actually sets. The workspace map's "the guest's routine 0x8008C3E0 ...
    dispatches through a table at 0x80096670, and calls *[0x800B3B18]" is TRUE of the body and
    FALSE of the reason: the body dispatches through 0x80096670 and the CALLERS call
    *[0x800B3B18]. `0x8008C3E0` itself never calls it.

  That is the root cause in one sentence: **the title never learns that a sector arrived, because
  the only code that would tell it is a poll the port never runs, sitting behind a gate word that
  is zero.**

CORRECTION 2026-09-29, WHICH REMOVES THE SECOND HALF OF THAT SENTENCE. An independent writer census
over the same 186,880 words found that NOTHING EVER SETS THE GATE:

  * every store whose byte range covers 0x800B2886 -- direct `sh`/`sw`/`sb` at $at-relative
    offsets 0x2880..0x2888, stores through any lui/addiu/ori-materialised pointer, stores through
    a register of any other provenance -- is EXACTLY ONE instruction, the clearing
    `sh $zero, 0x2886($at)` at 0x8008BBAC. There is no setter.
  * there are THREE `jal 0x8008B900` sites in the image, not four: 0x8008CA84, 0x8008CD04,
    0x8008D160. Each is followed 8 bytes later by `beq $v0, $zero` whose target is past the
    service call it skips. The fourth call site, 0x8008DA58, has no gate reader within 1,200 words.

So the three gated loops are DEAD CODE IN RETAIL, the gate is not a cause this port may open, and
"0x800B2886 == 0" is a vacuous zero of the same family as the `is3d` counter and the VSync(0)
census this image already paid for twice. `assert_gate_census` below is what keeps the tool from
regressing to the first draft's claim.

THE TWO GATES, both measured, and both naming why the poll does not run:

THE CALLBACK ABI, which is a second correction and the one the framework's comment got wrong.
THE CALLBACK ABI, which is a second correction and the one the framework's comment got wrong.
`cd_ready_delivery.cpp` passes `a0 = 1` and `a1 = 0` and asserts, in a comment, that
"the two functions its CdInit installs into this slot, and libstr's replacement for it during a
stream, read NEITHER argument". That is TRUE of 0x8008A260 and 0x800860B4, and it is FALSE of the
ABI the four poll loops actually build, because they do not call the function -- they call
whatever is IN the slot, and the slot is libstr's `StGetNext` consumer by then. The recovered
call shape at all four sites is `callback(status_byte_from_0x800C6384, ptr_0x800C6384_or_0x800C637C)`,
i.e. a0 is a value read from guest RAM and a1 is a POINTER to a guest buffer. Passing 0 for a1
hands the consumer a null pointer. The framework comment's own caveat -- "the layout declares the
slot and not the guest's private buffer" -- is the admission that it cannot know a1, and it
resolves that admission by passing 0.

Usage:
    uv run --frozen python tools/re_cd_stream.py report
    uv run --frozen python tools/re_cd_stream.py --selftest
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from probe_mips_image import branch_target, decode, jump_target  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
HEADER = 0x800  # PS-X EXE header; the text segment starts right after it
DEFAULT_IMAGE = ROOT / "scratch" / "assets" / "spiderman1" / "SLUS_008.75"
"""Resolved against the REPOSITORY, never against the process's working directory.

CTest runs a test with the build directory as its working directory, so a relative default would
make every registered `spider1_cd_stream*` case refuse with "not found" on a tree whose image is
provisioned and correct. That is the same class of failure as the ones this repository keeps
recording: a gate that reports the wrong subject. The other registered RE tools here resolve the
same way; see tools/re_cadence.py.
"""

# ---- the recovered model. Every value here is asserted against bytes by `verify()`. ----------

CD_SERVICE = 0x8008C3E0
"""The polled CD-ROM service. NOT an interrupt handler -- see the module docstring."""

# The u16 read by the three DEAD poll loops. `0x8008B900` reads it; each of those three loops then
# does `beq $v0, $zero, <skip the whole service>`. It is 0 in the image and NOTHING ever sets it.
POLL_GATE = 0x800B2886
"""u16. Read by `0x8008B900`; ZERO in the image with NO SETTER -- see the module docstring."""

POLL_GATE_READER = 0x8008B900
"""`lhu $v0, 0x2886($v0)`. Called by exactly THREE poll loops: 0x8008CA84, 0x8008CD04, 0x8008D160."""

POLL_GATE_SITES = (0x8008CA84, 0x8008CD04, 0x8008D160)
"""Every `jal 0x8008B900` in the image, in address order. Three, not four."""

POLL_GATE_CLEARER = 0x8008BBAC
"""`sh $zero, 0x2886($at)`. The image's ONLY store covering 0x800B2886. There is no setter."""

POLL_SITES = (0x8008CAAC, 0x8008CD2C, 0x8008D188, 0x8008DA58)
"""The four and only `jal CD_SERVICE` sites, in address order."""

RESPONSE_TYPE_REGISTER = 0x1F801803
"""Bank-1 interrupt-flag register. Bits 0..2 are the pending response type."""

RESPONSE_FIFO_REGISTER = 0x1F801801
"""Bank-1 response FIFO. The service drains up to 8 bytes from it."""

RESPONSE_TYPE_MASK = 0x07
"""`andi $v0, $v0, 7` at 0x8008C414. Three bits: five response types."""

RESPONSE_TYPE_NONE = 0
"""Type 0 means "no response", and the service returns 0 for it."""

RESPONSE_LENGTH = 8
"""`slti $v0, $s0, 8` at 0x8008C494. Eight payload bytes per response."""

RESPONSE_QUEUE_SLOTS = RESPONSE_LENGTH
"""The response queue is eight bytes wide, so eight bytes is a whole response."""

# The dispatch table at 0x80096670, read as `lw $v0, 0x6670($at)` with `at = 0x80090000 + (type-1)*4`
# at 0x8008C62C..0x8008C634. Five entries for response types 1..5, indexed by `type-1`, so table
# entry 0 serves response type 1.
#
# The index arithmetic is `addiu $v1, $v0, -1` (0x8008C61C) then `sltiu $v0, $v1, 5` (0x8008C620),
# and the entry load is `lw $v0, 0x6670($at)` with `at = 0x80090000 + (type-1)*4`. So the table is
# read in type order, and the FIVE read words are 0x8008C790, 0x8008C744, 0x8008C644, 0x8008C810,
# 0x8008C890. That is what the tool asserts, and the first draft of this file asserted the arms in
# a different order and the tool went red on its own author -- which is the reason every constant
# here is machine-checked rather than transcribed.
DISPATCH_TABLE = 0x80096670
DISPATCH_FIRST_TYPE = 1
DISPATCH_LAST_TYPE = 5
DISPATCH_TYPE_COUNT = DISPATCH_LAST_TYPE - DISPATCH_FIRST_TYPE + 1

# Each arm receives `s0` = the drained byte count and `s1` = response byte 0 & 0x1D, and returns a
# bitmask in `v0`. Bit 2 (4) and bit 1 (2) are the two the poll loops test.
DISPATCH_ARMS = (
    (1, 0x8008C790),  # data ready
    (2, 0x8008C744),  # sector buffer ready
    (3, 0x8008C644),  # command acknowledge
    (4, 0x8008C810),  # read state
    (5, 0x8008C890),  # error
)

# The poll result bits. Both are read by `andi $v0, $s0, 4` / `andi $v0, $s0, 2` at every poll site.
RESULT_DATA_READY = 0x04
RESULT_COMMAND_ACK = 0x02

# The bitmask the guest accumulates and the loop tests against a zero/non-zero result. This is the
# `s1` carry: `andi $s1, $v0, 0x001D` at 0x8008C580.
RESULT_ACCUMULATOR_MASK = 0x001D

# The three guest RAM words the service writes, read from the `sw $v0, 0x3Bxx($at)` sites.
CD_STATUS_WORD = 0x800B3B20
"""First response byte, whole. `sw $v0, 0x3B20($at)` at 0x8008C588."""

CD_STATUS_WORD_ALT = 0x800B3B24
"""Second response byte. `sw $v1, 0x3B24($at)` at 0x8008C590."""

CD_ERROR_COUNT = 0x800B3B28
"""`sw $v0, 0x3B28($at)` at 0x8008C570, after `addiu $v0, $v0, 1` -- an error counter."""

CD_COMMAND_COUNT = 0x800B3B1C
"""`lw $v0, 0x3B1C($v0)` at 0x8008C5A8, compared with `slti $v0, $v0, 3`."""

# The two callback slots. The ready slot is the framework's declared seam at 0x800B3B18; the sync
# slot at 0x800B3B14 is its sibling and the two are called adjacently at every poll site.
CD_READY_CALLBACK_SLOT = 0x800B3B18
CD_SYNC_CALLBACK_SLOT = 0x800B3B14

# The guest RAM buffer the poll loops pass as `a1`. Read from `addiu $a1, $a1, 25476` (0x800C6384)
# and `addiu $a1, $a1, 25468` (0x800C637C) in the delay slots of the two `jalr`s.
CD_READY_BUFFER = 0x800C6384
CD_SYNC_BUFFER = 0x800C637C

# The status byte the poll loops pass as `a0`: `lbu $a0, 0($s5)` / `lbu $a0, 0($s2)`, with
# `s5 = s2 + 1` (`addiu $s5, $s2, 1` at 0x8008C998) and `s2 = 0x800B3DF0` (0x8008C994).
CD_STATUS_BYTE = 0x800B3DF0
CD_STATUS_BYTE_ALT = 0x800B3DF1

# The interrupt-chain head the guest writes its element onto. Issue 0026 recorded 0x800C1528 as the
# element itself; 0x800C1520 is what 0x8008DC14 stores INTO, and 0x8008DC14 is reached from the
# guest's own interrupt registration, so 0x800C1520 is the head pointer.
CD_IRQ_CHAIN_HEAD = 0x800C1520
CD_IRQ_CHAIN_ELEMENT = 0x800C1528


@dataclass(frozen=True)
class Image:
    data: bytes
    text_addr: int
    text_size: int
    words: tuple[int, ...]

    def word(self, addr: int) -> int:
        off = addr - self.text_addr
        if off < 0 or off + 4 > self.text_size:
            raise ValueError(f"0x{addr:08X} is outside the text segment")
        return self.words[off // 4]

    def half(self, addr: int) -> int:
        """Read an ALIGNED-TO-2 halfword.

        This must select the containing WORD and then shift, not mask the low half of
        `word(addr)`. The gate this tool exists to measure lives at 0x800B2886, which is 2 mod 4:
        `word(0x800B2886) & 0xFFFF` truncates to the containing word 0x800B2884 and returns ITS low
        half -- the half at 0x800B2884, one word away from the byte under test. The first draft of
        this tool did exactly that, the gate assertion read 0 for the wrong reason, and the
        selftest's non-zero-gate mutation went green because it could not move the byte the check
        was really looking at. A diagnostic that is green for the wrong reason is the failure mode
        this repository keeps paying for, so the correction is in the reader and the selftest
        catches it.
        """
        return (self.word(addr & ~0x3) >> ((addr & 0x3) * 8)) & 0xFFFF


def load(path: Path) -> Image:
    data = path.read_bytes()
    if data[:8] != b"PS-X EXE":
        raise ValueError(f"{path} is not a PS-X EXE; nothing was scanned")
    _pc, _gp, t_addr, t_size = struct.unpack_from("<4I", data, 0x10)
    words = tuple(struct.unpack_from("<I", data, HEADER + 4 * i)[0] for i in range(t_size // 4))
    return Image(data, t_addr, t_size, words)


def is_call_to(word: int, pc: int, target: int) -> bool:
    """True for a DIRECT `jal target` only. A `jalr` is deliberately not accepted: this image
    reaches its library routines through `jalr $ra,$vN`, so accepting indirect forms here would
    make the site count a property of the scan rather than of the image -- the exact trap
    `docs/issues/0030` records this image already paid for once with VSync."""
    return (word >> 26) == 3 and jump_target(pc, word) == target


def direct_call_sites(img: Image, target: int) -> list[int]:
    return [
        img.text_addr + 4 * i
        for i, word in enumerate(img.words)
        if is_call_to(word, img.text_addr + 4 * i, target)
    ]


def any_reference_forms(img: Image, target: int) -> dict[str, int]:
    """Every OTHER way the image could reach `target`, so "four call sites" is a denominator-backed
    claim rather than the absence of a scan. Reported, never used as a gate."""
    hi, lo = (target >> 16) & 0xFFFF, target & 0xFFFF
    lui_sites, stored, jumps = 0, 0, 0
    for i, word in enumerate(img.words):
        pc = img.text_addr + 4 * i
        if (word >> 26) == 0x0F and (word & 0xFFFF) == hi:
            reg = (word >> 16) & 0x1F
            for k in range(i + 1, min(i + 10, len(img.words))):
                ins = decode(img.text_addr + 4 * k, img.words[k])
                if ins is None:
                    continue
                _op, rs, _rt, _rd, _sh, _fn = ins.fields
                if rs == reg and ins.mnemonic in ("addiu", "sw", "sh", "sb", "lw", "lhu", "jr", "jalr"):
                    if ins.mnemonic in ("sw", "sh", "sb") and (img.words[k] & 0xFFFF) == lo:
                        stored += 1
                    elif ins.mnemonic in ("jr", "jalr") and rs == reg:
                        # Only a jump THROUGH the materialised address is a reference. A `jr` on the
                        # lui register alone is not, and counting it would be a false positive.
                        jumps += 1
                    elif ins.mnemonic == "addiu" and (img.words[k] & 0xFFFF) == lo:
                        lui_sites += 1
    raw = struct.pack("<I", target)
    return {
        "lui+addiu": lui_sites,
        "stored_pointers": stored,
        "materialised_jumps": jumps,
        "raw_literal_in_file": img.data.count(raw),
    }


def verify(img: Image) -> list[tuple[bool, str]]:
    """Every recovered constant, checked against bytes. Returns (ok, message) per assertion."""
    out: list[tuple[bool, str]] = []

    def check(ok: bool, what: str) -> None:
        out.append((ok, what))

    # The service body's first and last words, and the two register reads that name its inputs.
    check(img.word(CD_SERVICE) == 0x27BDFFD0,
          f"0x{CD_SERVICE:08X} opens with addiu $sp,$sp,-48 (0x27BDFFD0)")
    check(img.word(0x8008C400) == 0x3C04800B and img.word(0x8008C404) == 0x8C843DE4,
          f"0x8008C400 loads the response-type register pointer 0x{RESPONSE_TYPE_REGISTER:08X}")
    check(img.word(0x8008C414) == 0x30420007,
          "0x8008C414 masks the response type with 7")
    check(img.word(0x8008C4EC) == 0x8C423DE0,
          f"0x8008C4EC loads the response FIFO pointer 0x{RESPONSE_FIFO_REGISTER:08X}")
    check(img.word(0x8008C494) == 0x2A020008,
          "0x8008C494 bounds the drain loop at 8 bytes")

    # The dispatch table, read as the five words at 0x80096670..0x80096680.
    table = [img.word(DISPATCH_TABLE + 4 * i) for i in range(DISPATCH_TYPE_COUNT)]
    expect = [arm for _t, arm in DISPATCH_ARMS]
    check(table == expect,
          f"0x{DISPATCH_TABLE:08X} holds the five recovered arms "
          f"{[hex(a) for a in expect]} (read {[hex(t) for t in table]})")

    # The index arithmetic that selects into it: `sltiu $v0, $v1, 5` bounds the type to 1..5.
    check(img.word(0x8008C620) == 0x2C620005,
          "0x8008C620 bounds the dispatch type to 1..5 with sltiu $v1, 5")
    check(img.word(0x8008C634) == 0x8C226670,
          f"0x8008C634 loads the dispatch entry at +0x6670 = 0x{DISPATCH_TABLE:08X}")

    # The two result bits, read at the four poll sites.
    for site in POLL_SITES:
        body = [img.word(site + 4 * k) for k in range(14)]
        text = " ".join(f"{w:08X}" for w in body)
        check("30420004" in text or "32020004" in text,
              f"poll site 0x{site:08X} tests the data-ready bit with andi ...,4")
        check("32020002" in text or "30420002" in text,
              f"poll site 0x{site:08X} tests the command-ack bit with andi ...,2")

    # The status byte and the two buffers, at the a0/a1 argument setup of every poll site.
    for site in POLL_SITES:
        body = " ".join(f"{img.word(site + 4 * k):08X}" for k in range(28))
        check("24A56384" in body, f"poll site 0x{site:08X} passes buffer 0x{CD_READY_BUFFER:08X}")
        check("24A5637C" in body, f"poll site 0x{site:08X} passes buffer 0x{CD_SYNC_BUFFER:08X}")

    # The four call sites are exactly the four, and there is no fifth form of reference.
    sites = direct_call_sites(img, CD_SERVICE)
    check(tuple(sites) == POLL_SITES,
          f"0x{CD_SERVICE:08X} has exactly {len(POLL_SITES)} direct jal sites "
          f"{[hex(s) for s in sites]}")

    # The gate. `lhu $v0, 0x2886($v0)` and the two clearing stores.
    check(img.word(POLL_GATE_READER) == 0x3C02800B and img.word(POLL_GATE_READER + 4) == 0x94422886,
          f"0x{POLL_GATE_READER:08X} reads the u16 gate at 0x{POLL_GATE:08X}")
    check(img.half(POLL_GATE) == 0,
          f"gate 0x{POLL_GATE:08X} is {img.half(POLL_GATE)} in the image (the service is skipped)")

    # THE GATE CENSUS, which is the assertion the first draft of this file was missing and which
    # is the reason its conclusion was wrong. Three facts, each of which can go red:
    #   * exactly the three named `jal 0x8008B900` sites exist,
    #   * each is followed 8 bytes later by `beq $v0, $zero`,
    #   * the image's ONLY store whose byte range covers the gate is the named clearer.
    out.extend(assert_gate_census(img))

    # The accumulator mask.
    check(img.word(0x8008C580) == 0x3051001D,
          "0x8008C580 accumulates response byte 0 & 0x1D")

    return out


def store_covering(img: Image, target: int) -> list[int]:
    """Every store instruction whose BYTE RANGE covers `target`.

    Wider than a scan for `sh` at exactly this offset on purpose: a `sw` at target-2 would also
    clear it, and a scan that only looked for the one form the answer happens to take would be a
    scan that agrees with itself.

    The base register is resolved by walking BACKWARD from each store through the standard
    lui/addiu pair, and the walk ABORTS on anything that redefines the register. That abort is the
    whole difficulty and it is not optional. A forward "remember every register a `lui` ever wrote"
    map is the obvious implementation and it is WRONG in a way that invents a writer: the first
    draft of this function kept a stale `lui $s1, 0x800B` value across a `jal` and then reported
    `sh $v0, 0x0002($s1)` at 0x8008BA68 as a second writer of the gate. That instruction is in an
    error path -- the instruction before it is `jal 0x8000710C` with a format string, and `$s1` is
    the function's pointer argument, read as `lhu $v1, 0x30($s1)` two instructions earlier. A
    diagnostic that reports a phantom setter is worse than one that reports none, because it turns
    a measured absence into an unmeasured presence.
    """
    sizes = {0x28: 1, 0x29: 2, 0x2B: 4}
    at_base = 0x800B0000  # $at holds 0x800B____ throughout this driver's data accesses
    hits: list[int] = []
    for i, word in enumerate(img.words):
        op, rs = word >> 26, (word >> 21) & 31
        if op not in sizes:
            continue
        imm = word & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        # (a) $at-relative: the form this driver's own code uses.
        if rs == 1:
            low, high = at_base + simm, at_base + simm + sizes[op]
            if low <= target < high:
                hits.append(img.text_addr + 4 * i)
                continue
        # (b) a pointer built by a lui/addiu pair in the instructions immediately above.
        value: int | None = None
        expect = rs
        for k in range(i - 1, max(i - 7, -1), -1):
            back = img.words[k]
            bop, brs, brt = back >> 26, (back >> 21) & 31, (back >> 16) & 31
            bimm = back & 0xFFFF
            bsimm = bimm - 0x10000 if bimm & 0x8000 else bimm
            if bop == 0x0F and brt == expect:          # lui rt, hi
                value = bimm << 16
            elif bop in (0x09, 0x0D) and brt == expect and brs == expect:  # addiu/ori rt, rt, lo
                if value is None:
                    break
                value = (value + (bsimm if bop == 0x09 else bimm)) & 0xFFFFFFFF
            else:
                break                                  # anything else redefines it; stop
        if value is not None:
            low = (value + simm) & 0xFFFFFFFF
            if low <= target < low + sizes[op]:
                hits.append(img.text_addr + 4 * i)
    return hits


def assert_gate_census(img: Image) -> list[tuple[bool, str]]:
    """The three facts that make the gate a DEAD TAP rather than a cause. Each is separately
    falsifiable, so the conclusion cannot survive a change to the image."""
    out: list[tuple[bool, str]] = []

    sites = tuple(t for t in direct_call_sites(img, POLL_GATE_READER))
    out.append((sites == POLL_GATE_SITES,
                f"0x{POLL_GATE_READER:08X} is called from exactly {len(POLL_GATE_SITES)} sites "
                f"{[hex(s) for s in POLL_GATE_SITES]} (read {[hex(s) for s in sites]}), so "
                f"{len(POLL_SITES) - len(sites)} of the {len(POLL_SITES)} service call sites are "
                f"UNGATED and do run in retail"))

    for site in POLL_GATE_SITES:
        # The gate branch is EIGHT bytes after the call: `jal` + delay slot, then `beq` + delay
        # slot. `beq $v0, $zero` is rs=$v0(2), rt=$zero(0) -- getting those two the wrong way
        # round is a green assertion that never fires, so the operands are spelled out here.
        nxt = img.word(site + 8)
        branches = (nxt >> 26) == 4 and ((nxt >> 21) & 31) == 2 and ((nxt >> 16) & 31) == 0
        imm = nxt & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        target = (site + 12 + simm * 4) & 0xFFFFFFFF
        # The substantive half: the target is PAST a service call, so the branch really does skip
        # the whole service rather than merely testing a flag.
        skips = any(site < call < target for call in POLL_SITES)
        out.append((branches and skips,
                    f"0x{site:08X} is followed by `beq $v0, $zero` (0x{nxt:08X}) branching to "
                    f"0x{target:08X}, which is PAST a service call -- so it skips the whole service"))

    writers = store_covering(img, POLL_GATE)
    out.append((writers == [POLL_GATE_CLEARER],
                f"the image's only store covering 0x{POLL_GATE:08X} is the clearer at "
                f"0x{POLL_GATE_CLEARER:08X} (read {[hex(w) for w in writers]}) -- there is NO "
                f"SETTER, so the gate is a dead tap and the loops behind it are retail-dead code"))
    return out


def report(img: Image) -> int:
    print("Spider-Man 1 CD-stream recovery -- SLUS_008.75 bytes\n")
    print(f"[image]     {img.text_size} text bytes at 0x{img.text_addr:08X} "
          f"({img.text_size // 4} words)")
    print(f"[denominator] words scanned {len(img.words)}")
    sites = direct_call_sites(img, CD_SERVICE)
    print(f"[call form]  direct `jal 0x{CD_SERVICE:08X}` sites: {len(sites)} "
          f"{[hex(s) for s in sites]}")
    forms = any_reference_forms(img, CD_SERVICE)
    print(f"[other forms] {forms}")
    print(f"[gate]      u16 0x{POLL_GATE:08X} = {img.half(POLL_GATE)} "
          f"(read by 0x{POLL_GATE_READER:08X}, which {len(POLL_GATE_SITES)} of the "
          f"{len(POLL_SITES)} poll loops call first)")
    print(f"[gate census] stores covering the gate: "
          f"{[hex(w) for w in store_covering(img, POLL_GATE)]} -- a clearer and NO setter")
    print(f"[dispatch]  table 0x{DISPATCH_TABLE:08X}, "
          f"{DISPATCH_TYPE_COUNT} arms for response types "
          f"{DISPATCH_FIRST_TYPE}..{DISPATCH_LAST_TYPE}")
    for t, arm in DISPATCH_ARMS:
        print(f"              type {t} -> 0x{arm:08X}")
    print(f"[status]    byte 0x{CD_STATUS_BYTE:08X}, "
          f"word 0x{CD_STATUS_WORD:08X}, count 0x{CD_ERROR_COUNT:08X}")
    print(f"[callbacks] ready 0x{CD_READY_CALLBACK_SLOT:08X} -> buffer 0x{CD_READY_BUFFER:08X}")
    print(f"            sync  0x{CD_SYNC_CALLBACK_SLOT:08X} -> buffer 0x{CD_SYNC_BUFFER:08X}")
    print()
    failures = 0
    for ok, msg in verify(img):
        print(f"  {'ok  ' if ok else 'FAIL'}  {msg}")
        if not ok:
            failures += 1
    print(f"\n{'all recovered constants match the image' if not failures else f'{failures} FAILED'}")
    return 1 if failures else 0


def selftest(img: Image) -> int:
    """Cases that can go red. Each one breaks ONE recovered fact and asserts the report goes red
    for that fact and not for the others -- so a tool that always fails is distinguishable from a
    tool that is measuring."""
    base = verify(img)
    base_fail = {m for ok, m in base if not ok}
    cases: list[tuple[str, callable, str]] = []

    def broken_dispatch(_img: Image) -> Image:
        # Slot 3, which currently holds 0x8008C810. Overwriting it with slot 0's value makes the
        # table a genuine duplicate rather than a no-op -- the first draft of this selftest
        # overwrote slot 2 with slot 2's own value and the case passed for the wrong reason.
        w = list(_img.words)
        off = (DISPATCH_TABLE + 4 * 3 - _img.text_addr) // 4
        w[off] = 0x8008C790
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_gate(_img: Image) -> Image:
        # The gate is a HALFWORD inside the aligned word at 0x800B2884, so the mutation has to
        # address the containing word. Indexing by the halfword's own address truncates and would
        # silently edit the wrong word -- which is the kind of selftest case that goes green for
        # no reason, so the containing-word form is used explicitly.
        w = list(_img.words)
        word_addr = POLL_GATE & ~0x3
        off = (word_addr - _img.text_addr) // 4
        shift = (POLL_GATE - word_addr) * 8
        w[off] = (w[off] & ~(0xFFFF << shift)) | (0x0001 << shift)
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_gate_setter(_img: Image) -> Image:
        """Add a SECOND, non-zero store covering the gate, and require the census to go red.

        Without this case the census is unfalsifiable in the one direction that matters: a tool that
        cannot see a setter appearing cannot claim there is none. The mutation overwrites a delay-
        slot `nop` with `sh $a1, 0x2886($at)` (0xA4252886), which is the same covering form the
        real clearer uses -- so the failure has to come from the COUNT, not from the store turning
        into something that no longer covers the byte.
        """
        w = list(_img.words)
        off = (0x8008CA90 - _img.text_addr) // 4
        assert w[off] == 0, f"expected a delay-slot nop, found 0x{w[off]:08X}"
        w[off] = 0xA4252886
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_gate_site(_img: Image) -> Image:
        """Turn one of the service call sites into a fourth gate-reader call."""
        w = list(_img.words)
        off = (POLL_SITES[0] - _img.text_addr) // 4
        w[off] = 0x0C022E40  # jal 0x8008B900
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_buffers(_img: Image) -> Image:
        w = list(_img.words)
        for site in POLL_SITES:
            for k in range(28):
                if w[(site - _img.text_addr) // 4 + k] == 0x24A56384:
                    w[(site - _img.text_addr) // 4 + k] = 0x24A56380
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_sites(_img: Image) -> Image:
        w = list(_img.words)
        off = (POLL_SITES[0] - _img.text_addr) // 4
        w[off] = 0x00000000
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    def broken_result_bits(_img: Image) -> Image:
        # Break the data-ready bit at ONE poll site only, so the case also proves the tool reads
        # every site rather than the first.
        w = list(_img.words)
        base = (POLL_SITES[1] - _img.text_addr) // 4
        for k in range(14):
            if w[base + k] in (0x32020004, 0x30420004):
                w[base + k] = 0x32020008
                break
        return Image(_img.data, _img.text_addr, _img.text_size, tuple(w))

    cases.append(("dispatch table arm changed", broken_dispatch,
                  f"0x80096670 holds the five recovered arms"))
    cases.append(("gate word made non-zero", broken_gate,
                  "in the image (the service is skipped)"))
    # The gate census must be able to go red on a SETTER appearing, or the conclusion is unfalsifiable.
    cases.append(("a non-zero gate setter appears", broken_gate_setter,
                  "NO SETTER"))
    cases.append(("a fourth gate-reader call site appears", broken_gate_site,
                  "is called from exactly"))
    cases.append(("ready buffer constant changed", broken_buffers,
                  f"passes buffer 0x800C6384"))
    cases.append(("a call site removed", broken_sites, "direct jal sites"))
    cases.append(("one poll site's result bit changed", broken_result_bits,
                  "poll site 0x8008CD2C tests the data-ready bit"))
    cases.append(("positive control: the unmutated image passes", lambda i: i, ""))

    passed = 0
    total = 0
    for name, mutate, expect_frag in cases:
        total += 1
        mut = mutate(img)
        fails = {m for ok, m in verify(mut) if not ok}
        if not expect_frag:
            ok = not fails
            detail = "no assertion fired" if ok else f"{len(fails)} fired: {sorted(fails)[:1]}"
        else:
            hits = [m for m in fails if expect_frag in m]
            ok = bool(hits) and fails != base_fail
            detail = f"went red on: {hits[0] if hits else '(NOT the expected fact)'}"
        print(f"  {'ok  ' if ok else 'FAIL'}  {name}: {detail}")
        if ok:
            passed += 1
    print(f"re_cd_stream selftest {passed}/{total}")
    return 0 if passed == total else 1


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", nargs="?", default=None,
                    help="the authenticated executable (default: the provisioned image in scratch/)")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    path = Path(args.image) if args.image else DEFAULT_IMAGE
    if not path.is_file():
        print(f"re_cd_stream: {path} not found -- provision the authenticated executable first. "
              f"Scanned 0 words, matched 0. Refusing: nothing was scanned.")
        return 2
    try:
        img = load(path)
    except ValueError as exc:
        print(f"re_cd_stream: {exc}")
        return 2
    return selftest(img) if args.selftest else report(img)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
