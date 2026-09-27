#!/usr/bin/env python3
"""probe_wide_geometry.py — Spider-Man 1's OWN per-view projection geometry, sampled live.

WHAT THIS MEASURES, and why it has to be live. `FUN_80075D0C` (0x80075D0C) is the authenticated
image's sole caller of libgte SetGeomOffset (0x8008BF24) and SetGeomScreen (0x8008BF14) — one `jal`
each in a whole-image word scan, confirmed by Ghidra's reference model — and it is the only writer
of the u16 viewport descriptor that 11 other functions read (27 references to 0x800B5918 =
gp+0x1124). It does not take a projection as an argument: it DERIVES one, into that descriptor:

    vp[7]  (descriptor+14) = H        = (((vp[0] - vp[2]) >> 1) << 12) / vp[6]  << 12 / *(int*)(gp+0x1140)
    vp[8]  (descriptor+16) = OFX      = (vp[2] + vp[0]) >> 1
    vp[9]  (descriptor+18) = OFY      = (vp[5] + vp[3]) >> 1

and `FUN_8007c2ac`'s tail re-asserts CR24/CR25 DIRECTLY from descriptor+16 with `ctc2`, bypassing
libgte entirely — so the descriptor, not the libgte argument, is the single source of truth for the
horizontal centre. That is why this instrument reads the descriptor and not the GTE.

It therefore cannot be answered from a literal. The focal length is a function of a per-view lens
divisor and a per-view width, and no view has been watched yet, so the field values here are
MEASUREMENTS and must not be copied into source as constants.

IT REPORTS ITS OWN COVERAGE and distinguishes the two ways a small number can be small. It prints
the samples it took, the fields it read, and exits 3 with an explicit message when the descriptor
never held a plausible projection — a census over an unpublished descriptor measured NOTHING, and
"0 samples" must not read as a clean result.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

# SLUS_008.75. gp = 0x800B47F4, so gp+0x1124 is the descriptor the publication writes and the
# transform function re-reads. 20 bytes = the ten u16 fields the derivation uses.
DESCRIPTOR = 0x800B5918
DESCRIPTOR_WORDS = 5
DESCRIPTOR_FIELDS = (
    "vp[0]+0", "vp[1]+2", "vp[2]+4", "vp[3]+6", "vp[4]+8",
    "vp[5]+10", "vp[6]+12", "vp[7]+14=H", "vp[8]+16=OFX", "vp[9]+18=OFY",
)

# The two draw/display environments `FUN_80061140` publishes. 0x8009A6E4 is the first double buffer
# (stride 0x78): DRAWENV at +0, DISPENV at +0x5C.
DRAW_ENV = (0x8009A6E4, 0x8009A75C)
DISP_ENV = (0x8009A740, 0x8009A7B8)

# A projection is "published" when the descriptor carries a non-zero H and a centre inside the
# declared bounds. Chosen from the retail measurement (OFX=256, OFY=120, H=276) with generous
# slack, not tuned to one view.
MIN_H = 8
MAX_H = 32767


class Channel:
    """The framework's control channel. One command per line, reply terminated by `---END---`
    (external/psxport/runtime/psx/dbg_server.cpp:10). READ-ONLY: this probe never writes guest
    memory, so a census that poked the guest would be measuring the poke."""

    TERMINATOR = b"---END---"

    def __init__(self, port: int, timeout: float = 20.0) -> None:
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buffer = b""

    def command(self, line: str) -> str:
        self.sock.sendall((line + "\n").encode())
        while self.TERMINATOR not in self.buffer:
            data = self.sock.recv(1 << 20)
            if not data:
                raise RuntimeError(f"control channel closed on {line!r}")
            self.buffer += data
        head, _, rest = self.buffer.partition(self.TERMINATOR)
        self.buffer = rest.lstrip(b"\r\n")
        return head.decode(errors="replace").strip()

    def words(self, address: int, count: int) -> list[int]:
        out: list[int] = []
        while len(out) < count:
            at = address + 4 * len(out)
            reply = self.command(f"rw {at:08x} 1")
            if not reply.startswith(f"{at:08X}:"):
                raise RuntimeError(f"control channel refused {address:#x}: {reply!r}")
            out.extend(int(word, 16) for word in reply.split(":", 1)[1].split())
        return out[:count]

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


def halfwords(words: list[int]) -> list[int]:
    out: list[int] = []
    for word in words:
        out.append(word & 0xFFFF)
        out.append((word >> 16) & 0xFFFF)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=5988)
    parser.add_argument("--samples", type=int, default=64, help="descriptor reads to take")
    parser.add_argument("--interval", type=float, default=0.25, help="seconds between reads")
    parser.add_argument("--json", type=Path, help="write the census here")
    args = parser.parse_args()

    try:
        channel = Channel(args.port)
    except OSError as error:
        print(f"[geometry] no control channel on 127.0.0.1:{args.port}: {error}", file=sys.stderr)
        print("[geometry] THIS INSTRUMENT READ NOTHING. It did not sample 0 views; it failed to connect.")
        return 2

    seen: list[list[int]] = []
    disp_replies: list[str] = []
    try:
        for index in range(args.samples):
            fields = halfwords(channel.words(DESCRIPTOR, DESCRIPTOR_WORDS))
            seen.append(fields)
            if index in {0, args.samples // 2, args.samples - 1}:
                disp_replies.append(channel.command("disp"))
            if index + 1 < args.samples:
                time.sleep(args.interval)
    finally:
        channel.close()

    print(f"[geometry] COVERAGE: {len(seen)} descriptor reads of {DESCRIPTOR_FIELDS[-2:]} at "
          f"0x{DESCRIPTOR:08X}; {len(disp_replies)} display dumps")
    for reply in disp_replies:
        print(f"[geometry] disp: {reply}")

    published = [f for f in seen if MIN_H < f[7] <= MAX_H]
    print(f"[geometry] {len(published)} of {len(seen)} samples carry a published H (H in "
          f"({MIN_H}, {MAX_H}])")
    if not published:
        print("[geometry] NO PUBLISHED PROJECTION. The run never reached a view that calls the "
              "projection publication, so every field above is the absence of state, not a "
              "measurement of the projection.")
        return 3

    print("[geometry] per-field min/max/distinct over the published samples:")
    summary: dict[str, dict[str, int]] = {}
    for slot, name in enumerate(DESCRIPTOR_FIELDS):
        values = [f[slot] for f in published]
        distinct = sorted(set(values))
        summary[name] = {
            "min": min(values),
            "max": max(values),
            "distinct": len(distinct),
            "first": values[0],
        }
        shown = distinct if len(distinct) <= 8 else distinct[:4] + ["..."] + distinct[-4:]
        print(f"[geometry]   {name:12s} min={min(values):6d} max={max(values):6d} "
              f"distinct={len(distinct):3d} {shown}")

    # The identity check that matters for a widening: the derived centre must be the midpoint of
    # the declared horizontal bounds and H must be a positive u16. Both are the retail relations
    # recovered from 0x80075E18-0x80075E74, so a violation means the descriptor is not the record
    # this instrument thinks it is, and every number above would be mislabelled.
    consistent = 0
    for fields in published:
        left, _, right = fields[0], fields[1], fields[2]
        if (fields[8] == ((right + left) >> 1)) and fields[7] > 0:
            consistent += 1
    print(f"[geometry] {consistent} of {len(published)} samples satisfy OFX == (vp[0]+vp[2])>>1 — the "
          f"relation recovered at 0x80075E48-0x80075E64")
    if consistent != len(published):
        print("[geometry] THE RELATION DOES NOT HOLD on every sample. The field map above is then "
              "WRONG and these values must not be used as this title's projection geometry.")

    if args.json:
        args.json.write_text(json.dumps({
            "descriptor": f"0x{DESCRIPTOR:08X}",
            "samples": len(seen),
            "published": len(published),
            "centre_relation_holds": consistent,
            "fields": summary,
            "first_published": published[0],
        }, indent=2))
        print(f"[geometry] wrote {args.json}")
    return 0 if consistent == len(published) else 4


if __name__ == "__main__":
    sys.exit(main())
