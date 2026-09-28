#!/usr/bin/env python3
"""probe_bios_stub_census.py — census the BIOS functions SLUS_008.75 EMITS, read from BYTES.

WHY. A missing BIOS service is a hard stop: `dispatchGuestHostService` returns
`ExecutionExitReason::Fault` with reason `unimplemented BIOS service` and the run dies. Which
services a title needs is therefore not a documentation question, it is the difference between a
port that boots and one that does not. Crash 1 in the sibling repository was blocked on exactly one
of these (`A0:0x27`), and the framework has now implemented it.

THE METHOD, AND WHY IT IS READ FROM THE IMAGE. A PSX guest emits each BIOS call as its own stub:

    addiu $t2,$zero,<VEC> ; jr $t2 ; addiu $t1,$zero,<FN>

The vector (`0xA0`/`0xB0`/`0xC0`) is loaded into the jump register, `jr` goes to it, and `$t1`
carries the function number — which is exactly what the framework reads (`core.r[9] & 0xff`).
Enumerating those three-word windows is a census of the functions the title ACTUALLY USES, recovered
from the image, not from a table somebody remembered. A title that keeps all of its BIOS calls in a
contiguous stub table makes the family position readable too.

WHAT THIS TOOL DELIBERATELY DOES NOT CLAIM.

* It is a census of what the image CONTAINS, not of what a run REACHES. A stub can be dead code.
  The reached set is a separate question with a separate instrument (`PSXPORT_DEBUG=bios`).
* The `lui` vector form is read from the instruction's OWN immediate field, bits 15..0. A `lui`
  immediate is not a displacement and not the jump target; the target is `(imm << 16) | region`, and
  a tool that read it as anything else would name a different number. That mistake is why the
  recogniser here accepts a vector only when the immediate itself is `0x00A0`/`0x00B0`/`0x00C0`,
  which is the shape both compilers emit, and prints every other candidate as REFUSED with its
  raw words so a reader can see it rather than have it vanish.
* The delay slot is decoded as its own word. `jr $t2`'s delay slot IS the `addiu $t1` here, so a
  decoder that stopped at `jr` would see no function number at all and report zero.
* The "framework implements" side is a static read of the framework's own `case 0xNN:` labels, not a
  probe of the running dispatcher. It names the files it read and the count it found in each, and it
  declares the delegation it cannot follow.

    uv run --frozen python tools/probe_bios_stub_census.py
    uv run --frozen python tools/probe_bios_stub_census.py --selftest
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_EXE = REPO / "scratch" / "assets" / "spiderman1" / "SLUS_008.75"
DEFAULT_FRAMEWORK = REPO / "external" / "psxport"

PSX_EXE_MAGIC = b"PS-X EXE"
PSX_EXE_TEXT_OFFSET = 0x800

# The BIOS entry vectors. The immediate the guest loads into the jump register is one of these
# three; anything else at a `jr` is some other call and is REFUSED, not guessed.
BIOS_VECTORS = {0xA0: "A", 0xB0: "B", 0xC0: "C"}

# The framework's BIOS dispatch owners. `Hle::dispatchBios` is the switch; the arms it DELEGATES to
# are separate functions with their own switches, and all of them are listed here so the static
# "implemented" set is the union rather than just the top-level switch.
FRAMEWORK_DISPATCH_SOURCES = (
    "runtime/psx/hle.cpp",
    "runtime/psx/bios_libc_string.cpp",
    "runtime/psx/memcard.cpp",
    "runtime/psx/bios_pad_work_area.cpp",
)

# The delegated owners write `case 0xABu:` with an integer suffix. A regex that requires a colon
# immediately after the hex digits reads THOSE files as containing zero cases — a declared set that
# silently omits libcard and the pad work area, which is the shape of a confident wrong answer.
CASE_LABEL = re.compile(r"^\s*case\s+0x([0-9A-Fa-f]{1,4})[uUlL]*\s*:")


def read_psx_exe(path: Path) -> tuple[int, bytes]:
    """Return (load address, loaded bytes) for the resident text, parsed from the header.

    The region is taken from the file's OWN `t_addr`/`t_size`, never from a hardcoded pair, so the
    denominator is the region the executable declares. A header that does not parse is a refusal, not
    a default.
    """
    data = path.read_bytes()
    if data[:8] != PSX_EXE_MAGIC:
        raise SystemExit(f"probe_bios_stub_census: {path} is not a PS-X EXE (magic {data[:8]!r})")
    t_addr, t_size = struct.unpack_from("<II", data, 0x18)
    if t_size == 0 or t_addr == 0:
        raise SystemExit(
            f"probe_bios_stub_census: {path} declares t_addr=0x{t_addr:08X} t_size=0x{t_size:X} — "
            "refusing to guess a region for a header this tool cannot read"
        )
    end = PSX_EXE_TEXT_OFFSET + t_size
    if end > len(data):
        raise SystemExit(
            f"probe_bios_stub_census: {path} declares a {t_size}-byte text at file offset "
            f"0x{PSX_EXE_TEXT_OFFSET:X} but the file is only {len(data)} bytes; refusing a short read"
        )
    return t_addr, data[PSX_EXE_TEXT_OFFSET:end]


def _addiu_zero(rt: int, imm: int) -> int:
    return (9 << 26) | (0 << 21) | (rt << 16) | (imm & 0xFFFF)


def _lui(rt: int, imm: int) -> int:
    return (15 << 26) | (rt << 16) | (imm & 0xFFFF)


def _jr(rs: int) -> int:
    return (0 << 26) | (rs << 21) | (0 << 16) | (0 << 11) | 0x08


def _ori_zero(rt: int, imm: int) -> int:
    return (13 << 26) | (0 << 21) | (rt << 16) | (imm & 0xFFFF)


def _addiu_reg(rt: int, rs: int, imm: int) -> int:
    return (9 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


def _op(word: int) -> int:
    return (word >> 26) & 0x3F


def _rs(word: int) -> int:
    return (word >> 21) & 0x1F


def _rt(word: int) -> int:
    return (word >> 16) & 0x1F


def _imm(word: int) -> int:
    return word & 0xFFFF


class Stub:
    __slots__ = ("address", "vector", "function", "vector_form", "number_form", "words")

    def __init__(self, address: int, vector: int, function: int, vector_form: str,
                 number_form: str, words: tuple[int, ...]):
        self.address = address
        self.vector = vector
        self.function = function
        # `vector_form` is how the ENTRY VECTOR was materialised (addiu/lui/lui+addiu) and
        # `number_form` is how the FUNCTION NUMBER was (addiu/ori). They are independent: the
        # MIPS-emit tool this port was built with uses `lui` for the vector and `addiu` for the
        # number, and naming only one of them would misdescribe half the sites.
        self.vector_form = vector_form
        self.number_form = number_form
        self.words = words

    @property
    def name(self) -> str:
        return f"{BIOS_VECTORS[self.vector]}0:0x{self.function:02X}"


class Candidate:
    """A `jr` whose DELAY SLOT carries a `$t1` immediate, but whose vector this tool refused."""

    __slots__ = ("address", "function", "vector_word", "words")

    def __init__(self, address: int, function: int, vector_word: int, words: tuple[int, ...]):
        self.address = address
        self.function = function
        self.vector_word = vector_word
        self.words = words


def census(base: int, text: bytes) -> tuple[list[Stub], list[Candidate], dict[str, int]]:
    """Enumerate BIOS call stubs. Returns (stubs, refused candidates, denominator)."""
    count = len(text) // 4
    denominator = {
        "words_scanned": count,
        "jr_seen": 0,
        "delay_slot_carries_t1_immediate": 0,
        "stubs_matched": 0,
        "candidates_refused": 0,
    }
    stubs: list[Stub] = []
    refused: list[Candidate] = []
    words = struct.unpack(f"<{count}I", text[: count * 4])

    for index in range(count - 2):
        jump = words[index + 1]
        # `jr $rs` is SPECIAL with funct 0x08, and both `rt` (bits 20..16) and `rd` (bits 15..11) are
        # ZERO. Testing the funct field with `word & 0x1F` would read funct's own low bits and reject
        # every real `jr`, which is a census that reports zero and looks like an absence.
        if _op(jump) != 0 or (jump & 0x3F) != 0x08 or _rt(jump) != 0 or ((jump >> 11) & 0x1F) != 0:
            continue
        target_register = _rs(jump)
        if target_register == 0:
            continue  # `jr $zero` is a no-op padding form, not a call
        denominator["jr_seen"] += 1

        delay = words[index + 2]
        # `$t1` is the ABI register the framework reads (`core.r[9] & 0xff`). A stub that puts the
        # number anywhere else is a different convention, and refusing it is the honest answer.
        if _rt(delay) != 9 or _rs(delay) != 0 or _op(delay) not in (9, 13):
            continue
        denominator["delay_slot_carries_t1_immediate"] += 1

        load = words[index]
        form = ""
        vector = -1
        # `lui $r,VEC ; addiu $r,$r,LO ; jr $r ; addiu $t1,FN` is checked FIRST, because the compiler's
        # other way to materialise the vector leaves an `addiu $r,$r,LO` where the `jr` guard looks for
        # a load, and the plain-`addiu` arm below would claim that word with vector = LO and refuse it
        # as a non-BIOS vector. The pair is one call, so it is matched as one call.
        if index >= 1:
            upper = words[index - 1]
            if (
                _op(upper) == 15
                and _rt(upper) == target_register
                and _op(load) == 9
                and _rs(load) == target_register
            ):
                form = "lui+addiu"
                vector = _imm(upper)
                # The stub STARTS at the `lui`, so that is the address a reader is given.
                index -= 1
        if form == "" and _op(load) == 9 and _rs(load) == 0 and _rt(load) == target_register:
            form = "addiu"
            vector = _imm(load)
        if form == "" and _op(load) == 15 and _rt(load) == target_register:
            form = "lui"
            vector = _imm(load)
        if form == "" or vector not in BIOS_VECTORS:
            refused.append(
                Candidate(
                    base + 4 * index,
                    _imm(delay) & 0xFF,
                    load,
                    (words[index], jump, delay),
                )
            )
            denominator["candidates_refused"] += 1
            continue
        stubs.append(Stub(base + 4 * index, vector, _imm(delay) & 0xFF, form,
                          "ori" if _op(delay) == 13 else "addiu", (load, jump, delay)))
        denominator["stubs_matched"] += 1
    return stubs, refused, denominator


def framework_implemented(framework: Path) -> tuple[dict[str, set[str]], list[str]]:
    """Read the framework's own `case 0xNN:` labels per BIOS table.

    Declared, not probed. This reads TEXT, so it can only see the labels it can see, and it says
    which files it read so a reader can judge the coverage. Delegation this cannot follow — a helper
    that switches on the function number from a table rather than a `case` label — is named in the
    report rather than assumed absent.
    """
    implemented: dict[str, set[str]] = {"A": set(), "B": set(), "C": set()}
    notes: list[str] = []
    for relative in FRAMEWORK_DISPATCH_SOURCES:
        path = framework / relative
        if not path.is_file():
            notes.append(f"{relative} IS ABSENT — not read, not counted")
            continue
        found = 0
        for line in path.read_text().splitlines():
            match = CASE_LABEL.match(line)
            if not match:
                continue
            found += 1
            implemented["A"].add(match.group(1).upper().zfill(2))
        notes.append(f"{relative}: {found} case label(s) read into the A/B/C union")
    # The B and C tables are not separable by a text scan of one file, so this reports the union and
    # says so rather than pretending to attribute a label to a table it cannot see.
    notes.append(
        "table attribution is NOT available from a text scan: the reported set is the UNION of every "
        "BIOS case label in the files read, so a function implemented only for B or only for C is "
        "reported as implemented for all three. A guest function absent from the union is the finding; "
        "a function present in the union is NOT proof it is implemented for the table this title uses"
    )
    return implemented, notes


def selftest() -> int:
    """Positive AND negative cases, and a self-check that the denominator can read the other answer.

    A census that cannot report a non-zero is not evidence of an absence, so the cases below include
    the three ways this matcher could be vacuously empty: no `jr` at all, a `jr` whose delay slot
    holds no `$t1` immediate, and a `$t1` immediate under a vector that is not a BIOS entry.
    """
    cases: list[tuple[str, bool]] = []

    def check(name: str, condition: bool) -> None:
        cases.append((name, condition))

    # A real stub in each accepted form. Each carries a DIFFERENT function number on purpose: a
    # recogniser that hardcoded one number would pass a single-form test and fail the image.
    for form, words, function, number_form in (
        ("addiu", (_addiu_zero(10, 0xA0), _jr(10), _addiu_zero(9, 0x27)), 0x27, "addiu"),
        ("lui", (_lui(10, 0xA0), _jr(10), _addiu_zero(9, 0x27)), 0x27, "addiu"),
        ("addiu", (_addiu_zero(10, 0xA0), _jr(10), _ori_zero(9, 0x3F)), 0x3F, "ori"),
        ("addiu", (_addiu_zero(10, 0xC0), _jr(10), _addiu_zero(9, 0x1B)), 0x1B, "addiu"),
    ):
        text = b"".join(struct.pack("<I", w) for w in words)
        stubs, refused, denominator = census(0x80010000, text)
        check(f"{form}/{function:#04x}: one stub at the base address", len(stubs) == 1)
        check(f"{form}/{function:#04x}: vector read from the instruction's own field",
              stubs[0].vector == words[0] & 0xFFFF)
        check(f"{form}/{function:#04x}: function read from the delay slot", stubs[0].function == function)
        check(f"{form}/{function:#04x}: stub address is the first word", stubs[0].address == 0x80010000)
        check(f"{form}/{function:#04x}: vector-load form named", stubs[0].vector_form == form)
        check(f"{form}/{function:#04x}: number-load form named", stubs[0].number_form == number_form)
        check(f"{form}/{function:#04x}: denominator counts the jr", denominator["jr_seen"] == 1)
        check(f"{form}/{function:#04x}: nothing refused", refused == [])

    # `lui $r,VEC ; addiu $r,$r,LO ; jr $r ; addiu $t1,FN` — the four-word form.
    four = struct.pack(
        "<IIII", _lui(10, 0xB0), _addiu_reg(10, 10, 0), _jr(10), _addiu_zero(9, 0x08)
    )
    stubs, _, _ = census(0x80010000, four)
    check("lui+addiu: one stub", len(stubs) == 1)
    check("lui+addiu: vector B", stubs[0].vector == 0xB0)
    check("lui+addiu: function 0x08", stubs[0].function == 0x08)
    check("lui+addiu: stub address is the lui", stubs[0].address == 0x80010000)

    # NEGATIVE 1: no `jr` anywhere. The census must be zero WITH a denominator that says it looked.
    stubs, _, denominator = census(0x80010000, struct.pack("<III", 0, 0, 0))
    check("negative: no jr means no stub", stubs == [])
    check("negative: denominator still reports the words scanned", denominator["words_scanned"] == 3)
    check("negative: jr count is zero, not absent", denominator["jr_seen"] == 0)

    # NEGATIVE 2: a `jr` whose delay slot is not a `$t1` immediate. Must not become a stub.
    text = struct.pack("<III", _addiu_zero(10, 0xA0), _jr(10), _addiu_zero(8, 0x27))
    stubs, refused, denominator = census(0x80010000, text)
    check("negative: jr with a non-$t1 delay slot is not a stub", stubs == [])
    check("negative: that jr is still counted", denominator["jr_seen"] == 1)
    check("negative: the delay-slot test rejected it", denominator["delay_slot_carries_t1_immediate"] == 0)
    check("negative: it is not silently refused either", refused == [])

    # THE TRAP THIS TOOL FELL INTO ONCE, pinned. A `jr` was recognised by testing `word & 0x1F`
    # for the rd field, but bits 4..0 ARE the funct field's low bits and every real `jr` has
    # funct = 0x08, so the test rejected 100% of them and the census reported ZERO BIOS stubs — the
    # answer that reads as "this title makes no BIOS calls". Two cases: the recogniser must ACCEPT
    # a real `jr`, and must still REJECT a SPECIAL funct-0x08 word whose rd is non-zero, so the fix
    # is a field test and not a loosened mask.
    check(
        "jr recogniser: a real `jr $t2` is ACCEPTED (bits 4..0 are funct, not rd)",
        census(0x80010000, struct.pack("<III", _addiu_zero(10, 0xA0), _jr(10), _addiu_zero(9, 0x27)))[2]["jr_seen"] == 1,
    )
    rd_set = _jr(10) | (3 << 11)  # funct 0x08 with rd = 3
    stubs, _, denominator = census(
        0x80010000, struct.pack("<III", _addiu_zero(10, 0xA0), rd_set, _addiu_zero(9, 0x27))
    )
    check("jr recogniser: funct 0x08 with rd set is REFUSED", stubs == [] and denominator["jr_seen"] == 0)

    # NEGATIVE 3: a `$t1` immediate under a non-BIOS vector. Must be REFUSED and reported, not
    # dropped — this is the shape that would otherwise read as "the guest emits no BIOS calls".
    text = struct.pack("<III", _addiu_zero(10, 0x60), _jr(10), _addiu_zero(9, 0x27))
    stubs, refused, denominator = census(0x80010000, text)
    check("negative: a non-BIOS vector is not a stub", stubs == [])
    check("negative: it is REFUSED, not dropped", len(refused) == 1)
    check("negative: the refusal is counted", denominator["candidates_refused"] == 1)
    check("negative: the refusal keeps the raw words", refused[0].words[0] == _addiu_zero(10, 0x60))

    # NEGATIVE 4: the delay slot must be decoded as its own word. A three-word window where the
    # number sits in the THIRD word of the scan is the case a decoder that stops at `jr` misses.
    text = struct.pack("<IIII", _addiu_zero(10, 0xA0), _jr(10), _addiu_zero(9, 0x2A), 0)
    stubs, _, _ = census(0x80010000, text)
    check("delay slot: the number is read from the word AFTER jr", len(stubs) == 1 and stubs[0].function == 0x2A)

    # DENOMINATOR HONESTY: a region three words long serves three words, not a page.
    text = b"".join(struct.pack("<I", w) for w in (_addiu_zero(10, 0xA0), _jr(10), _addiu_zero(9, 0x27)))
    _, _, denominator = census(0x80010000, text)
    check("denominator: words_scanned is 3 of 3, not padded", denominator["words_scanned"] == 3)
    check("denominator: one stub of one scanned stub", denominator["stubs_matched"] == 1)

    # THE INSTRUMENT MUST BE ABLE TO READ THE OTHER ANSWER. A census that can only ever say
    # "nothing missing" is the failure this repository has shipped before. Feed it a function the
    # framework does not implement and require the diff to name it.
    implemented, _ = framework_implemented(DEFAULT_FRAMEWORK)
    check(
        "other answer: an FN absent from the framework's union is reported missing",
        "AA" not in implemented["A"],
    )

    failed = [name for name, ok in cases if not ok]
    for name, ok in cases:
        print(f"  {'ok  ' if ok else 'FAIL'}  {name}")
    print(f"selftest: {len(cases) - len(failed)} of {len(cases)} case(s) passed")
    return 0 if not failed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("exe", nargs="?", type=Path, default=DEFAULT_EXE, help="authenticated PS-X EXE")
    parser.add_argument("--framework", type=Path, default=DEFAULT_FRAMEWORK)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--show-refused", type=int, default=20, help="how many refused candidates to print")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    if not args.exe.is_file():
        print(f"REFUSED: {args.exe} does not exist. Provision the authenticated executable first; "
              "this tool measures BYTES and has nothing to say without them.")
        return 2

    base, text = read_psx_exe(args.exe)
    print(f"[image] {args.exe}: {len(text)} text bytes (0x{len(text):X}) at 0x{base:08X}, "
          f"declared by the file's own t_addr/t_size")
    stubs, refused, denominator = census(base, text)
    print(f"[denominator] words scanned {denominator['words_scanned']} of {denominator['words_scanned']} "
          f"asked for; jr instructions {denominator['jr_seen']}; jr whose DELAY SLOT carries a "
          f"`addiu $t1,$zero,FN` {denominator['delay_slot_carries_t1_immediate']}; stubs matched "
          f"{denominator['stubs_matched']}; candidates REFUSED {denominator['candidates_refused']}")
    print("[scope] this is a census of what the image CONTAINS, not of what a run REACHES. A stub in "
          "dead code is still counted. The reached set is `PSXPORT_DEBUG=bios` on a real run.")

    by_name: dict[tuple[str, int], list[Stub]] = {}
    for stub in stubs:
        by_name.setdefault((BIOS_VECTORS[stub.vector], stub.function), []).append(stub)
    print(f"\n[emitted] {len(by_name)} distinct BIOS function(s) across {len(stubs)} stub site(s):")
    for name in sorted(by_name):
        sites = by_name[name]
        first = sites[0]
        label = f"{name[0]}0:0x{name[1]:02X}"
        print(f"  {label:8s} x{len(sites):<4d} first 0x{first.address:08X}  "
              f"vector={first.vector_form:9s} number={first.number_form:6s} "
              f"words={' '.join(f'{w:08X}' for w in first.words)}")

    implemented, notes = framework_implemented(args.framework)
    union = implemented["A"]
    missing = [name for name in by_name if f"{name[1]:02X}" not in union]
    print("\n[framework] the declared set is read from the framework's own `case 0xNN:` labels:")
    for note in notes:
        print(f"  {note}")
    print(f"  {len(union)} distinct BIOS function number(s) implemented across the files read")
    if missing:
        print(f"\n[FINDING] {len(missing)} function(s) this title EMITS that the framework's union does "
              f"NOT contain:")
        for name in sorted(missing):
            sites = by_name[name]
            print(f"  {name[0]}0:0x{name[1]:02X} x{len(sites):<4d} first 0x{sites[0].address:08X}  "
                  f"words={' '.join(f'{w:08X}' for w in sites[0].words)}")
    else:
        print("\n[FINDING] 0 of %d emitted functions are absent from the framework's declared union. "
              "That is a UNION result and not a per-table one: see the attribution note above."
              % len(by_name))

    if refused:
        print(f"\n[refused] {len(refused)} `jr` candidate(s) carried a `$t1` immediate under a vector "
              f"this tool does not accept; printing the first {min(args.show_refused, len(refused))}. "
              "These are NOT counted as BIOS calls and NOT counted as missing functions:")
        for candidate in refused[: args.show_refused]:
            print(f"  0x{candidate.address:08X}  {' '.join(f'{w:08X}' for w in candidate.words)}")
        if len(refused) > args.show_refused:
            print(f"  ... {len(refused) - args.show_refused} more, not printed. They were not fetched, "
                  "not counted as zero.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
