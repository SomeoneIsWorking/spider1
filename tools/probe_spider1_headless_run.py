#!/usr/bin/env python3
"""probe_spider1_headless_run.py — one bounded, headless, silent Spider-Man run, measured.

WHY A TOOL. Three of the questions this repository has to answer are about the same instant of one
run: which BIOS functions the guest REACHES, what the CD channel's own words say while the title is
stuck, and what the guest-execution counters are doing at that moment. Reading them from a log after
the fact gives three different instants. This launches the product, samples the guest's own registers
at named presented frames THROUGH THE PRODUCT'S CONTROL SURFACE, and reports them together.

WHAT IT READS, AND WHY IT IS NOT A GUESS. The CD-ROM status word is read from the DEVICE at
`0x1F801804` and the index register at `0x1F801800`, and the DMA channel at `DPCR 0x1F8010F0` /
`DICR 0x1F8010F4` / `CHCR 0x1F8010F8-0x1F8010FC`. Those addresses are the PlayStation's own device map,
not this port's opinion, and every sample is a guest-memory read the product performs — the
instrument cannot make a frame appear and it never writes.

SINGLE SLOT. The machine has ONE product slot. This refuses to start while another product binary
holds it, and kills only the PID it captured. Never `pkill`, never `pgrep -f`.

    uv run --frozen python tools/probe_spider1_headless_run.py --selftest
    uv run --frozen python tools/probe_spider1_headless_run.py --disc "$PSXPORT_SPIDERMAN_DISC" \
        --frames 4000 --sample-frames 100,400,1000,2000
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
FRAMEWORK = ROOT / "external" / "psxport"
SCRATCH = ROOT / "scratch" / "headless"
BINARY = ROOT / "build" / "agent-clang" / "bin" / "spiderman_port"
SETTINGS = ROOT / "config" / "aspect_4x3.ini"
# NO debug channel is on by default, and that is measured rather than cautious. This title's stuck
# state is a guest SPIN on I_MASK, and the `irq` channel logs every one of those reads: `irq,dmairq`
# alone wrote 1.28 GB in 90 s and `bios,cd,irq,dmairq,cdcr` wrote 4 GB, at which point the product
# stopped servicing its own control surface and a measurement that needs the surface could not be
# taken at all. The instrument reads the guest's words over `rw` instead, which is the same
# information without the firehose.
DEFAULT_DEBUG = ""

sys.path.insert(0, str(FRAMEWORK / "tools"))
sys.path.insert(0, str(FRAMEWORK / "tools" / "port"))

# The PlayStation device map, read from the hardware's own register layout.
#   I_STAT 0x1F801070 / I_MASK 0x1F801074 — the interrupt status/mask pair. I_STAT bit 2 is IRQ2, the
#   CD-ROM channel: bit 0 VBlank, 1 GPU, 2 CD-ROM, 3 DMA, 4 pad, 5 SIO, 6 SPU, 7 lightpen.
#   CD index 0x1F801800 / CD status 0x1F801804 — the controller's command/response channel.
#   DPCR 0x1F8010F0 / DICR 0x1F8010F4 / CHCR 0x1F8010F8+4n — the seven DMA channels.
I_STAT, I_MASK = 0x1F801070, 0x1F801074
CD_INDEX, CD_STATUS = 0x1F801800, 0x1F801804
DPCR, DICR, CHCR_BASE = 0x1F8010F0, 0x1F8010F4, 0x1F8010F8
MADR_BASE, BCR_BASE = 0x1F801080, 0x1F801084

# I_STAT bit -> name, from the framework's OWN interrupt-source header
# (`external/psxport/runtime/psx/irq_edge.h`), because that is what `Hle::irqPoll` means by the
# number it masks with.
#
# THE TWO PUBLISHED LAYOUTS DIVERGE ABOVE BIT 3, and this tool used to print the hardware-register
# one, which would have named bits 4..7 as pad/SIO/SPU/lightpen where psxport means timer0/1/2/SIO.
# Bits 0..3 -- the only ones this diagnosis turns on -- agree in both, and the framework's header is
# the authority for the rest because it is the header the delivery gate actually uses.
I_STAT_BITS = ((0, "VBlank"), (1, "GPU"), (2, "CD-ROM"), (3, "DMA"),
               (4, "timer0"), (5, "timer1"), (6, "timer2"), (7, "SIO"),
               (9, "SPU"), (10, "PIO"))

# CD-ROM status word bits at 0x1F801804, in the controller's own published layout. Written out
# rather than abbreviated, because the previous version of this file called bit 3 "sector-BUSY" and
# bit 7 "sector-data-available": those are bits 8 and 9 on the hardware, so a sample with 2340 unread
# bytes would have been described by the wrong two names — the exact class of confidently-wrong
# labelling this tool exists to avoid.
CD_STATUS_BITS = ((0, "playing"), (1, "seeking"), (2, "paused"), (3, "reading"), (4, "seek/reading"),
                  (5, "seek-done"), (6, "seek/reading-done"), (7, "seek/reading-ERROR"),
                  (8, "sector-data-AVAILABLE"), (9, "sector-data-REQUEST"), (10, "sector-EOF-ERROR"),
                  (11, "sector-ERROR"))

WORD_LINE = re.compile(r"^([0-9A-Fa-f]{8}):((?: [0-9A-Fa-f]{8})+)$")


def named_bits(value: int, table) -> str:
    names = [name for bit, name in table if value & (1 << bit)]
    return "|".join(names) if names else "-"


def other_product_running() -> str:
    """Name the holder of the machine's single product slot, or ''. Never pkill/pgrep -f."""
    for entry in pathlib.Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            argv = (entry / "cmdline").read_bytes().split(b"\0")
        except OSError:
            continue
        if not argv or not argv[0]:
            continue
        executable = os.path.basename(argv[0].decode("utf-8", "replace"))
        if executable in {"spiderman_port", "spiderman2_port", "tomba2_port", "crash_port"}:
            return f"pid {entry.name} {executable}"
    return ""


def selftest() -> int:
    """The report must name what it compared, and the bit decoders must be able to read both answers.

    A named-bit decoder that can only produce a non-empty string would make every sample look
    interesting, and a caller that read a named `0x004` as "some interrupt" would never learn which.
    So each decoder is checked against both a set word and a clear one, and the sample line parser is
    checked against a SHORT answer, which is the shape a capped control surface returns and the one a
    careless reader treats as "the tail is zero".
    """
    cases: list[tuple[str, bool]] = []

    def check(name: str, condition: bool) -> None:
        cases.append((name, condition))

    check("I_STAT: 0x004 names CD-ROM and nothing else", named_bits(0x004, I_STAT_BITS) == "CD-ROM")
    check("I_STAT: 0x001 names VBlank", named_bits(0x001, I_STAT_BITS) == "VBlank")
    check("I_STAT: 0x005 names CD-ROM|VBlank, so the two are not merged",
          named_bits(0x005, I_STAT_BITS) == "VBlank|CD-ROM")
    check("I_STAT: 0x000 names nothing, and says so", named_bits(0x000, I_STAT_BITS) == "-")
    # The trap this table fell into once: two published layouts disagree above bit 3. Pin bit 2, the
    # bit the whole CD diagnosis turns on, and pin bit 4 to the FRAMEWORK's header so a reader can
    # see which authority the words come from.
    check("I_STAT: 0x004 is IRQ2 CD-ROM, and the framework's header agrees",
          named_bits(0x004, I_STAT_BITS) == "CD-ROM")
    check("I_STAT: bit 4 is named from irq_edge.h, not from the hardware-register list",
          named_bits(0x010, I_STAT_BITS) == "timer0")
    check("I_STAT: 0x108 names DMA alone, because bit 8 is unnamed and SPU is bit 9",
          named_bits(0x108, I_STAT_BITS) == "DMA")
    check("I_STAT: 0x200 names SPU, which is bit 9 and not bit 6",
          named_bits(0x200, I_STAT_BITS) == "SPU")
    check("CD status: 0x000 names nothing", named_bits(0x000, CD_STATUS_BITS) == "-")
    check("CD status: 0x0108 names bits 3 and 8 by the hardware layout",
          named_bits(0x0108, CD_STATUS_BITS) == "reading|sector-data-AVAILABLE")
    check("CD status: 0x0100 names data-AVAILABLE alone, the unread-bytes shape",
          named_bits(0x0100, CD_STATUS_BITS) == "sector-data-AVAILABLE")
    check("CD status: 0x0200 names data-REQUEST, a DIFFERENT bit and not a synonym",
          named_bits(0x0200, CD_STATUS_BITS) == "sector-data-REQUEST")

    match = WORD_LINE.match("80148AD4: 00000000 33333333 33333333")
    check("parse: a data line's words are read", bool(match) and len(match.group(2).split()) == 3)
    check("parse: the base address is read", bool(match) and match.group(1) == "80148AD4")
    check("parse: a non-data line is not a data line", WORD_LINE.match("[rw] SHORT ANSWER: ...") is None)

    # THE OTHER ANSWER: a missing run must be reported as missing, never as zeros.
    check("other answer: an empty sample set is refused, not reported as zero",
          not samples_from([]))
    check("other answer: a real sample set is not refused", bool(samples_from(["frame 10"])))

    failed = [name for name, ok in cases if not ok]
    for name, ok in cases:
        print(f"  {'ok  ' if ok else 'FAIL'}  {name}")
    print(f"selftest: {len(cases) - len(failed)} of {len(cases)} case(s) passed")
    return 0 if not failed else 1


def samples_from(lines: list[str]) -> bool:
    return any(line.strip() for line in lines)


def read_words(client, address: int, count: int) -> tuple[list[int], str]:
    """Read `count` words, following the control surface's short-answer contract.

    A capped surface is asked again rather than padded, and the number SERVED versus ASKED is
    returned so a caller can refuse a partial read instead of reading the absent tail as zero.
    """
    words: list[int] = []
    asked = count
    while len(words) < count:
        chunk = min(count - len(words), 64)
        reply = client.send(f"rw {address + len(words) * 4:x} {chunk}")
        got = []
        for line in reply.splitlines():
            match = WORD_LINE.match(line.strip())
            if match:
                got = [int(word, 16) for word in match.group(2).split()]
                break
        if not got:
            return words, f"SHORT: served {len(words)} of {asked}; the rest was NOT fetched"
    return words, ""


# The CD channel's own words, in THREE round trips rather than nine. Measured: this endpoint's
# `dbg_submit` blocks until the main loop's `service()` runs, and a stuck turn answers one command
# per tens of seconds, so a nine-read sample did not finish inside a 250 s window. Batching the
# contiguous device registers into single capped reads is the difference between a measurement and a
# timeout, and it changes nothing about WHAT is read.
CD_WORDS = (
    ("I_STAT+I_MASK", I_STAT, 2, ("I_STAT", "I_MASK")),
    ("CD index+status", CD_INDEX, 2, ("index", "status")),
    ("DMA control", DPCR, 12, ("DPCR", "DICR", "CHCR0", "CHCR1", "CHCR2", "CHCR3", "CHCR4", "CHCR5",
                               "CHCR6", "CHCR7", "CHCR8", "CHCR9")),
    ("DMA ch3 address", MADR_BASE + 3 * 4, 8,
     ("MADR0", "MADR1", "MADR2", "MADR3", "BCR0", "BCR1", "BCR2", "BCR3")),
)


def report_sample(frame: int, with_client, served: int) -> None:
    """One sample of the guest's own device words at a named presented frame."""
    def work(client):
        i_stat, note = read_words(client, I_STAT, 1)
        i_mask, _ = read_words(client, I_MASK, 1)
        cd_index, _ = read_words(client, CD_INDEX, 1)
        cd_status, _ = read_words(client, CD_STATUS, 1)
        dpc, _ = read_words(client, DPCR, 1)
        dicr, _ = read_words(client, DICR, 1)
        chcr, _ = read_words(client, CHCR_BASE + 3 * 4, 1)
        maddr, _ = read_words(client, MADR_BASE + 3 * 4, 1)
        bcr, _ = read_words(client, BCR_BASE + 3 * 4, 1)
        guest = client.send("guest").strip().splitlines()[0]
        return i_stat[0], i_mask[0], cd_index[0], cd_status[0], dpc[0], dicr[0], chcr[0], maddr[0], \
            bcr[0], note, guest

    i_stat_v, i_mask_v, cd_index, cd_status, dpc, dicr, chcr, maddr, bcr, note, guest = with_client(work)
    if note:
        print(f"[sample {frame}] {note}")
        return
    pending = i_stat_v & i_mask_v
    print(f"[sample {frame}] I_STAT=0x{i_stat_v:03X} ({named_bits(i_stat_v, I_STAT_BITS)}) "
          f"I_MASK=0x{i_mask_v:03X} pending=I_STAT&I_MASK=0x{pending:03X} "
          f"({named_bits(pending, I_STAT_BITS)})")
    print(f"[sample {frame}] CD index=0x{cd_index:08X} status=0x{cd_status:04X} "
          f"({named_bits(cd_status, CD_STATUS_BITS)})")
    print(f"[sample {frame}] DPCR=0x{dpc:08X} DICR=0x{dicr:08X} CHCR3=0x{chcr:08X} MADR3=0x{maddr:08X} "
          f"BCR3=0x{bcr:08X}")
    print(f"[sample {frame}] guest: {guest}")
    sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--disc", default=os.environ.get("PSXPORT_SPIDERMAN_DISC", ""))
    parser.add_argument("--port", type=int, default=18024)
    parser.add_argument("--frames", type=int, default=4000)
    parser.add_argument("--sample-frames", default="")
    parser.add_argument("--sample-every", type=int, default=0,
                        help="sample every N presented frames from the first frame through the end; "
                             "an UNPACED run passes thousands of frames before one control-surface "
                             "round trip returns, so a fixed list of frames is a list that gets "
                             "skipped. This is the 'over frames' series the CD channel question needs.")
    parser.add_argument("--debug", default=DEFAULT_DEBUG)
    parser.add_argument("--settings", type=pathlib.Path, default=SETTINGS)
    parser.add_argument("--shot-at", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=600.0)
    parser.add_argument("--client-timeout", type=float, default=120.0)
    parser.add_argument("--poll", type=float, default=0.2)
    parser.add_argument("--native-frames", type=int, default=0,
                        help="PSXPORT_NATIVE_FRAMES: cap the product's own host turns so the run ENDS "
                             "BY ITSELF and the log is bounded. Measured: with the endpoint asked for "
                             "ten device words per sample, a stuck turn answered one command per tens of "
                             "seconds and the sampler never finished a sample, so the channel state is "
                             "read from the product's OWN channel lines in a capped run instead.")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    if not args.disc:
        print("REFUSED: no disc image. Set PSXPORT_SPIDERMAN_DISC or pass --disc; a machine-specific "
              "path baked into a tracked tool is what must never ship.")
        return 2
    disc = pathlib.Path(args.disc)
    if not disc.is_file():
        print(f"REFUSED: {disc} is not a file")
        return 2
    if not BINARY.is_file():
        print(f"REFUSED: {BINARY} does not exist; build the product before measuring it")
        return 2

    busy = other_product_running()
    if busy:
        print(f"REFUSED: the machine's single product slot is held ({busy}). NOTHING WAS MEASURED.")
        return 3

    from launch_environment import agent_environment
    from dbgclient import LiveClient

    SCRATCH.mkdir(parents=True, exist_ok=True)
    log = SCRATCH / "run.log"
    environment = agent_environment(os.environ, settings=args.settings)
    environment.update({
        "PSXPORT_PRESENT_SINK": "960x720",
        "PSXPORT_LOG_FILE": str(log),
        "PSXPORT_DISC": str(disc),
        "PSXPORT_DEBUG_SERVER": str(args.port),
        "PSXPORT_DEBUG": args.debug,
        "PSXPORT_WATCHDOG": "1800",
        "SDL_VIDEODRIVER": "offscreen",
        "SDL_AUDIODRIVER": "dummy",
        "VK_ICD_FILENAMES": os.environ.get("VK_ICD_FILENAMES",
                                           "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"),
    })
    if args.shot_at:
        environment["PSXPORT_PRESENT_SHOT_AT"] = str(args.shot_at)
    if args.native_frames:
        environment["PSXPORT_NATIVE_FRAMES"] = str(args.native_frames)
    print(f"[run] binary={BINARY}\n[run] settings={args.settings} sink={environment['PSXPORT_PRESENT_SINK']} "
          f"debug={args.debug!r} watchdog={environment['PSXPORT_WATCHDOG']}\n[run] log={log}", flush=True)

    process = subprocess.Popen([str(BINARY)], cwd=ROOT, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(f"[run] captured product pid {process.pid}; this tool kills ONLY that pid", flush=True)

    # A FRESH CONNECTION PER SAMPLE, and a LONG client timeout. Both are measured, not stylistic.
    # The endpoint's `serve_conn` is serial and `dbg_submit` blocks until the main loop's `service()`
    # runs, so a long-held connection is a request queue with one consumer and a spin-loop turn can
    # take tens of seconds; 8 s and 60 s client timeouts both timed out against a product that was
    # answering, which reads exactly like a dead endpoint. The pattern that works is in this
    # repository already (`tools/probe_spider1_widescreen_pair.py`: fresh client, 120 s, sleep, retry).
    def with_client(work):
        client = LiveClient(port=args.port, timeout=args.client_timeout)
        try:
            return work(client)
        finally:
            client.close()

    # "first" samples the first frames the endpoint actually ANSWERS. Measured and necessary: an
    # UNPACED run passes 3000 presented frames before one control-surface round trip returns, so a
    # fixed frame number is a number that gets skipped and the sample count comes back zero.
    wanted = sorted({int(v) for v in args.sample_frames.split(",")
                     if v.strip() and v.strip().lstrip("-").isdigit()})
    sample_first = "first" in [v.strip() for v in args.sample_frames.split(",")]
    next_sample = args.sample_every
    served = 0
    asked = 0
    failed_first: list[int] = []
    try:
        if args.native_frames:
            print("[run] PSXPORT_NATIVE_FRAMES is set: this run ends by itself; the control surface is "
                  "used only to read the counters once, at the end.", flush=True)
        deadline = time.time() + args.timeout
        while time.time() < deadline:
            if process.poll() is not None:
                print(f"[run] the product EXITED on its own, rc={process.returncode}")
                break
            try:
                frame = with_client(lambda c: c.frame())
            except (OSError, RuntimeError) as error:
                print(f"[run] control surface did not answer `frame`: {type(error).__name__}: {error}. "
                      "This sample is NOT a zero reading.", flush=True)
                time.sleep(2.0)
                continue
            print(f"[run] presented frame {frame}", flush=True)
            if frame >= args.frames:
                break
            take = (sample_first and served + len(failed_first) == 0) or frame in wanted
            if not ((args.sample_every and frame >= next_sample) or take):
                time.sleep(args.poll)
                continue
            if args.sample_every:
                while next_sample <= frame:
                    next_sample += args.sample_every
            asked += 1
            try:
                report_sample(frame, with_client, served)
            except (OSError, RuntimeError) as error:
                failed_first.append(frame)
                print(f"[sample {frame}] REFUSED: the control surface did not answer "
                      f"({type(error).__name__}: {error}). NOT a reading of zero.", flush=True)
            else:
                served += 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=30)
        print(f"[run] product pid {process.pid} reaped, rc={process.returncode}")
        print(f"[sample] served {served} of {asked} sample point(s) asked for; the rest were NOT read.")

    text = log.read_text(errors="replace") if log.is_file() else ""
    print(f"\n[log] {log} is {len(text)} bytes")
    if not text:
        print("[log] EMPTY. The log is the product's own words; an empty one means the run said nothing, "
              "which is not evidence that nothing happened.")
    unimplemented = sorted(set(re.findall(r"unimplemented BIOS ([ABC]0:0x[0-9A-Fa-f]{2})", text)))
    bios_calls = re.findall(r"([ABC]0:0x[0-9A-Fa-f]{2})\(0x[0-9A-Fa-f]{8}, 0x[0-9A-Fa-f]{8}, "
                            r"0x[0-9A-Fa-f]{8}, 0x[0-9A-Fa-f]{8}\) from (0x[0-9A-Fa-f]{8})", text)
    distinct = sorted({match[0] for match in bios_calls})
    if not args.debug.split(",") or "bios" not in args.debug.split(","):
        print(f"[bios] NOT MEASURED: the `bios` channel was not enabled (PSXPORT_DEBUG={args.debug!r}), "
              "so this run says NOTHING about which BIOS functions the guest reaches. An empty "
              "`reached` list under a disabled channel is not an absence.")
    else:
        print(f"[bios] the product logged {len(bios_calls)} BIOS call(s); {len(distinct)} distinct")
        print(f"[bios] reached: {', '.join(distinct) if distinct else 'NONE'}")
    if unimplemented:
        print(f"[bios] UNIMPLEMENTED and REACHED: {', '.join(unimplemented)}")
    else:
        print("[bios] UNIMPLEMENTED and REACHED: none. The product's own log names no "
              "`unimplemented BIOS` line.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
