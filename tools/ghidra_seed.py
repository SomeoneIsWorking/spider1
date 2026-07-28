#!/usr/bin/env python3
"""Seed the Ghidra project with the recompiler's own function boundaries, then disassemble.

WHY THIS IS REQUIRED. The RAM image is imported as a RAW BINARY: no headers, no entry point, no
symbol table. Ghidra's auto-analysis therefore has nothing to anchor on and leaves the text
undisassembled, so every reference query returns ZERO — indistinguishable from "genuinely
unreferenced", which is the precise failure mode this tooling exists to eliminate. A control query
against 0x8008C3E0 (four known `jal` callers) returning 0 is what exposed it.

The fix closes the loop between the project's two tools: the RECOMPILER already derives 1561
function entries from this exact binary, and those are the ground truth Ghidra is missing. Feed them
in, disassemble at each, and the reference model becomes real.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJ = os.path.join(ROOT, "scratch", "ghidra")


def entries():
    out = set()
    for f in glob.glob(os.path.join(ROOT, "generated", "shard_*.c")):
        with open(f) as fh:
            for m in re.finditer(r"void gen_func_([0-9A-F]{8})\(", fh.read()):
                out.add(int(m.group(1), 16))
    return sorted(out)


def main():
    ents = entries()
    if not ents:
        print("no generated/shard_*.c function entries — run tools/ensure_recomp.py first")
        return 1
    print("seeding %d function entries from the recompiled substrate" % len(ents))

    import pyghidra
    pyghidra.start()
    from ghidra.program.model.symbol import SourceType
    from ghidra.app.cmd.disassemble import DisassembleCommand
    from ghidra.app.cmd.function import CreateFunctionCmd

    # Bind the program ALREADY IN THE PROJECT. open_program(path, ...) silently creates a fresh
    # EMPTY program instead, which is why the first attempt reported "created 0 functions": it was
    # seeding nothing. open_project + program_context binds the saved, imported one.
    project = pyghidra.open_project(PROJ, "spider1")
    with pyghidra.program_context(project, "/ram.bin") as prog:
        af = prog.getAddressFactory()
        fm = prog.getFunctionManager()
        tx = prog.startTransaction("seed recompiler entries")
        made = 0
        try:
            for a in ents:
                addr = af.getAddress("0x%08X" % a)
                DisassembleCommand(addr, None, True).applyTo(prog)
                if fm.getFunctionAt(addr) is None:
                    if CreateFunctionCmd(addr).applyTo(prog):
                        made += 1
        finally:
            prog.endTransaction(tx, True)
        print("created %d functions; program now has %d" % (made, fm.getFunctionCount()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
