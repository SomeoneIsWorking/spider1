#!/usr/bin/env python3
"""ensure_recomp.py — the single, hash-checked recompilation step.

ONE entry point that guarantees the statically-recompiled substrate in generated/ is PRESENT and
matches a deterministic hash of its INPUTS. run.sh calls only this; all recomp provisioning lives
here rather than scattered through the shell script.

What it does, in order:
  1. Resolve the disc image (CLI arg > $PSXPORT_SPIDERMAN_DISC > .env > *.chd drop-in — this
     mirrors run.sh exactly, so both agree on which disc is in play).
  2. Extract the boot executable SLUS_008.75 from the disc via the framework's `discdump`.
     SYSTEM.CNF boots SLUS_008.75 directly; the rest of the game lives in the packed archive
     CD.WAD, from which step 2b pulls 30 runtime-loaded code modules (see MODULE_SRCS below).
  3. Compute the recomp IDENTITY = emit.py's RECOMP_VERSION + a hash of the INPUTS (the executable,
     this game's seed file, and the recompiler module sources). The seed set is a GAME fact the
     framework no longer ships, and changing it changes the emitted function set, so it is an input
     like the executable. If the stored identity (generated/.recomp.hash) matches,
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract_modules  # noqa: E402  (same directory; see MODULE_SRCS)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The recompiler lives in the psxport framework submodule. A change to any of these modules changes
# the emitted C, so they are hash inputs alongside the game executable.
# WHICH framework checkout the recompiler comes from is the same decision CMake already makes: PSXPORT_DIR
# selects it and defaults to the vendored submodule, so a bare clone still provisions standalone. Hardcoding
# the submodule meant the substrate could ONLY be regenerated from the recorded pin, so in-progress
# framework work on the recompiler was unverifiable end-to-end — and worse, this tool would hash a DIFFERENT
# emit.py than the one being edited and report "up to date". That cost two false "up to date" results in
# Tomba2Engine (fixed there 2026-08-12); this is the same latent defect in this repo.
PSXPORT_DIR = os.environ.get("PSXPORT_DIR", "external/psxport")
RECOMP_DIR = f"{PSXPORT_DIR}/tools/recomp"
RECOMP_SRCS = [f"{RECOMP_DIR}/emit.py", f"{RECOMP_DIR}/decode.py", f"{RECOMP_DIR}/psexe.py"]
# The module extractor decides the CONTENT of every overlay the recompiler is fed (which bytes, and
# what base they are relocated to), so it is as much a recomp input as emit.py itself.
MODULE_SRCS = ["tools/extract_modules.py"]

EXE_NAME = "SLUS_008.75"          # the retail US boot executable, per SYSTEM.CNF
EXE = f"scratch/bin/spiderman/{EXE_NAME}"
# The recompiler seed set is a GAME fact, supplied by this repo — the framework ships none. A change
# to it changes the emitted function set, so it is a hash input like the executable itself.
SEEDS = "game/recomp_seeds.json"

# Runtime-loaded code modules. SLUS_008.75 is NOT the whole game: further CODE lives in the packed
# archive CD.WAD as <name>.bin + <name>.rel pairs, loaded and relocated at runtime by the game's own
# loader (FUN_8001B990). tools/extract_modules.py is the offline half of that loader — it reads the
# CD.HED index, pulls each module out of CD.WAD, and applies the .rel relocations at the module's
# load base, producing an image the recompiler can treat as an ordinary overlay. Without this, the
# first call into a module aborts with a recomp MISS. See docs/re-frontier.md RE-09.
HED_NAME = "CD.HED"
WAD_NAME = "CD.WAD"
WAD_DIR = "scratch/wad"
OVERLAY_DIR = "scratch/overlays"
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
    """SHA-256 over the game executable + this game's seed file + the recompiler sources."""
    h = hashlib.sha256()

    def feed(label, path):
        h.update(label.encode())
        with open(path, "rb") as f:
            h.update(f.read())

    feed(EXE_NAME, os.path.join(ROOT, EXE))
    feed(SEEDS, os.path.join(ROOT, SEEDS))
    for src in RECOMP_SRCS + MODULE_SRCS:
        feed(src, os.path.join(ROOT, src))
    # The relocated module images are recomp INPUTS exactly like the executable: change a load base
    # or a module's disc contents and the emitted substrate must change with it.
    ov = os.path.join(ROOT, OVERLAY_DIR)
    for name in sorted(os.listdir(ov)) if os.path.isdir(ov) else []:
        if name.endswith(".bin"):
            feed(f"{OVERLAY_DIR}/{name}", os.path.join(ov, name))
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
           os.path.join(ROOT, EXE), os.path.join(ROOT, GEN_MAIN),
           "--seeds", os.path.join(ROOT, SEEDS),
           "--overlays", os.path.join(ROOT, OVERLAY_DIR)]
    if subprocess.run(cmd).returncode != 0:
        die("emit.py failed")


def main():
    disc = resolve_disc(sys.argv)
    say(f"disc: {disc}")
    discdump = find_discdump()

    extract(discdump, disc, EXE_NAME, "scratch/bin/spiderman")

    # Runtime-loaded modules: extract + relocate BEFORE the hash is taken, since the relocated images
    # are recomp inputs. Both archive files are large-ish, so extract() caches them in scratch/.
    hed = extract(discdump, disc, HED_NAME, WAD_DIR)
    wad = extract(discdump, disc, WAD_NAME, WAD_DIR)
    say("extracting runtime-loaded code modules from CD.WAD…")
    try:
        extract_modules.extract(hed, wad, os.path.join(ROOT, OVERLAY_DIR))
    except (KeyError, ValueError, OSError) as e:
        die(f"could not prepare the runtime-loaded modules: {e}")

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
