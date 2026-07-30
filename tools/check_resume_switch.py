#!/usr/bin/env python3
"""Soundness gate for the recompiler's `jr $ra` RESUME SWITCH (see docs/re-frontier.md RE-16).

WHAT IT CHECKS, and why it is a soundness property rather than a lint.

When a guest body uses the intra-frame coroutine idiom (`jal` into its own body, `jr $ra` back), the
emitter can render the `jr $ra` as a switch over the body's own resume addresses:

    switch (c->r[31]) { case 0x8002A77C: goto L_8002A77C; ... default: return; }

That is sound ONLY while a body's case labels are disjoint from the addresses that HOST CALLS link.
`c->r[31]` is guest state, and the guest has one address space while the emitted artifact has several
host instantiations of it (tail-duplicated regions, plus pending host continuations). So `c->r[31]`
cannot discriminate "resume inside me" from "return to my host caller" once an address is used for
both — and on a collision the switch STEALS a genuine return: the callee runs the caller's remaining
work itself, then `default: return`s, and the caller re-executes that work on a guest that has already
finished it and freed its frame. Measured consequence in this port: `sp` skews 4 bytes per spurious
pass and callee-saved reloads come back full of bitstream data (`ra=0x03FF03FF`, `s0=0x03FF07FF`).

    exit 0 = no collision   exit 1 = collision (with addresses)   exit 2 = the scan could not run

THE NEGATIVE IS THE POINT. This prints its denominators next to its verdict, always. The FIRST version
of this scan required the link assignment and the call to be on ONE line, but the emitter appends them
as TWO, so it matched 18 links instead of ~17000 and reported a confident "0 collisions" over a
substrate that had 28. A bare "no collisions" is indistinguishable from "I never looked", so an empty
link set is a HARD ERROR (exit 2), never a pass. See docs/info/instruments.md I004.

Usage:  tools/check_resume_switch.py [generated_dir]        (default: generated/)
        tools/check_resume_switch.py --selftest             (proves it can report a POSITIVE)
"""
import collections
import glob
import os
import re
import sys

CASE = re.compile(r'case 0x([0-9A-Fa-f]{8})u: goto L_')
SWITCH = re.compile(r'switch \(c->r\[31\]\)')
LINK = re.compile(r'c->r\[31\] = 0x([0-9A-Fa-f]{8})u;')
CALL = re.compile(r'\b(?:\w*func_[0-9A-Fa-f]{8}|\w*dispatch)\s*\(c')
FUNC = re.compile(r'^\w[\w \*]*\b((?:\w+_)?func_[0-9A-Fa-f]{8})\s*\(')


def scan(paths):
    """-> (cases_by_body, link_addrs, stats). A link is a `c->r[31] = <addr>;` whose CALL sits on the
    same line or the NEXT one — emit_control appends the two as separate lines."""
    cases_by_body = collections.defaultdict(set)
    links, stats = set(), collections.Counter()
    for path in paths:
        with open(path) as fh:
            lines = fh.readlines()
        stats["files"] += 1
        body = None
        for n, line in enumerate(lines):
            m = FUNC.match(line)
            if m:
                body = m.group(1)
            if SWITCH.search(line):
                stats["switch_sites"] += 1
                cases_by_body[body or os.path.basename(path)] |= {
                    int(x, 16) for x in CASE.findall(line)}
            for lm in LINK.finditer(line):
                tail = line[lm.end():] + (lines[n + 1] if n + 1 < len(lines) else "")
                if CALL.search(tail):
                    links.add(int(lm.group(1), 16))
                    stats["link_sites"] += 1
    return cases_by_body, links, stats


def report(cases_by_body, links, stats, where):
    all_cases = set().union(*cases_by_body.values()) if cases_by_body else set()
    print(f"[resume-switch] scanned {stats['files']} file(s) in {where}: "
          f"{stats['switch_sites']} resume switch(es) in {len(cases_by_body)} bodies, "
          f"{len(all_cases)} distinct case address(es); "
          f"{stats['link_sites']} host-call link site(s), {len(links)} distinct.")

    # REFUSE rather than pass when the corpus or a whole matcher came up empty. Either one makes the
    # verdict void, and a void verdict that prints as "OK" is worse than no check at all.
    if not stats["files"]:
        print(f"[resume-switch] ERROR: no .c files under {where} — scanned NOTHING.", file=sys.stderr)
        return 2
    if not links:
        print("[resume-switch] ERROR: the host-call-link matcher found ZERO links across "
              f"{stats['files']} files. That cannot be true of a real substrate, so the emitter's "
              "output shape has changed and this scan is blind. Verdict is VOID, not clean.",
              file=sys.stderr)
        return 2
    if not all_cases:
        print("[resume-switch] no resume switches emitted at all — nothing to collide. "
              "(Vacuously clean: this substrate does not use the in-body `jal` rendering.)")
        return 0

    collisions = {b: (c & links) for b, c in cases_by_body.items() if c & links}
    if not collisions:
        print(f"[resume-switch] OK: none of the {len(all_cases)} case address(es) is also a "
              f"host-call link. Blind spot: this reads the EMITTED C only, so it says nothing about "
              f"a configuration that was never emitted.")
        return 0

    total = len({a for c in collisions.values() for a in c})
    print(f"[resume-switch] UNSOUND: {total} address(es) across {len(collisions)} body(ies) are BOTH "
          f"a resume case and a host-call link. On each, the switch steals a genuine return and the "
          f"caller re-executes guest work already done.", file=sys.stderr)
    for body in sorted(collisions, key=str):
        addrs = " ".join(f"0x{a:08X}" for a in sorted(collisions[body]))
        print(f"    {body:<28} {addrs}", file=sys.stderr)
    print("  Fix: demote the colliding entry into the body that owns its frame, so the complex is "
          "ONE host function. Do not silence this by narrowing the switch.", file=sys.stderr)
    return 1


def selftest():
    """Prove the scan can report a POSITIVE — a check that can only ever print 'OK' is not a check.
    Feeds one body whose case address is also a link, and one clean body, and requires both verdicts."""
    import tempfile
    bad = ("void gen_func_8002A338(Core* c) {\n"
           "  c->r[31] = 0x8002A77Cu;\n"
           "  func_8002A5F4(c);\n"
           "}\n"
           "void gen_func_8002A5F4(Core* c) {\n"
           "  { switch (c->r[31]) { case 0x8002A77Cu: goto L_8002A77C; default: return; } }\n"
           "}\n")
    clean = ("void gen_func_80001000(Core* c) {\n"
             "  c->r[31] = 0x80001234u;\n"
             "  func_80005000(c);\n"
             "}\n"
             "void gen_func_80005000(Core* c) {\n"
             "  { switch (c->r[31]) { case 0x80009999u: goto L_80009999; default: return; } }\n"
             "}\n")
    ok = True
    for name, text, want in (("collide", bad, 1), ("clean", clean, 0)):
        with tempfile.TemporaryDirectory() as d:
            open(os.path.join(d, "shard_0.c"), "w").write(text)
            got = report(*scan(glob.glob(os.path.join(d, "*.c"))), where=d)
        verdict = "PASS" if got == want else "FAIL"
        if got != want:
            ok = False
        print(f"[selftest] {name}: expected exit {want}, got {got} -> {verdict}\n")
    # And the void case: a directory with no .c files must REFUSE, not report clean.
    with tempfile.TemporaryDirectory() as d:
        got = report(*scan(glob.glob(os.path.join(d, "*.c"))), where=d)
    print(f"[selftest] empty-corpus: expected exit 2, got {got} -> "
          f"{'PASS' if got == 2 else 'FAIL'}")
    ok = ok and got == 2
    return 0 if ok else 1


def main(argv):
    if "--selftest" in argv:
        return selftest()
    where = argv[1] if len(argv) > 1 else "generated"
    return report(*scan(sorted(glob.glob(os.path.join(where, "*.c")))), where=where)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
