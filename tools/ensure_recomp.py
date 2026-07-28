#!/usr/bin/env python3
"""ensure_recomp.py — the single, hash-checked recompilation step.

ONE entry point that guarantees the statically-recompiled substrate in generated/ is PRESENT and
matches a deterministic hash of its INPUTS. run.sh calls only this; all recomp provisioning lives
here rather than scattered through the shell script.

What it does, in order:
  1. Resolve the disc image (CLI arg > $PSXPORT_SPIDERMAN_DISC > .env > *.chd drop-in — this
     mirrors run.sh exactly, so both agree on which disc is in play).
  2. Extract the boot executable SLUS_008.75 from the disc via the framework's `discdump`.
     Spider-Man ships ONE executable and NO overlay modules — SYSTEM.CNF boots SLUS_008.75
     directly, and the rest of the game lives in the packed archive CD.WAD, which the recompiler
     does not consume. So unlike Tomba!2 (BIN/START|DEMO|GAME|A00..A0L) there is nothing else to
     extract, and emit.py reports "0 overlay module(s)".
  3. Compute the recomp IDENTITY = emit.py's RECOMP_VERSION + a hash of the INPUTS (the executable
     plus the recompiler module sources). If the stored identity (generated/.recomp.hash) matches,
     the on-disk version stamp matches RECOMP_VERSION, AND the generated set is complete, do
     nothing. Otherwise re-run emit.py and rewrite the identity.

     RECOMP_VERSION is the EXPLICIT staleness knob: bumping it in emit.py forces every machine to
     regenerate, which catches a stale-but-self-consistent generated/ that an input hash alone
     would happily accept.

Usage: python3 tools/ensure_recomp.py [/path/to/disc.chd]
Env:   PSXPORT_SPIDERMAN_DISC (disc path), PSXPORT_DISCDUMP (discdump binary override),
       PSXPORT_FORCE_RECOMP=1 (ignore the hash and always re-emit).
Exit:  0 on success (recomp present & current), non-zero with a diagnostic on any failure.
"""
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The recompiler lives in the psxport framework submodule. A change to any of these modules changes
# the emitted C, so they are hash inputs alongside the game executable.
RECOMP_DIR = "external/psxport/tools/recomp"
RECOMP_SRCS = [f"{RECOMP_DIR}/emit.py", f"{RECOMP_DIR}/decode.py", f"{RECOMP_DIR}/psexe.py"]

EXE_NAME = "SLUS_008.75"          # the retail US boot executable, per SYSTEM.CNF
EXE = f"scratch/bin/spiderman/{EXE_NAME}"
GEN_DIR = "generated"
GEN_MAIN = "generated/spiderman_rec.c"
HASH_FILE = "generated/.recomp.hash"
VERSION_FILE = "generated/.recomp_version"


def say(msg):
    sys.stderr.write(f"\033[1;36m[ensure-recomp]\033[0m {msg}\n")


def die(msg):
    sys.stderr.write(f"\033[1;31m[ensure-recomp] error:\033[0m {msg}\n")
    sys.exit(1)


def recomp_version():
    """The RECOMP_VERSION constant declared in the recompiler's emit.py, read TEXTUALLY so we don't
    import the whole recompiler just for one string."""
    src = open(os.path.join(ROOT, f"{RECOMP_DIR}/emit.py")).read()
    m = re.search(r'^RECOMP_VERSION\s*=\s*"([^"]+)"', src, re.M)
    if not m:
        die(f"could not read RECOMP_VERSION from {RECOMP_DIR}/emit.py")
    return m.group(1)


def resolve_disc(argv):
    """CLI arg > $PSXPORT_SPIDERMAN_DISC > .env (PSXPORT_SPIDERMAN_DISC|PSXPORT_DISC) > *.chd drop-in."""
    disc = argv[1] if len(argv) > 1 and argv[1] else os.environ.get("PSXPORT_SPIDERMAN_DISC", "")
    if not disc and os.path.isfile(os.path.join(ROOT, ".env")):
        env = open(os.path.join(ROOT, ".env")).read()
        for key in ("PSXPORT_SPIDERMAN_DISC", "PSXPORT_DISC"):
            m = re.search(rf"^\s*{key}\s*=\s*(.+?)\s*$", env, re.M)
            if m:
                disc = m.group(1)
                break
    if not disc:
        chds = sorted(p for p in os.listdir(ROOT) if p.lower().endswith(".chd"))
        if chds:
            disc = os.path.join(ROOT, chds[0])
    if not disc or not os.path.isfile(disc):
        die("no disc image — pass it as ./run.sh <disc.chd>, set PSXPORT_SPIDERMAN_DISC, "
            "or drop a *.chd here")
    return disc


def find_discdump():
    cand = os.environ.get("PSXPORT_DISCDUMP", "")
    if cand and os.access(cand, os.X_OK):
        return cand
    # discdump is a FRAMEWORK tool, so it builds into the submodule's own build tree; the top-level
    # paths are kept too for a unified configure.
    for p in ("external/psxport/build/tools/discdump", "external/psxport/build/tools/discdump.exe",
              "build/tools/discdump", "build/tools/discdump.exe"):
        full = os.path.join(ROOT, p)
        if os.access(full, os.X_OK):
            return full
    die("discdump not built — run.sh builds it before calling ensure_recomp.py "
        "(cmake --build external/psxport/build --target discdump)")


def extract(discdump, disc, disc_path, dest_dir):
    """Pull one file off the disc into dest_dir, if not already present. Never swallows discdump's
    diagnostic — a missing executable is a build-breaker, not a warning to bury."""
    out = os.path.join(ROOT, dest_dir, os.path.basename(disc_path))
    if os.path.isfile(out):
        return out
    os.makedirs(os.path.join(ROOT, dest_dir), exist_ok=True)
    r = subprocess.run([discdump, "get", disc_path, disc, os.path.join(ROOT, dest_dir)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0 or not os.path.isfile(out):
        err = (r.stderr or b"").decode(errors="replace").strip()
        tree = subprocess.run([discdump, "list", disc], stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
        die(f"could not extract {disc_path} from the disc"
            + (f"\n  discdump: {err}" if err else "")
            + "\n  disc tree:\n" + (tree.stdout or b"").decode(errors="replace"))
    return out


def input_hash():
    """SHA-256 over the game executable + the recompiler sources."""
    h = hashlib.sha256()

    def feed(label, path):
        h.update(label.encode())
        with open(path, "rb") as f:
            h.update(f.read())

    feed(EXE_NAME, os.path.join(ROOT, EXE))
    for src in RECOMP_SRCS:
        feed(src, os.path.join(ROOT, src))
    return h.hexdigest()


def generated_complete():
    """The generated set is complete iff the manifest exists and every TU it lists is present."""
    manifest = os.path.join(ROOT, GEN_DIR, "rec_sources.cmake")
    for f in (manifest, os.path.join(ROOT, GEN_DIR, "shard_disp.c"),
              os.path.join(ROOT, GEN_MAIN), os.path.join(ROOT, GEN_DIR, "overlay_table.c")):
        if not os.path.isfile(f):
            return False
    listed = re.findall(r"^\s*(\S+\.c)\s*$", open(manifest).read(), re.M)
    return all(os.path.isfile(os.path.join(ROOT, GEN_DIR, tu)) for tu in listed)


def run_emit():
    say("recompiling SLUS_008.75 -> C (the execution substrate)…")
    cmd = [sys.executable, os.path.join(ROOT, f"{RECOMP_DIR}/emit.py"),
           os.path.join(ROOT, EXE), os.path.join(ROOT, GEN_MAIN)]
    if subprocess.run(cmd).returncode != 0:
        die("emit.py failed")


def main():
    disc = resolve_disc(sys.argv)
    say(f"disc: {disc}")
    discdump = find_discdump()

    extract(discdump, disc, EXE_NAME, "scratch/bin/spiderman")

    os.makedirs(os.path.join(ROOT, GEN_DIR), exist_ok=True)
    want = recomp_version() + ":" + input_hash()
    have = ""
    if os.path.isfile(os.path.join(ROOT, HASH_FILE)):
        have = open(os.path.join(ROOT, HASH_FILE)).read().strip()

    if (not os.environ.get("PSXPORT_FORCE_RECOMP")) and have == want and generated_complete():
        say("recomp up to date")
        return

    run_emit()
    if not generated_complete():
        die("emit.py finished but the generated set is incomplete")
    with open(os.path.join(ROOT, HASH_FILE), "w") as f:
        f.write(want + "\n")
    with open(os.path.join(ROOT, VERSION_FILE), "w") as f:
        f.write(recomp_version() + "\n")
    say("recomp complete")


if __name__ == "__main__":
    main()
