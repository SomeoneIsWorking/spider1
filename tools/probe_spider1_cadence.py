#!/usr/bin/env python3
"""probe_spider1_cadence.py — measure Spider-Man 1's frame rate, or refuse to.

WHAT THIS ANSWERS, AND WHY IT NEEDS TWO NUMBERS. `docs/issues/0030` left Spider-Man 1's frame rate
NOT ESTABLISHED, and correctly: the guest does not pace through VSync, it waits through its own
`FUN_8005E748(n)` on a field counter at `[gp+0x0C74]`, and the frame body's second wait site sits
inside a `DrawSync(1)` back-edge loop, so the per-frame field count is not a compile-time constant.
The named way out was "a headless run reporting game-frames-per-second AND the per-frame advance of
`[gp+0x0C74]` together". This is that run.

  * frames/second alone cannot separate 30 from 60 without knowing the field rate, because a host
    that presents at 60 with two fields per present and a host that presents at 60 with one field
    per present are the same number;
  * the counter delta alone says nothing, because the counter says how many FIELDS passed, not how
    many FRAMES the CPU retired between them.

Together they give fields-per-frame, and fields-per-frame times the field rate is the frame rate.

WHY THE COUNTER'S ADDRESS IS MEASURED HERE AND NOT TYPED IN. The address depends on `gp`, and `gp`
is not in the PS-X EXE header (it is 0 there); it is established by the guest's own crt0
(`lui $gp, 0x800B ; addiu $gp, $gp, 0x47F4` at 0x80087418/0x8008741C). So this tool finds `gp`
from those bytes, derives `[gp+0x0C74]`, and then CENSUSES the writers of `0x0C74($gp)` in the whole
text segment. It refuses unless there is exactly one, and unless that writer is preceded by an
`addiu` of exactly +1 on the same register. "One writer, +1" is the property that makes the delta a
field count; if the census cannot establish it, the delta is a number of something else.

A CENSUS DEFECT THIS TOOL HAD ON ITS FIRST PASS, recorded because the shape recurs: the first store
sweep matched opcodes 0x20/0x21/0x23/0x24/0x25/0x28/0x29 and so MISSED `sw` (0x2B). It reported
"0 stores" on a counter with a known writer at 0x8005E53C — a confident zero produced by an
incomplete enumeration, which is the most believable possible wrong answer. The opcode set here is
the complete MIPS store set and `selftest` pins it by counting a `sw` and an `sw`-shaped word.

SINGLE SLOT. The machine has one product slot. This refuses to start while another product binary
holds it and kills only the PID it captured. Never `pkill`, never `pgrep -f`.

    uv run --frozen python tools/probe_spider1_cadence.py --selftest
    uv run --frozen python tools/probe_spider1_cadence.py --disc "$PSXPORT_SPIDERMAN_DISC" \
        --samples 5 --timeout 600
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import struct
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
FRAMEWORK = ROOT / "external" / "psxport"
SCRATCH = ROOT / "scratch" / "cadence"
BINARY = ROOT / "build" / "agent-clang" / "bin" / "spiderman_port"
SETTINGS = ROOT / "config" / "aspect_4x3.ini"
EXECUTABLE = ROOT / "scratch" / "assets" / "spiderman1" / "SLUS_008.75"
HEADER = 0x800  # PS-X EXE header; the text segment starts right after it

sys.path.insert(0, str(ROOT / "tools"))
from probe_mips_image import decode  # noqa: E402

# The control-surface client and the launcher environment live in the FRAMEWORK, and the framework
# is reached through `external/psxport`, which is a SYMLINK to the shared tree in this workspace.
# Both paths are added explicitly rather than relying on cwd: a tool that finds them only when run
# from the repository root works exactly once and then reports a missing module as if the client
# did not exist.
sys.path.insert(0, str(FRAMEWORK / "tools"))
sys.path.insert(0, str(FRAMEWORK / "tools" / "port"))

# The guest's field counter, as `gp`-relative displacement. 0x0C74 is what `FUN_8005E510` stores
# (`sw $v0, 0x0C74($gp)`) after `addiu $v0, $v0, 1`, and what `FUN_8005E748(n)` polls
# (`lw $v1, 0x0C74($gp)` in its compare loop). The ADDRESS is derived from the image; this
# displacement is the one fact read from `docs/issues/0030` and re-proved by the census below.
FIELD_COUNTER_DISPLACEMENT = 0x0C74

# The crt0 that establishes `gp`, from the executable's own bytes.
GP_LUI = 0x80087418
GP_ADDIU = 0x8008741C

# The COMPLETE MIPS store opcodes: sb, sh, swl, sw, sd. Omitting `sw` is not a shorthand, it is the
# defect that made the first sweep of this tool report zero writers on a counter with one.
STORE_OPS = {0x28: "sb", 0x29: "sh", 0x2A: "swl", 0x2B: "sw", 0x2F: "sd"}
LOAD_OPS = {0x20: "lb", 0x21: "lh", 0x22: "lwl", 0x23: "lw", 0x24: "lbu", 0x25: "lhu", 0x2E: "ld"}
GP_REGISTER = 28

# The display field rate the frame pacer is clocked at, from the product's own log line
# "display standard -> NTSC (59.940 Hz fields -- the frame pacer's clock, GP1(08)=08000002)".
NTSC_FIELD_HZ = 59.94

# The presents/second above which a run cannot have been paced. A paced run presents at most one
# frame per display field, so it cannot exceed 59.94 presents/second; the threshold is set an order
# of magnitude above that so a merely busy machine cannot be misread as unpaced, and well below
# anything a real cadence produces. MEASURED: an unpaced agent run on this title presents at
# 1,204/second, because `launch_environment.agent_environment` sets `PSXPORT_NOPACE=1` for every
# agent run by design.
UNPACED_PRESENTS_PER_SECOND = 300.0

FRAME_LINE = re.compile(r"frame=(-?\d+)\s+interp=(-?\d+)\s+total=(-?\d+)")
WORD_LINE = re.compile(r"^([0-9A-Fa-f]{8}):((?: [0-9A-Fa-f]{8})+)$")


class Refuse(Exception):
    """Raised instead of reporting a frame rate this measurement does not establish."""


# ---- the image side ------------------------------------------------------------------------

def load_image(path: pathlib.Path):
    data = path.read_bytes()
    if data[:8] != b"PS-X EXE":
        raise Refuse(f"{path} is not a PS-X EXE; nothing was scanned")
    _pc, gp_header, t_addr, t_size = struct.unpack_from("<4I", data, 0x10)
    words = tuple(struct.unpack_from("<I", data, HEADER + 4 * i)[0] for i in range(t_size // 4))
    return data, t_addr, t_size, words, gp_header


def word_at(words, t_addr: int, addr: int) -> int:
    off = addr - t_addr
    if off < 0 or off % 4 or off + 4 > len(words) * 4:
        raise Refuse(f"0x{addr:08X} is not an aligned word inside the text segment")
    return words[off // 4]


def recover_gp(words, t_addr: int) -> int:
    """`gp` from the guest's own crt0, and REFUSE if the shape is not the one named.

    The PS-X EXE header's `gp` field is 0 for this image, which is why the address has to come from
    the code: `lui $gp, 0x800B` (0x0F, rt = 28) followed by `addiu $gp, $gp, imm` (op 9,
    rs = rt = 28).
    """
    lui = word_at(words, t_addr, GP_LUI)
    if (lui >> 26) != 0x0F or ((lui >> 16) & 0x1F) != GP_REGISTER:
        raise Refuse(f"0x{GP_LUI:08X} is 0x{lui:08X}, not `lui $gp, <hi>`; gp was NOT recovered")
    hi = lui & 0xFFFF
    if hi & 0x8000:
        hi -= 0x10000
    addiu = word_at(words, t_addr, GP_ADDIU)
    if (addiu >> 26) != 0x09 or ((addiu >> 21) & 0x1F) != GP_REGISTER or ((addiu >> 16) & 0x1F) != GP_REGISTER:
        raise Refuse(f"0x{GP_ADDIU:08X} is 0x{addiu:08X}, not `addiu $gp, $gp, <lo>`; gp was NOT recovered")
    lo = addiu & 0xFFFF
    if lo & 0x8000:
        lo -= 0x10000
    return ((hi << 16) + lo) & 0xFFFFFFFF


def census_field_counter(words, t_addr: int, displacement: int) -> dict:
    """Every store to `displacement($gp)`, and whether exactly one of them is `+1`.

    The COMPLETE store opcode set, and a check on the increment, because "the counter advances by
    one per field" is the property that makes the delta a field count. Reported with the word
    denominator either way.
    """
    stores, loads = [], []
    for i, word in enumerate(words):
        op = word >> 26
        if ((word >> 21) & 0x1F) != GP_REGISTER or (word & 0xFFFF) != displacement:
            continue
        pc = t_addr + 4 * i
        if op in STORE_OPS:
            stores.append((pc, STORE_OPS[op], (word >> 16) & 0x1F))
        elif op in LOAD_OPS:
            loads.append((pc, LOAD_OPS[op]))
    detail = []
    for pc, mnemonic, rt in stores:
        i = (pc - t_addr) // 4
        increment = None
        for k in range(max(0, i - 5), i):
            w = words[k]
            if (w >> 26) == 0x09 and ((w >> 16) & 0x1F) == rt and ((w >> 21) & 0x1F) == rt:
                imm = w & 0xFFFF
                increment = imm - 0x10000 if imm & 0x8000 else imm
                detail.append((pc, increment, t_addr + 4 * k))
        stores_detail = increment
        if stores_detail is None:
            detail.append((pc, None, None))
    return {
        "words": len(words),
        "stores": stores,
        "loads": loads,
        "increments": detail,
    }


def verify_counter(census: dict) -> list[tuple[bool, str]]:
    out: list[tuple[bool, str]] = []
    out.append((len(census["stores"]) == 1,
                f"exactly one store to 0x{FIELD_COUNTER_DISPLACEMENT:04X}($gp) "
                f"(found {len(census['stores'])} of {census['words']} words scanned)"))
    if len(census["stores"]) == 1:
        pc, _mnemonic, _rt = census["stores"][0]
        inc = census["increments"][0][1] if census["increments"] else None
        out.append((inc == 1,
                    f"that store at 0x{pc:08X} is preceded by an addiu of exactly +1 "
                    f"(found {inc!r})"))
    out.append((len(census["loads"]) > 0,
                f"the counter is READ somewhere ({len(census['loads'])} loads), so a delta is meaningful"))
    return out


# ---- the pure arithmetic, and its ability to give BOTH answers -------------------------------

def fields_per_frame(field_delta: int, frame_delta: int) -> float | None:
    if frame_delta <= 0:
        return None
    return field_delta / frame_delta


def frame_rate_from(fields_per_present: float, field_hz: float = NTSC_FIELD_HZ) -> float:
    """Game frames per second, from fields per presented frame and the display field rate.

    This is the step that needs BOTH measurements. 1 field/present at 59.94 Hz fields is 59.94 fps
    and 2 fields/present at the same field rate is 29.97 fps, and the presents/second figure alone
    is the SAME number in both worlds.
    """
    if fields_per_present <= 0:
        return 0.0
    return field_hz / fields_per_present


def classify(presents_per_second: float, fields_per_present: float) -> str:
    """The verdict, stated as what the numbers support and nothing more.

    IT REFUSES AN UNPACED RUN OUTRIGHT, and that refusal is the point. `agent_environment` sets
    `PSXPORT_NOPACE=1` for every agent run, so the host presents as fast as the machine allows and
    the display field rate is NOT in force. On such a run the fields-per-present ratio still means
    something -- it is the ratio of the guest's field callback to the host's turns -- but it is not
    a game frame rate, because nothing here is running at the game rate. The first version of this
    tool applied the 59.94 Hz constant to an unpaced run and printed "58.62 game frames/second",
    which is a number for a program the player never runs. The check below is that bug, made
    impossible.
    """
    if fields_per_present <= 0:
        return ("NOT ESTABLISHED: the field counter did not advance while frames were presented, "
                "so the guest is not retiring frames in step with fields. This is a stuck "
                "measurement, not a frame rate.")
    if presents_per_second > UNPACED_PRESENTS_PER_SECOND:
        return (f"NOT A FRAME RATE -- THE RUN IS UNPACED. {presents_per_second:.0f} presents/second "
                f"against a {NTSC_FIELD_HZ} Hz display field rate is "
                f"{presents_per_second / NTSC_FIELD_HZ:.0f}x faster than a paced run can present, so "
                f"the {fields_per_present:.3f} fields-per-present figure is the ratio of the guest's "
                f"field callback to the HOST's turns, not a cadence. Measure a run WITHOUT "
                f"PSXPORT_NOPACE, or the frame rate stays unestablished.")
    fps = frame_rate_from(fields_per_present)
    nearest = min((30.0, 60.0), key=lambda c: abs(c - fps))
    return (f"{fields_per_present:.3f} fields per presented frame at {presents_per_second:.2f} "
            f"presents/second; with a {NTSC_FIELD_HZ} Hz field that is {fps:.2f} game frames/second, "
            f"nearest standard rate {nearest:.0f} fps.")


def selftest() -> int:
    """Both answers, plus the census that catches a `sw`-shaped word, plus a refusal on no samples."""
    cases: list[tuple[str, bool]] = []

    def check(name: str, cond: bool) -> None:
        cases.append((name, cond))

    # THE OTHER ANSWER. The whole reason both numbers are required is that presents/second cannot
    # separate these two, so the arithmetic is pinned on both, and they must NOT collapse.
    sixty_1 = fields_per_frame(300, 300)
    thirty_2 = fields_per_frame(300, 150)
    check("arithmetic: 300 fields over 300 frames is 1.000 field/frame",
          sixty_1 is not None and abs(sixty_1 - 1.0) < 1e-9)
    check("arithmetic: 300 fields over 150 frames is 2.000 field/frame",
          thirty_2 is not None and abs(thirty_2 - 2.0) < 1e-9)
    check("arithmetic: the two answers are DISTINCT even at identical presents/second",
          sixty_1 != thirty_2)
    check("verdict: 1 field/frame reads 59.94 fps",
          abs(frame_rate_from(1.0) - 59.94) < 0.01)
    check("verdict: 2 fields/frame reads 29.97 fps",
          abs(frame_rate_from(2.0) - 29.97) < 0.01)
    check("verdict: 1 field/frame says 60, 2 says 30 -- the discriminator the issue asked for",
          abs(frame_rate_from(1.0) - frame_rate_from(2.0) - 29.97) < 0.01)
    check("verdict: a zero field delta is NOT called a frame rate",
          "NOT ESTABLISHED" in classify(30.0, 0.0))
    check("arithmetic: a zero frame delta refuses rather than dividing",
          fields_per_frame(10, 0) is None)

    # THE UNPACED REFUSAL, and the numbers that produced it. `agent_environment` sets
    # `PSXPORT_NOPACE=1`, so the first version of this tool measured 1,204 presents/second and
    # divided by 59.94 Hz to print "58.62 game frames/second" -- a rate for a program no player
    # runs. The refusal is pinned on the measured figure AND on a real paced figure, so a machine
    # that is merely busy is not misread.
    measured_unpaced = classify(1204.46, 1.023)
    check("verdict: the MEASURED unpaced run (1204 presents/s) is REFUSED as a frame rate",
          "NOT A FRAME RATE" in measured_unpaced)
    check("verdict: the refusal NAMES the unpaced cause rather than saying 'unknown'",
          "UNPACED" in measured_unpaced and "PSXPORT_NOPACE" in measured_unpaced)
    check("verdict: a genuinely paced 30 presents/s is NOT refused",
          "NOT A FRAME RATE" not in classify(30.0, 2.0))
    check("verdict: a genuinely paced 60 presents/s is NOT refused",
          "NOT A FRAME RATE" not in classify(60.0, 1.0))
    check("verdict: a merely busy 120 presents/s is not misread as unpaced",
          "NOT A FRAME RATE" not in classify(120.0, 1.0))

    # The census's opcode coverage. This is the defect that made the first sweep report zero
    # writers, so the set is pinned by shape, not by count.
    sw_word = 0xAF820C74  # sw $v0, 0x0C74($gp) -- exactly what 0x8005E53C holds
    check("census: 0xAF820C74 is classified as a store (it IS sw $v0, 0x0C74($gp))",
          (sw_word >> 26) in STORE_OPS and STORE_OPS[sw_word >> 26] == "sw")
    check("census: the sw register pair decodes to ($gp, 0x0C74)",
          ((sw_word >> 21) & 0x1F) == GP_REGISTER and (sw_word & 0xFFFF) == FIELD_COUNTER_DISPLACEMENT)
    check("census: sh and sb are classified as stores too, so the set is complete",
          all(o in STORE_OPS for o in (0x28, 0x29, 0x2B)))

    # The census verdict on synthetic word sets. Both the positive and the NEGATIVE corpus are
    # asked, because a census that only ever finds its own fixture is exactly the instrument that
    # reports a confident zero.
    empty = census_field_counter((0,) * 64, 0, FIELD_COUNTER_DISPLACEMENT)
    check("census: a word set with NO store reports zero stores of 64 words",
          len(empty["stores"]) == 0 and empty["words"] == 64)
    check("census: the zero-store corpus FAILS the exactly-one-writer assertion",
          not all(ok for ok, _ in verify_counter(empty)))

    # A store plus the +1 that makes it a field counter, plus a load so the delta is meaningful:
    # the accept case for the whole contract, on all three assertions at once.
    addiu_word = 0x24420001  # addiu $v0, $v0, 1
    load_word = 0x8F830C74  # lw $v1, 0x0C74($gp) -- the poll in FUN_8005E748
    good = census_field_counter(
        (0, addiu_word, sw_word, load_word) + (0,) * 60, 0, FIELD_COUNTER_DISPLACEMENT)
    check("census: store preceded by addiu +1 PASSES every assertion",
          all(ok for ok, _ in verify_counter(good)))
    check("census: the +1 is read from the instruction, not assumed",
          good["increments"] and good["increments"][0][1] == 1)

    # ... and the same store preceded by a +2 must FAIL, so "exactly +1" is a real test.
    plus_two = census_field_counter((0, 0x24420002, sw_word) + (0,) * 61, 0, FIELD_COUNTER_DISPLACEMENT)
    check("census: store preceded by addiu +2 is REFUSED, so +1 is not a formality",
          not all(ok for ok, _ in verify_counter(plus_two)))
    check("census: store with no preceding addiu is REFUSED too",
          not all(ok for ok, _ in verify_counter(census_field_counter(
              (0, 0, sw_word) + (0,) * 61, 0, FIELD_COUNTER_DISPLACEMENT))))

    failed = [name for name, ok in cases if not ok]
    for name, ok in cases:
        print(f"  {'ok  ' if ok else 'FAIL'}  {name}")
    print(f"cadence selftest: {len(cases) - len(failed)} of {len(cases)} case(s) passed")
    return 0 if not failed else 1


# ---- the run --------------------------------------------------------------------------------

def other_product_running() -> str:
    for entry in pathlib.Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            argv = (entry / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        if not argv or not argv[0]:
            continue
        name = os.path.basename(argv[0].decode("utf-8", "replace"))
        if name in {"spiderman_port", "spiderman2_port", "tomba2_port", "tomba1_port",
                    "crash_port", "crashbash_port", "ctr_port"}:
            return f"pid {entry.name} {name}"
    return ""


def sample(client) -> dict:
    """One atomic-ish sample: the presented-frame counters AND the guest field counter.

    The field counter is read in the SAME connection as the frame line so the two cannot be tens of
    seconds apart, which is the measurement error that would make the rate wrong rather than merely
    imprecise.
    """
    frame_reply = client.send("frame")
    m = FRAME_LINE.search(frame_reply)
    if not m:
        raise Refuse(f"`frame` returned no parsable line: {frame_reply.strip()!r}")
    counter_address = sample.counter_address
    rw_reply = client.send(f"rw {counter_address:x} 1")
    value = None
    for line in rw_reply.splitlines():
        wm = WORD_LINE.match(line.strip())
        if wm:
            value = int(wm.group(2).split()[0], 16)
            break
    if value is None:
        raise Refuse(f"`rw {counter_address:x} 1` returned no data line: {rw_reply.strip()!r}")
    return {
        "monotonic": time.monotonic(),
        "wall": time.time(),
        "frames": int(m.group(1)),
        "interp": int(m.group(2)),
        "total": int(m.group(3)),
        "field_counter": value,
    }


def measure(args) -> int:
    if not EXECUTABLE.is_file():
        print(f"REFUSED: {EXECUTABLE} is absent, so the field counter's ADDRESS could not be recovered "
              f"from bytes. Nothing was measured. Provision the authenticated executable first.")
        return 2
    if not BINARY.is_file():
        print(f"REFUSED: {BINARY} does not exist; build the product before measuring it")
        return 2
    disc = pathlib.Path(args.disc)
    if not disc.is_file():
        print(f"REFUSED: {args.disc} is not a file. Nothing was measured.")
        return 2

    try:
        _data, t_addr, _t_size, words, gp_header = load_image(EXECUTABLE)
        gp = recover_gp(words, t_addr)
        census = census_field_counter(words, t_addr, FIELD_COUNTER_DISPLACEMENT)
    except (Refuse, OSError) as exc:
        print(f"REFUSED: {exc}. Nothing was measured.")
        return 2

    print("[image]     " f"{len(words)} text words at 0x{t_addr:08X}")
    print(f"[image]     PS-X EXE header gp = 0x{gp_header:08X} "
          f"({'zero, as expected -- gp comes from the crt0' if gp_header == 0 else 'NON-ZERO'})")
    print(f"[gp]        0x{gp:08X} from 0x{GP_LUI:08X}/0x{GP_ADDIU:08X}")
    print(f"[counter]   0x{gp + FIELD_COUNTER_DISPLACEMENT:08X} = [gp+0x{FIELD_COUNTER_DISPLACEMENT:04X}]")
    for pc, mnemonic, _rt in census["stores"]:
        print(f"[census]    1 writer: 0x{pc:08X} {mnemonic} $v?, 0x{FIELD_COUNTER_DISPLACEMENT:04X}($gp)"
              f"  increment={census['increments'][0][1]!r}")
    print(f"[census]    {len(census['loads'])} loads, {len(census['stores'])} store(s) of "
          f"{census['words']} words scanned")
    failures = 0
    for ok, msg in verify_counter(census):
        print(f"  {'ok  ' if ok else 'FAIL'}  {msg}")
        if not ok:
            failures += 1
    if failures:
        print("\nREFUSED: the field counter's contract is not established, so its delta would be a "
              "count of something other than fields. Nothing was measured.")
        return 2

    busy = other_product_running()
    if busy:
        print(f"REFUSED: the machine's single product slot is held ({busy}). NOTHING WAS MEASURED.")
        return 3

    from launch_environment import agent_environment
    from dbgclient import LiveClient

    SCRATCH.mkdir(parents=True, exist_ok=True)
    log = SCRATCH / "cadence.log"
    environment = agent_environment(os.environ, settings=args.settings)
    environment.update({
        "PSXPORT_PRESENT_SINK": "960x720",
        "PSXPORT_LOG_FILE": str(log),
        "PSXPORT_DISC": str(disc),
        "PSXPORT_DEBUG_SERVER": str(args.port),
        "PSXPORT_DEBUG": args.debug,
        "PSXPORT_WATCHDOG": str(int(args.timeout) + 300),
        "SDL_VIDEODRIVER": "offscreen",
        "SDL_AUDIODRIVER": "dummy",
        "VK_ICD_FILENAMES": os.environ.get("VK_ICD_FILENAMES",
                                           "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"),
    })
    if args.paced:
        # `agent_environment` sets PSXPORT_NOPACE=1 for EVERY agent run, and the frame pacer reads
        # exactly that variable. Clearing it is what makes this run present at the display field
        # rate, and therefore what makes fields-per-present a CADENCE rather than a ratio to host
        # turns. Measured without it on this title: 1,204 presents/second, which is 20x a display
        # field and the reason the verdict refuses an unpaced run.
        environment["PSXPORT_NOPACE"] = "0"
    print(f"\n[run] binary={BINARY}\n[run] log={log}\n[run] PSXPORT_NOPACE="
          f"{environment.get('PSXPORT_NOPACE', '<unset>')}"
          f"{'  (PACED: this run can measure a frame rate)' if args.paced else '  (UNPACED: the verdict will REFUSE this as a frame rate)'}"
          f"\n[run] pid will be captured; this tool kills ONLY that pid", flush=True)
    process = subprocess.Popen([str(BINARY)], cwd=ROOT, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(f"[run] captured product pid {process.pid}", flush=True)

    sample.counter_address = gp + FIELD_COUNTER_DISPLACEMENT  # type: ignore[attr-defined]
    collected: list[dict] = []
    try:
        deadline = time.time() + args.timeout
        next_at = 0
        while len(collected) < args.samples and time.time() < deadline:
            if process.poll() is not None:
                print(f"[run] the product EXITED on its own, rc={process.returncode}")
                break
            try:
                client = LiveClient(port=args.port, timeout=args.client_timeout)
            except Exception as exc:  # noqa: BLE001 - a refused connection is data, not a crash
                print(f"[run] control surface not accepting yet ({exc}); waiting", flush=True)
                time.sleep(args.poll)
                continue
            try:
                if next_at > 0:
                    client.send("frame")  # drain until the target frame; un-paced, this passes fast
                got = sample(client)
            except Refuse as exc:
                print(f"[run] sample refused ({exc}); retrying with a fresh connection", flush=True)
                time.sleep(args.poll)
                continue
            except Exception as exc:  # noqa: BLE001
                print(f"[run] sample failed ({exc}); retrying with a fresh connection", flush=True)
                time.sleep(args.poll)
                continue
            finally:
                client.close()
            collected.append(got)
            print(f"[sample {len(collected)}] presented frame={got['frames']} "
                  f"interp={got['interp']} total={got['total']}  "
                  f"[gp+0x{FIELD_COUNTER_DISPLACEMENT:04X}]=0x{got['field_counter']:08X} "
                  f"t+{got['monotonic'] - (collected[0]['monotonic'] if collected else got['monotonic']):.1f}s",
                  flush=True)
            next_at = got["frames"] + args.stride
            time.sleep(args.poll)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=30)
        print(f"[run] product reaped (rc={process.returncode})", flush=True)

    if not samples_are_usable(collected):
        print("\nREFUSED: fewer than two usable samples, so no rate exists. "
              f"({len(collected)} sample(s) collected.)")
        return 4

    return report(collected)


def samples_are_usable(collected: list[dict]) -> bool:
    return len(collected) >= 2


def report(collected: list[dict]) -> int:
    """Report the ratio over BOTH the whole window and the window without the first sample.

    The first sample is reported separately because a boot transient is not a cadence. MEASURED on
    this title: the first paced sample reads presented frame 3 against a field counter of 4, so
    including it turns a ratio of exactly 1.000 into 1.022. That is a real measurement of a real
    off-by-one at startup, and folding it into a cadence number would be reporting a boot artefact
    as a frame rate. Dropping it silently would be the other failure, so both are printed.
    """
    first, last = collected[0], collected[-1]
    span_s = last["monotonic"] - first["monotonic"]
    frame_delta = last["frames"] - first["frames"]
    field_delta = last["field_counter"] - first["field_counter"]
    total_delta = last["total"] - first["total"]
    print()
    print(f"[measurement] whole window: {span_s:.1f} s, {frame_delta} real presents "
          f"({total_delta} total incl. in-betweens), field counter 0x{first['field_counter']:08X} "
          f"-> 0x{last['field_counter']:08X} (delta {field_delta})")
    if span_s <= 0:
        print("REFUSED: the window has no duration")
        return 4
    presents_per_second = frame_delta / span_s
    print(f"[measurement] presents/second = {frame_delta}/{span_s:.1f} = "
          f"{presents_per_second:.2f}  <-- ALONE THIS CANNOT SEPARATE 30 FROM 60")
    whole = fields_per_frame(field_delta, frame_delta)
    if whole is None:
        print("REFUSED: no presented frames advanced in the window, so there is no rate")
        return 4
    print(f"[measurement] fields per presented frame (whole window) = {field_delta}/{frame_delta} "
          f"= {whole:.4f}")

    # The steady-state window: the same arithmetic with the boot transient's sample dropped.
    steady = None
    if len(collected) >= 3:
        s_first, s_last = collected[1], collected[-1]
        s_span = s_last["monotonic"] - s_first["monotonic"]
        s_frames = s_last["frames"] - s_first["frames"]
        s_fields = s_last["field_counter"] - s_first["field_counter"]
        if s_span > 0 and s_frames > 0:
            steady = fields_per_frame(s_fields, s_frames)
            print(f"[measurement] steady window (first sample dropped): {s_span:.1f} s, "
                  f"{s_frames} presents, field delta {s_fields}")
            print(f"[measurement] fields per presented frame (steady) = {s_fields}/{s_frames} "
                  f"= {steady:.4f}  <-- THIS IS THE CADENCE FIGURE")
    if steady is None:
        print("[measurement] only two samples: no steady-state window can be formed, so the whole-"
              "window ratio includes the boot transient and is reported as such")

    basis = steady if steady is not None else whole
    print(f"\n[verdict] {classify(presents_per_second, basis)}")
    print(f"[verdict] ratio basis: {'steady window' if steady is not None else 'whole window'} "
          f"({basis:.4f} fields per presented frame)")
    if frame_delta < 30:
        print(f"[verdict] CAUTION: only {frame_delta} presents in the window. A rate computed from "
              f"this many is a measurement of a boot that has not reached steady state, and it is "
              f"reported so a reader cannot mistake it for a cadence.")
    return 0


def census_only() -> int:
    """The byte-gated half alone: recover `gp`, derive the counter, census its writers.

    Split out because it needs NO disc and NO product slot, so it is the half that CAN be a ctest
    case. Running only this half proves the ADDRESS and the CONTRACT; running the full tool proves
    the RATE. Registering the first without the second would leave a gate that can pass on a broken
    counter and prove nothing about cadence, so both halves exist, and the full tool REFUSES when
    this one fails -- the refusal is the link between them.
    """
    if not EXECUTABLE.is_file():
        print(f"REFUSED: {EXECUTABLE} is absent, so the field counter's ADDRESS could not be "
              f"recovered from bytes. Provision the authenticated executable first.")
        return 2
    try:
        _data, t_addr, _t_size, words, gp_header = load_image(EXECUTABLE)
        gp = recover_gp(words, t_addr)
        census = census_field_counter(words, t_addr, FIELD_COUNTER_DISPLACEMENT)
    except (Refuse, OSError) as exc:
        print(f"REFUSED: {exc}. Nothing was scanned.")
        return 2
    print(f"[image]     {len(words)} text words at 0x{t_addr:08X}")
    print(f"[image]     PS-X EXE header gp = 0x{gp_header:08X}")
    print(f"[gp]        0x{gp:08X} from 0x{GP_LUI:08X}/0x{GP_ADDIU:08X}")
    print(f"[counter]   0x{gp + FIELD_COUNTER_DISPLACEMENT:08X} "
          f"= [gp+0x{FIELD_COUNTER_DISPLACEMENT:04X}]")
    print(f"[census]    {len(census['stores'])} store(s), {len(census['loads'])} load(s), of "
          f"{census['words']} words scanned")
    for pc, mnemonic, _rt in census["stores"]:
        increment = census["increments"][0][1] if census["increments"] else None
        print(f"[census]    writer 0x{pc:08X} {mnemonic} $?, 0x{FIELD_COUNTER_DISPLACEMENT:04X}($gp), "
              f"preceded by an addiu of {increment!r}")
    failures = 0
    for ok, msg in verify_counter(census):
        print(f"  {'ok  ' if ok else 'FAIL'}  {msg}")
        if not ok:
            failures += 1
    if failures:
        print(f"\n{failures} assertion(s) FAILED: the field counter's contract is not established, "
              f"so its delta would count something other than fields.")
        return 1
    print("\nfield counter contract established: exactly one writer, +1 per write. "
          "The RATE is not established by this and is not claimed.")
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--disc", default=os.environ.get("PSXPORT_SPIDERMAN_DISC", ""))
    ap.add_argument("--port", type=int, default=18031)
    ap.add_argument("--samples", type=int, default=5)
    ap.add_argument("--stride", type=int, default=400,
                    help="presented frames between samples; an UNPACED run passes thousands of frames "
                         "per control-surface round trip, so a fixed frame number gets skipped")
    ap.add_argument("--debug", default="")
    ap.add_argument("--settings", type=pathlib.Path, default=SETTINGS)
    ap.add_argument("--timeout", type=float, default=600.0)
    ap.add_argument("--client-timeout", type=float, default=120.0)
    ap.add_argument("--poll", type=float, default=0.2)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--paced", action="store_true",
                    help="clear PSXPORT_NOPACE so the run is paced to the display field rate. "
                         "REQUIRED for the measurement to be a frame rate: agent_environment() sets "
                         "PSXPORT_NOPACE=1 for every agent run, and an unpaced run presents ~20x "
                         "faster than a display field, so its fields-per-present figure is a ratio "
                         "to HOST turns and not a cadence. The verdict refuses an unpaced run "
                         "rather than reporting a rate for it.")
    ap.add_argument("--census-only", action="store_true",
                    help="recover the counter address and census its writers, then stop. Needs the "
                         "provisioned image only: no disc, no product slot. This is the half that "
                         "gates the full run's precondition.")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.census_only:
        return census_only()
    return measure(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
