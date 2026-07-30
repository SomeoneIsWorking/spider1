#!/usr/bin/env python3
"""RE-16 rule 2, REPORT-ONLY: which function entries cannot be emitted as host callees?

The emitter translates a callee's `jr $ra` into a host `return`. That is sound only if, at that
`jr`, (a) guest `$sp` is back to its entry value and (b) `$ra` still holds entry provenance. An
entry that violates either cannot be a function *as emitted*, whatever the guest intended.

That reframing is the whole point. "Is this a spurious entry or a legitimate shared-epilogue
continuation?" is a question about INTENT, and intent has no proof-grade witness in the bytes —
which is why four heuristics failed on it (78 / 325 / 50 / 255 demotions, one of them taking
`StGetNext` and breaking the link). This question is about the EMITTED ARTIFACT, and the bytes
answer it.

REPORT-ONLY BY DESIGN. It changes nothing; it prints the violation set so it can be diffed against
rule 1 and the must-keep list BEFORE it is ever allowed to influence emission. If it flags dozens,
the walk rules are wrong — most plausibly the jal-inline rule — and the numbers should be believed
over the reasoning that produced them.

    python3 tools/callee_contract.py [--selftest]
"""
import os
import sys
import bisect

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(REPO, "external/psxport/tools/recomp"))

import psexe                      # noqa: E402
from decode import decode         # noqa: E402
import decode as D                # noqa: E402

EXE = os.path.join(REPO, "scratch/bin/SLUS_008.75")
DECLS = os.path.join(REPO, "generated/rec_decls.h")

# Ground truth for this binary (docs/re-frontier.md RE-16).
WANT_VIOLATION = 0x8002A5F4          # the coroutine yield — must be flagged
RULE1 = 0x8002A478                   # already demoted by rule 1
MUST_KEEP = {0x80086B10: "StGetNext (the port super-calls it — demoting breaks the LINK)",
             0x8007CD44: "branch-and-link subroutine (seeded by the 131-site link fix)",
             0x8007D160: "branch-and-link subroutine",
             0x8007D1F0: "branch-and-link subroutine",
             0x8007D254: "branch-and-link subroutine",
             0x8007D534: "tail-call target (jal x5 then j x2)"}


def die(msg):
    print(f"REFUSED: {msg}", file=sys.stderr)
    sys.exit(2)


def load():
    if not os.path.exists(EXE):
        die(f"no executable at {EXE} — this script examined NOTHING.")
    if not os.path.exists(DECLS):
        die(f"no {DECLS} — the function set is unknown, so this script examined NOTHING.")
    exe = psexe.load_exe(EXE) if hasattr(psexe, "load_exe") else psexe.load(EXE)
    funcs = set()
    for line in open(DECLS):
        k = line.find("void gen_func_")
        while k >= 0:
            funcs.add(int(line[k + 14:k + 22], 16))
            k = line.find("void gen_func_", k + 1)
    funcs = {f for f in funcs if exe.load <= f < exe.text_end}
    if not funcs:
        die(f"{DECLS} parsed to ZERO in-text functions — the parse is broken, not the binary.")
    return exe, sorted(funcs)


def has_prologue(exe, a):
    return (exe.word(a) & 0xFFFF8000) in (0x27BD8000, 0x23BD8000)


# ─────────────────────────────────────────────────────────────────────────────────────────────────
# The walk. It must mirror EMISSION, or the proof is about the wrong object.
# ─────────────────────────────────────────────────────────────────────────────────────────────────
UNKNOWN = "unknown"      # cannot be proven either way -> REFUSE, never default


def walk(exe, entry, end, live, demoted):
    """Walk the CFG from `entry` tracking net sp displacement and $ra provenance.

    Returns (verdict, detail) where verdict is "ok", "violation" or "unknown".

    State per path: (sp_delta, ra_prov, frozenset of stack offsets this path wrote $ra to).
    `ra_prov` is "entry" (never redefined), ("slot", off) (reloaded from a stack slot) or UNKNOWN.

    REFUSING IS MANDATORY, not tidiness. `$ra` through a register move or non-sp memory, or an sp
    adjustment by a non-constant, means balance cannot be proven — and a default in either direction
    is exactly where a silent miscompile would re-enter. Those paths report "unknown" and the caller
    must treat them as un-classifiable rather than safe.
    """
    seen = set()
    work = [(entry, 0, "entry", frozenset())]
    saw_unknown = None
    while work:
        a, sp, ra, wrote = work.pop()
        if not (exe.load <= a < exe.text_end):
            continue
        key = (a, sp, ra, wrote)
        if key in seen:
            continue
        seen.add(key)
        try:
            i = decode(a, exe.word(a))
        except IndexError:
            continue
        if i.kind == D.UNKNOWN:
            continue                       # trailing data, not a path

        # --- sp adjustment
        if i.op in ("addi", "addiu") and i.rt == 29:
            if i.rs != 29:
                saw_unknown = saw_unknown or f"sp written from r{i.rs} at 0x{a:08X}"
                continue
            sp += i.simm
        elif (i.kind in (D.ALU_RRR, D.SHIFT_I, D.SHIFT_V) and i.rd == 29) or \
             (i.kind in (D.ALU_RRI, D.LUI) and i.rt == 29):
            saw_unknown = saw_unknown or f"non-constant sp adjust at 0x{a:08X}"
            continue

        # --- $ra definition / save
        if i.op == "sw" and i.rt == 31:
            if i.rs == 29:
                wrote = wrote | {sp + i.simm}      # slot recorded in ENTRY-relative terms
        elif i.op == "lw" and i.rt == 31:
            ra = ("slot", sp + i.simm) if i.rs == 29 else UNKNOWN
        elif i.op in ("jal", "bltzal", "bgezal"):
            ra = "link"
        elif (i.kind in (D.ALU_RRR, D.SHIFT_I, D.SHIFT_V) and i.rd == 31) or \
             (i.kind in (D.ALU_RRI, D.LUI) and i.rt == 31):
            ra = UNKNOWN

        # --- control flow
        if i.op == "jr" and i.rs == 31:
            if ra == "entry" and sp == 0:
                continue                                  # representable: a clean return
            if isinstance(ra, tuple) and ra[0] == "slot":
                if ra[1] in wrote and sp == 0:
                    continue                              # restored what this path saved: fine
                return ("violation",
                        f"`jr $ra` at 0x{a:08X} with $ra from stack slot entry{ra[1]:+d}, "
                        f"which this path never wrote (net sp {sp:+d})")
            if sp != 0:
                return ("violation",
                        f"`jr $ra` at 0x{a:08X} reached with net sp displacement {sp:+d}")
            if ra is UNKNOWN or ra == "link":
                saw_unknown = saw_unknown or f"$ra provenance unprovable at `jr` 0x{a:08X}"
            continue

        if i.kind == D.BRANCH:
            if i.target is not None:
                work.append((i.target, sp, ra, wrote))     # taken
            work.append((a + 8, sp, ra, wrote))            # not taken (delay slot consumed)
            continue
        if i.kind == D.JUMP:
            if i.op == "jal":
                # A call to a SURVIVING entry is opaque and balanced; a call to a DEMOTED one is
                # inline flow, mirroring the `link + goto` the emitter will produce for it.
                if i.target in demoted:
                    work.append((i.target, sp, "link", wrote))
                else:
                    work.append((a + 8, sp, "link", wrote))
            else:                                          # j — control leaves, does not come back
                work.append((i.target, sp, ra, wrote))
            continue
        if i.kind == D.JUMPR:
            continue                                       # computed jump: out of scope for this walk
        a += 4
        work.append((a, sp, ra, wrote))
    return ("unknown", saw_unknown) if saw_unknown else ("ok", "")


def main():
    exe, funcs = load()
    live = list(funcs)
    demoted = {RULE1}                      # rule 1's result, taken as given
    live = [f for f in live if f not in demoted]

    # ONLY `jal`-REACHED ENTRIES ARE CANDIDATES. Dropping this filter was an implementation bug and
    # the numbers caught it: without it the scan flags 124 of 738, path-sensitivity discharging
    # almost nothing. The reason is that a mid-body continuation reached ONLY by branches is
    # unrepresentable-as-a-callee too — trivially, since it is not a function start — but nothing
    # ever CALLS it, so the emitter's tail-duplication already handles it and demoting it would be
    # both unnecessary and wrong. The contract only binds where a `jal` actually demands it.
    called = set()
    for a in range(exe.load, exe.text_end, 4):
        try:
            i = decode(a, exe.word(a))
        except IndexError:
            break
        if i.op in ("jal", "bltzal", "bgezal") and i.target is not None:
            called.add(i.target)

    cands, viol, unk = [], [], []
    for idx, a in enumerate(live):
        end = live[idx + 1] if idx + 1 < len(live) else exe.text_end
        if has_prologue(exe, a) or a not in called:
            continue                       # own frame, or never called: not a candidate
        cands.append(a)
        verdict, why = walk(exe, a, end, live, demoted)
        if verdict == "violation":
            viol.append((a, why))
        elif verdict == "unknown":
            unk.append((a, why))

    print(f"corpus: {len(funcs)} functions; {len(live)} live after rule 1 demoted "
          f"0x{RULE1:08X}; {len(cands)} no-prologue candidates examined")
    print(f"blind spots, stated so a small number is not mistaken for certainty: computed jumps "
          f"(`jr` through a non-$ra register) end a path rather than being followed, and paths whose "
          f"balance cannot be proven are reported UNKNOWN below rather than counted either way.")
    print()
    print(f"VIOLATIONS ({len(viol)}) — cannot be emitted as a host callee:")
    for a, why in viol:
        tag = "  <-- WANTED" if a == WANT_VIOLATION else ("  <-- MUST-KEEP!" if a in MUST_KEEP else "")
        print(f"   0x{a:08X}  {why}{tag}")
    if not viol:
        print("   (none — which would be a FAILURE of this scan, not a clean result: "
              f"0x{WANT_VIOLATION:08X} is known to violate)")
    print()
    print(f"UNCLASSIFIABLE ({len(unk)}) — refused rather than defaulted:")
    for a, why in unk[:12]:
        print(f"   0x{a:08X}  {why}")
    if len(unk) > 12:
        print(f"   … and {len(unk) - 12} more")
    print()

    ok = True
    if WANT_VIOLATION not in {a for a, _ in viol}:
        ok = False
        print(f"FAIL: 0x{WANT_VIOLATION:08X} (the coroutine yield) was NOT flagged")
    else:
        print(f"ok: 0x{WANT_VIOLATION:08X} flagged")
    hit = {a for a, _ in viol} & set(MUST_KEEP)
    if hit:
        ok = False
        for a in sorted(hit):
            print(f"FAIL: must-keep 0x{a:08X} flagged — {MUST_KEEP[a]}")
    else:
        print(f"ok: all {len(MUST_KEEP)} must-keeps spared")
    print()
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def selftest():
    """Prove the walk can produce BOTH answers — a checker hardwired to "ok" would pass almost every
    real entry by accident, since most functions are balanced."""
    class Fake:
        load, text_end = 0x80000000, 0x80000000 + 0x100

        def __init__(self, w):
            self.w = w

        def word(self, a):
            o = (a - self.load) // 4
            if o < 0 or o >= len(self.w):
                raise IndexError(a)
            return self.w[o]

    # balanced leaf: jr $ra ; nop            -> ok
    ok_w = [0x03E00008, 0x00000000] + [0] * 8
    v, _ = walk(Fake(ok_w), 0x80000000, 0x80000008, [], set())
    assert v == "ok", f"balanced leaf should be ok, got {v}"

    # pops a frame it never pushed: lw $ra,0(sp) ; addiu sp,sp,4 ; jr $ra  -> violation
    bad_w = [0x8FBF0000, 0x27BD0004, 0x03E00008, 0x00000000] + [0] * 6
    v, why = walk(Fake(bad_w), 0x80000000, 0x80000010, [], set())
    assert v == "violation", f"unbalanced pop should violate, got {v} ({why})"

    # saves then restores its OWN slot: sw $ra,0(sp) ; lw $ra,0(sp) ; jr $ra -> ok
    bal_w = [0xAFBF0000, 0x8FBF0000, 0x03E00008, 0x00000000] + [0] * 6
    v, _ = walk(Fake(bal_w), 0x80000000, 0x80000010, [], set())
    assert v == "ok", f"save+restore of its own slot should be ok, got {v}"

    print("SELFTEST PASS: walk separates balanced leaf / unbalanced pop / save-and-restore")
    return 0


if __name__ == "__main__":
    sys.exit(selftest() if "--selftest" in sys.argv else main())
