#!/usr/bin/env python3
"""Bulk-export decompiled C + a cross-reference index, so RE becomes grep instead of Q&A.

WHY THIS EXISTS. Querying Ghidra one function at a time is the wrong shape for this work: each
invocation pays ~40s of JVM + project startup, so every question costs a minute and answers exactly
one thing. Reverse-engineering a subsystem means reading twenty functions and following the names
between them — that is a TEXT problem, and text tools are already excellent at it.

So: decompile once, write everything to scratch/decomp/, and from then on use grep/ripgrep and an
ordinary editor. Finding every caller of a function, every reader of a global, or every routine that
touches 0x1F801800 becomes a one-second grep over C instead of a Ghidra session per question.

Outputs (all gitignored, all derived from your own disc via tools/redump_ram.py):
  scratch/decomp/<addr>_<name>.c   decompiled C, one file per function
  scratch/decomp/index.tsv         addr, name, size, #callers, #callees  — sortable/greppable
  scratch/decomp/xrefs.tsv         to_addr, from_addr, type, from_function

Usage:
  tools/ghidra_export.py              export everything
  tools/ghidra_export.py 0x80080000 0x80096000    export a range (much faster while iterating)
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJ = os.path.join(ROOT, "scratch", "ghidra")
OUT = os.path.join(ROOT, "scratch", "decomp")


def main():
    lo = int(sys.argv[1], 16) if len(sys.argv) > 2 else 0
    hi = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0xFFFFFFFF
    os.makedirs(OUT, exist_ok=True)

    import pyghidra
    pyghidra.start()
    from ghidra.app.decompiler import DecompInterface, DecompileOptions
    from ghidra.util.task import ConsoleTaskMonitor

    project = pyghidra.open_project(PROJ, "spider1")
    with pyghidra.program_context(project, "/ram.bin") as prog:
        fm = prog.getFunctionManager()
        rm = prog.getReferenceManager()
        mon = ConsoleTaskMonitor()

        di = DecompInterface()
        di.setOptions(DecompileOptions())
        di.openProgram(prog)

        funcs = [f for f in fm.getFunctions(True)
                 if lo <= int(str(f.getEntryPoint()), 16) <= hi]
        print("exporting %d functions to %s" % (len(funcs), OUT))

        index, xrefs, done, failed = [], [], 0, 0
        for f in funcs:
            ea = int(str(f.getEntryPoint()), 16)
            name = f.getName()
            callers = list(rm.getReferencesTo(f.getEntryPoint()))
            callees = list(f.getCalledFunctions(mon))
            index.append("%08X\t%s\t%d\t%d\t%d" %
                         (ea, name, f.getBody().getNumAddresses(), len(callers), len(callees)))
            for r in callers:
                src = r.getFromAddress()
                owner = fm.getFunctionContaining(src)
                xrefs.append("%08X\t%s\t%s\t%s" % (ea, src, r.getReferenceType().getName(),
                                                   owner.getName() if owner else "-"))
            res = di.decompileFunction(f, 60, mon)
            if res.decompileCompleted():
                with open(os.path.join(OUT, "%08X_%s.c" % (ea, name)), "w") as fh:
                    # Header carries what the C body cannot: who calls this, and what it calls.
                    # Keeping it IN the file means one grep answers "who uses X" without a second tool.
                    fh.write("// %08X %s   callers=%d callees=%d\n" % (ea, name, len(callers), len(callees)))
                    for r in callers:
                        o = fm.getFunctionContaining(r.getFromAddress())
                        fh.write("// caller: %s %s (%s)\n" % (r.getFromAddress(),
                                                              r.getReferenceType().getName(),
                                                              o.getName() if o else "-"))
                    fh.write(res.getDecompiledFunction().getC())
                done += 1
            else:
                failed += 1
            if (done + failed) % 200 == 0:
                print("  %d/%d" % (done + failed, len(funcs)))

        with open(os.path.join(OUT, "index.tsv"), "w") as fh:
            fh.write("addr\tname\tsize\tcallers\tcallees\n" + "\n".join(sorted(index)) + "\n")
        with open(os.path.join(OUT, "xrefs.tsv"), "w") as fh:
            fh.write("to\tfrom\ttype\tfrom_func\n" + "\n".join(sorted(xrefs)) + "\n")
        # Failures are reported, never silent: a missing .c file must not read as "no such function".
        print("decompiled %d, FAILED %d, index + xrefs written to %s" % (done, failed, OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
