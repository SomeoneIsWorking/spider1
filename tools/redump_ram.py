#!/usr/bin/env python3
"""redump_ram.py — build a 2 MB PSX main-RAM image from the game executable.

The framework's disassembler (external/psxport/tools/disasm.py) reads a RAM DUMP, not a PS-X EXE:
addresses map to the file by `addr & 0x1FFFFF`. This lays the executable's text segment at its load
address in an otherwise-zero 2 MB image so any guest address can be disassembled directly.

    python3 tools/redump_ram.py
    python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8008739C 0x80087440

This is the reproduction step cited by every RE provenance comment in game/ — those comments are
only checkable if regenerating the image is one command.

Usage: redump_ram.py [exe] [out]
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_EXE = os.path.join(ROOT, "scratch/bin/spiderman/SLUS_008.75")
DEFAULT_OUT = os.path.join(ROOT, "scratch/bin/spiderman/ram.bin")
RAM_SIZE = 2 * 1024 * 1024
HEADER_SIZE = 0x800   # PS-X EXE header; the text segment starts right after it


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_EXE
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    if not os.path.isfile(exe):
        sys.exit(f"redump_ram: {exe} not found — provision the authenticated executable first")

    data = open(exe, "rb").read()
    if data[:8] != b"PS-X EXE":
        sys.exit(f"redump_ram: {exe} is not a PS-X EXE")

    pc, gp, load, size = struct.unpack("<4I", data[0x10:0x20])
    off = load & 0x1FFFFF
    if off + size > RAM_SIZE or HEADER_SIZE + size > len(data):
        sys.exit(f"redump_ram: text segment (load 0x{load:08X} size 0x{size:X}) does not fit")

    ram = bytearray(RAM_SIZE)
    ram[off:off + size] = data[HEADER_SIZE:HEADER_SIZE + size]

    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "wb").write(ram)
    print(f"wrote {out}  (entry 0x{pc:08X}, load 0x{load:08X}, text 0x{size:X})")


if __name__ == "__main__":
    main()
