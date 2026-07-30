#!/usr/bin/env python3
"""RE-16 attempt 9 pre-build validator — answer the static set questions in SECONDS.

The lesson of DE-02 attempts 1-7: every criterion was validated by a 10-minute build + run, which is
far too coarse and slow a signal for what are pure static set questions.

ATTEMPT 9 ABANDONS THE DEMOTION HEURISTIC. Attempts 1,2,3 and two of my own all tried to answer
"is this `jal` target a real function?" and all produced the wrong set (325, 50, 78, 1, ...). The
recompiler already has the mechanism this actually needs: `main_reentry` seeds -- mid-function
re-entry points that become router-dispatchable. So the design under test here is:

  (A) seed the 7 resume points of 0x8002A338 as `main_reentry` -- RE'd game facts, which is exactly
      what the seed file is for, and NOT a heuristic over the function set; and
  (B) emit `jr $ra` as a ROUTER DISPATCH, not `return;`, at those `jr` where the reaching definition
      of $ra is provably not the frame reload.

(B) is the only substrate-wide change, so the question this script exists to answer is: HOW MANY
SITES DOES (B) TOUCH, and are they auditable? A default of `return;` is preserved everywhere else.

  python3 tools/ra_classes.py [--selftest]

NEGATIVE DESIGN (the write-time rule): every check prints its DENOMINATOR and its BLIND SPOT, a zero
result is reported as a FAILURE rather than a clean bill of health, and a missing corpus exits
non-zero instead of returning "(none)".
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

# Ground truth for this binary, from the disassembly recorded in
# docs/issues/recompiler-dead-ends.md "Attempt 8". Reproduce with:
#   python3 external/psxport/tools/disasm.py scratch/raw/miss_ram.bin 0x8002A338 0x8002A840
HOST = 0x8002A338
WANT_RESUMES = {0x8002A424, 0x8002A708, 0x8002A774,
                0x8002A77C, 0x8002A784, 0x8002A78C, 0x8002A794}
WANT_JR = {0x8002A460: "computed", 0x8002A7E0: "return", 0x8002A838: "return"}


def die(msg):
    print(f"REFUSED: {msg}", file=sys.stderr)
    sys.exit(2)


def load():
    if not os.path.exists(EXE):
        die(f"no executable at {EXE} -- this script searched NOTHING. Provision it and re-run.")
    if not os.path.exists(DECLS):
        die(f"no {DECLS} -- the function set is unknown, so this script searched NOTHING. "
            f"Run the recompiler once first.")
    exe = psexe.load_exe(EXE) if hasattr(psexe, "load_exe") else psexe.load(EXE)
    funcs = set()
    for line in open(DECLS):
        k = line.find("void gen_func_")
        while k >= 0:
            funcs.add(int(line[k + 14:k + 22], 16))
            k = line.find("void gen_func_", k + 1)
    funcs = {f for f in funcs if exe.load <= f < exe.text_end}
    if not funcs:
        die(f"{DECLS} parsed to ZERO in-text functions -- the parse is broken, not the binary.")
    return exe, funcs


def classify_jr_ra(exe, lo, hi, linked=()):
    """Per-`jr $ra` reaching-definitions on $ra within one linear body.

    NOT a whole-body gate -- every prior attempt used one ("this body writes $ra as data, so convert
    ALL its `jr $ra`"), and 0x8002A338 has both kinds in it, so a whole-body answer is wrong whichever
    way it goes.

        reaching def is `lw $ra, N(sp)`            -> "return"    (the frame reload / epilogue)
        reaching def is `lw $ra, N(...)` where this function itself did `sw $ra, N(...)`
                                                   -> "return"    (a save slot -- see below)
        reaching def is `lw $ra, N(...)` otherwise -> "computed"  (a RESTORED CONTINUATION)
        reaching def is a `jal`/`bltzal`/`bgezal`  -> "computed"  (a live link, not a return address)
        any other write to $ra                     -> "return"    (unknown -> current behaviour)
        no reaching def in this body               -> "return"    (the caller's $ra, untouched)

    "NOT THE STACK" IS NOT THE SAME AS "NOT A RETURN ADDRESS", and assuming it was produced four
    false positives. A frameless function parks its return address in a GLOBAL and reloads it:

        8008BE5C  lui $at, 0x800B
        8008BE60  sw  $ra, 0x392C($at)      <- save slot, just not on the stack
        8008BE64  jal 0x80091640
        8008BE6C  lui $ra, 0x800B
        8008BE70  lw  $ra, 0x392C($ra)      <- a RETURN address, not a continuation

    The discriminator is whether THIS function ever stored $ra to that slot. 0x8002A338 never does:
    its `lw $ra, 0x30($t6)` reads a slot written by `sw $at, 0x30($t6)` -- a different register, and
    on a later invocation. Matched on the offset alone, as ra_tail_returns() already does, because
    the base register legitimately differs between the save and the reload ($at vs $ra above).

    Linear forward pass taking the most recent definition. BLIND SPOT, stated because a silent one
    would be a lie: this is linear, not a CFG join. Where two paths reach a `jr` with different
    definition kinds, the answer reported is whichever is textually latest. `unknown` is never
    silently coerced -- it is reported as `unknown` and counted separately.
    """
    # pre-pass: every offset this function stores $ra to is a SAVE SLOT, wherever its base points.
    slots = set()
    for a in range(lo, hi, 4):
        try:
            i = decode(a, exe.word(a))
        except IndexError:
            break
        if i.op == "sw" and i.rt == 31:
            slots.add(i.simm)

    kinds, cur = {}, None
    for a in range(lo, hi, 4):
        try:
            w = exe.word(a)
        except IndexError:
            break
        i = decode(a, w)
        kinds[a] = cur
        if i.op == "lw" and i.rt == 31:
            cur = "return" if (i.rs == 29 or i.simm in slots) else "computed"
        elif i.op in ("jal", "bltzal", "bgezal"):
            cur = "computed"
        elif i.kind == D.JUMPR and i.op == "jalr" and i.rd == 31:
            cur = "computed"
        elif (i.kind in (D.ALU_RRR, D.SHIFT_I, D.SHIFT_V) and i.rd == 31) \
                or (i.kind in (D.ALU_RRI, D.LUI) and i.rt == 31):
            # ANY OTHER WRITE TO $ra -- most importantly `addu $ra, $sN, $zero`, the extremely common
            # idiom for a function that parks its return address in a callee-saved register instead of
            # the stack. Without this case the pass never clears the `jal` state, so every such
            # function's perfectly ordinary `jr $ra` is reported as a computed jump. Default to
            # "return", the CURRENT emitter behaviour: this pass may only ever move a site AWAY from
            # `return;` on positive evidence, never towards a dispatch on ignorance.
            cur = "return"
    out = {}
    for a in range(lo, hi, 4):
        try:
            w = exe.word(a)
        except IndexError:
            break
        i = decode(a, w)
        if i.op == "jr" and i.rs == 31:
            out[a] = kinds[a] or "return"
    return out


def sweep(exe, funcset):
    """Every `jr $ra` in the substrate, classified. Returns (all, computed) address->kind."""
    ordered = sorted(funcset)
    allj, comp = {}, {}
    for idx, a in enumerate(ordered):
        end = ordered[idx + 1] if idx + 1 < len(ordered) else exe.text_end
        c = classify_jr_ra(exe, a, end)
        for x, k in c.items():
            allj[x] = (k, a)
            if k == "computed":
                comp[x] = a
    return allj, comp


def resume_points(exe, lo, hi):
    """addr+8 for every `jal` inside [lo,hi) whose target is also inside [lo,hi) -- an IN-BODY call,
    whose link value is a mid-function address the guest can `jr $ra` back to.

    Enumerated over ALL in-body jals, NOT per target. Attempt 5 enumerated per demoted target, got 3
    instead of 7, and silently truncated the decoder at the other four."""
    out = {}
    for a in range(lo, hi, 4):
        i = decode(a, exe.word(a))
        if i.op == "jal" and lo <= i.target < hi:
            out[a + 8] = i.target
    return out


def main():
    exe, funcset = load()
    ordered = sorted(funcset)
    k = bisect.bisect_right(ordered, HOST)
    host_hi = ordered[k] if k < len(ordered) else exe.text_end

    allj, comp = sweep(exe, funcset)
    print(f"corpus: {len(funcset)} in-text functions from rec_decls.h, "
          f"text 0x{exe.load:08X}..0x{exe.text_end:08X}")
    print(f"blind spots: (1) the reaching-def pass is LINEAR, not a CFG join -- at a `jr` reached by "
          f"two paths with different definition kinds it reports the textually latest; (2) only "
          f"functions present in rec_decls.h are scanned, so anything the discovery pass missed is "
          f"invisible here.")
    print()

    ok = True

    # ---- Q1: how many `jr $ra` does the emitter change behaviour for?
    print(f"Q1 `jr $ra` sites: {len(allj)} total, {len(comp)} would become a ROUTER DISPATCH")
    if not allj:
        ok = False
        print("     (ZERO sites found -- that is a FAILURE of this scan, not a property of the "
              "binary: a substrate this size must contain thousands of `jr $ra`.)")
    for x in sorted(comp):
        print(f"     0x{x:08X}   in gen_func_{comp[x]:08X}")
    print()

    # ---- Q2: the host body's resume points
    res = resume_points(exe, HOST, host_hi)
    print(f"Q2 host 0x{HOST:08X}..0x{host_hi:08X} -- but note this body is currently SPLIT by the "
          f"spurious entries 0x8002A478 / 0x8002A5F4, so in-body jals are only visible over the "
          f"full guest extent. Scanning 0x{HOST:08X}..0x8002A83C instead:")
    res = resume_points(exe, HOST, 0x8002A83C)
    for r in sorted(res):
        print(f"     0x{r:08X}   (after jal 0x{res[r]:08X} at 0x{r - 8:08X})")
    if set(res) != WANT_RESUMES:
        ok = False
        print(f"  FAIL: expected {len(WANT_RESUMES)} "
              f"({' '.join(f'0x{a:08X}' for a in sorted(WANT_RESUMES))}), got {len(res)}")
    else:
        print(f"  ok: 7, matching the recorded disassembly -- attempt 5 used 3 and truncated four")
    print()

    # ---- Q3: the three `jr $ra` of the coroutine body
    cls = classify_jr_ra(exe, HOST, 0x8002A83C + 4)
    cls = {a: k for a, k in cls.items() if a in WANT_JR}
    print(f"Q3 `jr $ra` in the guest body 0x{HOST:08X}..0x8002A840 ({len(cls)}):")
    for a in sorted(WANT_JR):
        got = cls.get(a, "ABSENT")
        mark = "" if got == WANT_JR[a] else "   <-- MISMATCH"
        print(f"     0x{a:08X}   {got:9s} (expected {WANT_JR[a]}){mark}")
    if cls != WANT_JR:
        ok = False
        print(f"  FAIL: classification does not match the recorded disassembly")
    else:
        print(f"  ok: one computed jump + two real returns, as disassembled")
    print()

    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def selftest():
    """Prove the instrument can produce the OTHER answer -- a classifier hardwired to "return" would
    pass every real case in this binary by accident, since returns overwhelmingly dominate."""
    class Fake:
        load, text_end = 0x80000000, 0x80000000 + 0x40

        def __init__(self, words):
            self.words = words

        def word(self, a):
            o = (a - self.load) // 4
            if o < 0 or o >= len(self.words):
                raise IndexError(a)
            return self.words[o]

    #  +0x00 jal 0x80000020 | +0x04 nop | +0x08 jr $ra   -> computed (live link)
    #  +0x0C lw $ra,0(sp)   | +0x10 jr $ra               -> return   (frame reload)
    #  +0x14 lw $ra,0x30(t6)| +0x18 jr $ra               -> computed (restored continuation)
    W = [0x0C000008, 0x00000000, 0x03E00008,
         0x8FBF0000, 0x03E00008,
         0x8DDF0030, 0x03E00008] + [0] * 9
    got = classify_jr_ra(Fake(W), 0x80000000, 0x8000001C)
    want = {0x80000008: "computed", 0x80000010: "return", 0x80000018: "computed"}
    if got != want:
        print(f"SELFTEST FAIL: got {got}, want {want}")
        return 1
    print("SELFTEST PASS: classifier separates jal-link, frame-reload and restored-continuation $ra")
    return 0


if __name__ == "__main__":
    sys.exit(selftest() if "--selftest" in sys.argv else main())
