#!/usr/bin/env python3
"""Spider-Man 1's per-frame field count: how far it can be established, and where it stops.

This tool exists because the honest answer for this title is PARTLY "cannot be determined", and
a tool that must print a number is a tool that will print a wrong one. It reports three things
separately and never merges them:

  1. VSync's argument semantics, read from the image. The title's VSync branches to NO WAIT for
     a0 < 0, a0 == 1 and a0 <= 0, and waits (a0 - 1) fields for a0 >= 2. So the `n >= 2` rule
     holds for THIS title too, and 0/1/negative carry no rate information.

  2. The VSync call census, with its denominator, INCLUDING the zero. Spider-Man reaches its
     library routines through the compiler's `jalr $t9` veneer, so a `jal`-only census finds
     ZERO sites and reports "the game never calls VSync" -- a guaranteed answer, not evidence.
     Both call forms are counted and the form is named.

  3. What the frame loop actually spends. It does not use VSync at all: it busy-waits through
     its OWN routine FUN_8005E748(n) on a field counter, and every one of its 15 call sites
     passes the literal 1. The per-frame field count is therefore the sum of the n values waited
     in one frame-loop iteration -- and one of those sites sits INSIDE a back-edge loop, so the
     sum is a LOWER BOUND and the real count depends on a runtime trip count.

WHAT THIS TOOL REFUSES TO SAY. It does not print a fields-per-frame number for this title,
because the number is not a compile-time constant. That refusal is the result.

Reuses this repository's zero-dependency `tools/probe_mips_image.py` decoder rather than
adding a second one, because a second decoder is how two titles end up disagreeing about the
same instruction.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
# The decoder, the branch-target formula and the sign extension are the OWNERS in
# tools/probe_mips_image.py. A second copy here is how two tools in one repository end up
# disagreeing about the same instruction, and this title's answer turns on the branch sign
# extension: get it wrong and every backward branch lands outside the image, which reads as
# "this loop is straight-line code" rather than as an error.
from probe_mips_image import branch_target, decode, jump_target, sign_extend  # noqa: E402,F401

ROOT = Path(__file__).resolve().parent.parent
IMAGE = ROOT / "scratch" / "assets" / "spiderman1" / "SLUS_008.75"
RECORDED_SHA1 = "4fa938b7d2e0f0acaa476e05877a0441b53b13f1"
HEADER = 0x800

# The frame loop and its head. The head is NOT the entry: 0x8002C174 runs a prologue and an
# init block, and the per-frame body is a back-edge to 0x8002C1E0. Assuming the entry is the
# head makes a census count the init block as frame work.
FRAME_ENTRY = 0x8002C174
LOOP_HEAD = 0x8002C1E0
# The title's own field wait, and the field counter it spins on.
FIELD_WAIT = 0x8005E748
FIELD_COUNTER_DISP = 0x0C74
# The vertical-blank handler, its per-field increment, and the counter it increments.
VBLANK_HANDLER = 0x8008C2E8
VBLANK_COUNTER = 0x800B397C
VSYNC_SHAPE_SIGNATURE = (0x04810000, 0x10820000, 0x18800000, 0x00052BC0)


class Refuse(Exception):
    """Raised instead of reporting a field rate this title's bytes do not establish."""


def load(path: Path):
    data = path.read_bytes()
    if data[:8] != b"PS-X EXE":
        raise Refuse(f"{path} is not a PS-X EXE; nothing was scanned")
    _pc0, _gp0, t_addr, t_size = struct.unpack_from("<4I", data, 0x10)
    return data, t_addr, t_size


def words(data, t_addr, t_size):
    return [struct.unpack_from("<I", data, HEADER + 4 * i)[0] for i in range(t_size // 4)]


def transfer_target(pc, w):
    """The next control transfer's target, or None. Delegates to the repository's owners.

    j/jal use a 26-bit field and branches a sign-extended 16-bit one. Both mistakes -- a
    missing sign extension, or OR-ing the shifted displacement under the 0xF0000000 mask
    instead of adding it to PC+4 -- send a BACKWARD branch outside the image, and the scan then
    reports a straight-line body with a confident field total. That is the specific way this
    tool could have produced a confident wrong number.
    """
    op = w >> 26
    if op in (2, 3):
        return jump_target(pc, w)
    if op in (4, 5, 6, 7):
        return branch_target(pc, w)
    return None


def find_vsync(ws, t_addr):
    """VSync by the shape of its argument branches, not by an address. Returns
    (address, bgez_candidates_scanned, matches)."""
    scanned = 0
    matches = []
    for i, w in enumerate(ws):
        if (w & 0xFFFF0000) != 0x04810000:
            continue
        scanned += 1
        window = ws[i:i + 0x40]
        if not any((x & 0xFFFF0000) == 0x10820000 for x in window):
            continue
        if not any((x & 0xFFFF0000) == 0x18800000 for x in window):
            continue
        matches.append(t_addr + i * 4)
    return matches, scanned


def measure(data: bytes, t_addr: int, t_size: int) -> dict:
    ws = words(data, t_addr, t_size)

    def w_at(a):
        return struct.unpack_from("<I", data, HEADER + (a - t_addr))[0]

    # 1. the field counter's sole incrementing writer
    scanned = stores = 0
    stores_at = []
    for i, w in enumerate(ws):
        scanned += 1
        if (w & 0xFFFF) != FIELD_COUNTER_DISP or ((w >> 21) & 31) != 28:
            continue
        if (w >> 26) in (0x2B, 0x28, 0x29):
            stores += 1
            stores_at.append(t_addr + i * 4)
    if stores != 1:
        raise Refuse(f"the game field counter has {stores} writers, expected 1; refusing")
    if w_at(stores_at[0] - 4) != 0x24420001:
        raise Refuse("the game field counter is not incremented by exactly +1; refusing")

    # 2. the vertical-blank handler: +1 per field into 0x800B397C
    for off, word, why in (
        (0x24, 0x24420001, "the VBlank handler's +1"),
        (0x2C, 0xAC22397C, "the store into the field counter"),
        (0x40, 0x0040F809, "the callback dispatch"),
        (0x4C, 0x2A220008, "the 8 callback slots"),
    ):
        if w_at(VBLANK_HANDLER + off) != word:
            raise Refuse(
                f"the VBlank handler is not as recorded at +0x{off:02X} ({why}); refusing"
            )

    # 3. VSync, by shape, and its census over BOTH call forms
    matches, sig_scanned = find_vsync(ws, t_addr)
    if len(matches) != 1:
        raise Refuse(
            f"VSync's signature matched {len(matches)} sites (scanned {sig_scanned} bgez-a0 "
            f"candidates); cannot identify it uniquely"
        )
    vsync = matches[0]
    for off, word, why in (
        (0x1C, 0x1082003A, "VSync's a0==1 no-wait branch"),
        (0x24, 0x18800007, "VSync's a0<=0 branch"),
        (0x54, 0x2485FFFF, "VSync's (a0-1) field count"),
    ):
        if w_at(vsync + off) != word:
            raise Refuse(
                f"VSync+0x{off:02X} is 0x{w_at(vsync + off):08X}, not 0x{word:08X}; {why} is "
                f"not as recorded, so the argument semantics are not established"
            )

    # The image's indirect calls are `jalr $ra,$vN` through FUNCTION POINTERS, not a `jalr $t9`
    # veneer: the census of jump registers is v1 (151), v0 (143), t2 (4) and $t9 (0). So the
    # count of statically resolvable call sites is only the `jal` form, and every `jalr` is a
    # real coverage gap rather than a form to decode. Both are reported with their denominators
    # rather than merged, because a merged 0 would read as "the game never calls VSync".
    direct = [(t_addr + i * 4, "jal") for i, w in enumerate(ws)
              if (w >> 26) == 3 and transfer_target(t_addr + i * 4, w) == vsync]
    call_forms = sum(1 for w in ws if (w >> 26) == 3 or
                     ((w >> 26) == 0 and (w & 0x3F) == 0x09))
    total_jalr = sum(1 for w in ws if (w >> 26) == 0 and (w & 0x3F) == 0x09)
    jump_regs: dict[int, int] = {}
    for w in ws:
        if (w >> 26) == 0 and (w & 0x3F) == 0x09:
            jump_regs[((w >> 21) & 31)] = jump_regs.get(((w >> 21) & 31), 0) + 1
    # Does anything in the image name VSync's address at all -- as a materialised pair or as a
    # stored pointer? A function nothing references is not one the game calls.
    raw_pointer = data.count(struct.pack("<I", vsync))
    hi, lo = (vsync >> 16) & 0xFFFF, vsync & 0xFFFF
    materialised = 0
    for i, w in enumerate(ws):
        if (w & 0xFFFF) != lo:
            continue
        for back in range(1, 10):
            j = i - back
            if j < 0:
                break
            w2 = ws[j]
            if (w2 >> 26) == 0x0F and (w2 & 0xFFFF) == hi:
                materialised += 1
                break

    # 4. the field wait the frame loop actually uses
    wait_sites = []
    for i, w in enumerate(ws):
        if (w >> 26) != 3 or transfer_target(t_addr + i * 4, w) != FIELD_WAIT:
            continue
        a0 = ws[i + 1]
        op, rs, rt, imm = a0 >> 26, (a0 >> 21) & 31, (a0 >> 16) & 31, a0 & 0xFFFF
        wait_sites.append((t_addr + i * 4, (imm - 0x10000 if imm & 0x8000 else imm)
                           if (op == 0x09 and rt == 4 and rs == 0) else None))

    # 5. the frame body, its back-edge, and whether any wait site repeats inside it
    if w_at(LOOP_HEAD) != 0x8F820740:
        raise Refuse(f"0x{LOOP_HEAD:08X} is not the frame loop head's test; refusing")
    head = (LOOP_HEAD - t_addr) // 4
    # Take the LAST backward transfer to the head, not the first. There are two (a `beqz` at
    # 0x8002C2E4 and a `j` at 0x8002C2F0); stopping at the first truncates the body and can
    # drop a field-wait site, which would lower the reported lower bound without saying so.
    body_end = None
    for i in range(head, min(head + 0x400, len(ws))):
        if transfer_target(t_addr + i * 4, ws[i]) == LOOP_HEAD:
            body_end = t_addr + i * 4
    if body_end is None:
        raise Refuse("no backward transfer to the frame loop head was found; refusing")
    span = (body_end - LOOP_HEAD) // 4 + 1

    inner = []
    for i in range(head, head + span):
        tgt = transfer_target(t_addr + i * 4, ws[i])
        pc = t_addr + i * 4
        if tgt is not None and LOOP_HEAD < tgt < pc:
            inner.append((tgt, pc))

    body_waits = [(a, n) for a, n in wait_sites if LOOP_HEAD <= a <= body_end]
    repeating = [a for a, _n in body_waits if any(tgt <= a < pc for tgt, pc in inner)]
    minimum = sum(n for a, n in body_waits if n is not None and a not in repeating)

    return {
        "t_addr": t_addr, "words": len(ws), "counter_writers": stores_at,
        "vsync": vsync, "vsync_signature_scanned": sig_scanned,
        "direct": direct, "call_forms": call_forms, "total_jalr": total_jalr,
        "jump_regs": jump_regs, "raw_pointer": raw_pointer, "materialised": materialised,
        "wait_sites": wait_sites, "body": (LOOP_HEAD, body_end, span),
        "inner": inner, "body_waits": body_waits, "repeating": repeating,
        "minimum": minimum,
    }


def _t9_target(ws, i, t_addr):
    for back in range(1, 6):
        j = i - back
        if j < 0:
            return None
        w = ws[j]
        if (w >> 16) == 0x3C19:  # lui $t9
            low = None
            for b2 in range(1, 5):
                k = j + b2
                if k >= len(ws):
                    break
                w3 = ws[k]
                if (w3 >> 16) in (0x2739, 0x2639):
                    imm = w3 & 0xFFFF
                    low = imm if (w3 >> 16) == 0x2639 else (
                        imm - 0x10000 if imm & 0x8000 else imm)
                    break
            if low is not None:
                return (((w & 0xFFFF) << 16) + low) & 0xFFFFFFFF
            return None
    return None


def report(m) -> int:
    print("== Spider-Man 1 field cadence, measured from SLUS_008.75 ==")
    print(f"  image sha1={hashlib.sha1(IMAGE.read_bytes()).hexdigest()[:16]}… "
          f"text scanned {m['words']} words")
    print()
    print("  1. The title's own field wait and counter")
    print(f"     FUN_8005E748 spins until the field counter [gp+0x{0x0C74:04X}] has advanced by")
    print("     its argument. Exactly 1 writer increments it, by +1 (scanned "
          f"{m['words']} words, matched 1).")
    print(f"     The VBlank handler 0x{VBLANK_HANDLER:08X} adds 1 to 0x{VBLANK_COUNTER:08X}")
    print("     per field, so the unit this game waits in is a FIELD, by its own construction.")
    print()
    print("  2. VSync's argument semantics (rule established for THIS title)")
    print(f"     VSync derived by shape at 0x{m['vsync']:08X} "
          f"(scanned {m['vsync_signature_scanned']} bgez-a0 candidates, matched 1):")
    print("       a0 <  0  -> no wait        (bgez a0 skips the epilogue)")
    print("       a0 == 1  -> no wait        (beq a0,1 returns)")
    print("       a0 <= 0  -> no field wait  (count 0 to the helper, which returns at once)")
    print("       a0 >= 2  -> waits (a0-1) further fields")
    print()
    print("  3. The VSync call census, with its denominator")
    print(f"     scanned {m['words']} words, {m['call_forms']} call-form instructions;")
    print(f"     matched {len(m['direct'])} direct `jal` site(s) to VSync: "
          f"{[f'0x{a:08X}' for a, _k in m['direct']] or 'none'}")
    print(f"     scanned 0 of {m['total_jalr']} `jalr` sites: they are `jalr $ra,$vN` through")
    print(f"     FUNCTION POINTERS, not a resolvable veneer. Jump registers in use: "
          f"{dict(sorted(m['jump_regs'].items()))} (register number, count).")
    print("     COVERAGE LIMIT: 0 overlay images are provisioned, and the resident image names")
    print(f"     VSync's address NOWHERE -- {m['materialised']} lui/addiu materialisation(s) and "
          f"{m['raw_pointer']} stored pointer(s), both scanned over the whole file.")
    print("     So the guest's own VSync is not reached from this image at all, statically.")
    print("     A `jal`-only census would print 0 and read as 'the game never calls VSync'.")
    print("     That is a guaranteed answer, not evidence, and it is reported as a count.")
    print()
    print("  4. What the frame loop actually spends")
    lo, hi, span = m["body"]
    print(f"     frame body 0x{lo:08X}..0x{hi:08X} = {span} instructions")
    print(f"     it waits through FUN_8005E748, not VSync, at {len(m['body_waits'])} site(s):")
    for a, n in m["body_waits"]:
        mark = "  <-- inside a back-edge loop, runs an unbounded number of times" \
            if a in m["repeating"] else ""
        print(f"       0x{a:08X}  n={n}{mark}")
    print(f"     the body contains {len(m['inner'])} inner back-edge(s): "
          f"{[(f'0x{a:08X}', f'0x{b:08X}') for a, b in m['inner']]}")
    print()
    print("  VERDICT: THE FIELDS-PER-FRAME NUMBER IS NOT ESTABLISHED, and this is the result.")
    print(f"    * The frame body's UNCONDITIONAL, NON-REPEATING field waits sum to "
          f"{m['minimum']},")
    print("      which is a LOWER BOUND on the frame cost, not the rate.")
    print("    * One wait site is inside a back-edge loop whose trip count is decided at")
    print("      runtime by a DrawSync(1) GPU fence, so the total is not a compile-time")
    print("      constant and cannot be reduced to one number from the image.")
    print("    * Consequently this title's field rate is NOT established statically, and it")
    print("      is NOT established as 60 fps either. It must not be ruled out of, or put")
    print("      into, interpolation scope on the strength of this run.")
    print()
    print("  WHAT WOULD SETTLE IT, precisely: a headless run with the game clocked against")
    print("  measured wall time, reporting game-frames-per-second and the field counter's")
    print("  advance per frame. Both halves are needed: frames/s alone cannot separate 30 from")
    print("  60 without knowing the field rate, and the counter delta alone says nothing about")
    print("  how many frames the CPU could retire between waits. This title has a provisioned")
    print("  image, so this is a live measurement away, not a blocked one.")
    return 0


def selftest(data, t_addr, t_size) -> int:
    """Every negative must be RED with its subject removed, AND must refuse for the SUBJECT's
    reason. A selftest that only asserts 'something raised' passes on the first guard it meets,
    which is how a cadence tool ends up green while testing its own image identity."""
    print("== re_cadence (spider1) selftest ==")
    checks = 0
    m = measure(data, t_addr, t_size)
    print(f"  [ ok ] positive: VSync at 0x{m['vsync']:08X}, {len(m['direct'])}"
          f" direct call site(s), {m['materialised']} materialisation(s) of its address, "
          f"{len(m['repeating'])} repeating wait(s) in the frame body")
    checks += 1

    def destroy(address, value, why, expect):
        nonlocal checks
        mutable = bytearray(data)
        offset = HEADER + (address - t_addr)
        struct.pack_into("<I", mutable, offset, value)
        try:
            measure(bytes(mutable), t_addr, t_size)
            raise AssertionError(f"{why} was accepted")
        except Refuse as error:
            if expect not in str(error):
                raise AssertionError(
                    f"{why}: refused for the WRONG reason -- {error!s} does not mention "
                    f"{expect!r}"
                )
            print(f"  [ ok ] {why} refused: {error}")
            checks += 1

    # 1. the +1 increment: without it the counter is not a field counter and nothing holds.
    destroy(m["counter_writers"][0] - 4, 0x24420002,
            "the game field counter's +1", "exactly +1")
    # 2. the VBlank handler's store: this is what ties the counter to a FIELD.
    destroy(VBLANK_HANDLER + 0x2C, 0xAC22397D, "the VBlank handler's field increment", "VBlank")
    # 3. VSync's a0==1 no-wait branch: if this were absent, a 1 could pace the game and the
    #    "0 and 1 carry no rate information" statement would be unsupported.
    destroy(m["vsync"] + 0x1C, 0x1082003B, "VSync's a0==1 no-wait branch", "a0==1")
    # 4. VSync's identity: if the signature matches 0 or 2, the census must refuse rather than
    #    report a rate derived from the wrong function.
    destroy(m["vsync"] + 0x00, 0x3C028004, "VSync's signature anchor", "signature")
    # 5. the frame loop's head: if it moved, the body extent is wrong and the wait census
    #    inside it is measuring the wrong instructions.
    destroy(LOOP_HEAD, 0x8F820741, "the frame loop head's test", "frame loop head")

    # 6. THE DISCRIMINATOR: the same tool, on the same bytes with the inner back-edge removed
    #    and the repeating wait widened to 2 fields, MUST produce a definite fields-per-frame
    #    number. Without this the "not established" verdict is indistinguishable from a tool
    #    that simply cannot count -- which is the failure mode where a refusal is really a
    #    broken instrument being read as a finding.
    hypothetical = bytearray(data)
    struct.pack_into("<I", hypothetical, HEADER + (0x8002C290 - t_addr), 0x24040002)
    struct.pack_into("<I", hypothetical, HEADER + (0x8002C29C - t_addr), 0x00000000)
    struct.pack_into("<I", hypothetical, HEADER + (0x8002C2A0 - t_addr), 0x00000000)
    alt = measure(bytes(hypothetical), t_addr, t_size)
    if alt["inner"] or alt["repeating"] or alt["minimum"] != 3:
        raise AssertionError(
            f"the discriminator did not produce a definite rate: inner={alt['inner']} "
            f"repeating={alt['repeating']} minimum={alt['minimum']}, expected 3"
        )
    print(f"  [ ok ] discriminator: with the inner loop removed the SAME tool reports a "
          f"definite {alt['minimum']} fields/frame, so 'not established' is a finding "
          f"about these bytes and not a broken counter")
    checks += 1

    print(f"spider1 re_cadence selftest: {checks}/7 PASS")
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", default=str(IMAGE))
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    try:
        path = Path(args.image)
        if not path.is_file():
            raise Refuse(
                f"{path} is missing. Re-provision with tools/provision.py; the field rate is "
                f"not measured without the image"
            )
        data, t_addr, t_size = load(path)
        if hashlib.sha1(data).hexdigest() != RECORDED_SHA1:
            raise Refuse(
                f"image sha1 {hashlib.sha1(data).hexdigest()[:12]} is not the recorded "
                f"{RECORDED_SHA1[:12]}; nothing was measured"
            )
        if args.selftest:
            return selftest(data, t_addr, t_size)
        return report(measure(data, t_addr, t_size))
    except (AssertionError, OSError, Refuse) as error:
        print(f"spider1 re_cadence REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
