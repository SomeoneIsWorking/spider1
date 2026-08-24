#!/usr/bin/env python3
"""ensure_recomp.py — the single, hash-checked recompilation step.

ONE entry point that guarantees the statically-recompiled substrate in generated/ is PRESENT and
matches a deterministic hash of its INPUTS. run.sh calls only this; all recomp provisioning lives
here rather than scattered through the shell script.

What it does, in order:
  1. Resolve the disc image through tools/disc_path.py (CLI arg >
     $PSXPORT_SPIDERMAN_DISC > .env > *.chd drop-in), so every tool agrees which disc is in play.
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

Usage: python3 tools/ensure_recomp.py [--title spiderman1|spiderman2] [/path/to/disc.chd]
Env:   the selected title manifest's disc key, PSXPORT_DISCDUMP (discdump binary override),
       PSXPORT_FORCE_RECOMP=1 (ignore the hash and always re-emit).
Exit:  0 on success (recomp present & current), non-zero with a diagnostic on any failure.
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract_modules
from disc_path import resolve_disc as resolve_disc_path
from title_catalog import Title, TitleCatalogError, load_catalog

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
RECOMP_SRCS = [
    f"{RECOMP_DIR}/emit.py",
    f"{RECOMP_DIR}/decode.py",
    f"{RECOMP_DIR}/psexe.py",
]
# The module extractor decides the CONTENT of every overlay the recompiler is fed (which bytes, and
# what base they are relocated to), so it is as much a recomp input as emit.py itself.
MODULE_SRCS = ["tools/extract_modules.py"]

# Runtime-loaded code modules. SLUS_008.75 is NOT the whole game: further CODE lives in the packed
# archive CD.WAD as <name>.bin + <name>.rel pairs, loaded and relocated at runtime by the game's own
# loader (FUN_8001B990). tools/extract_modules.py is the offline half of that loader — it reads the
# CD.HED index, pulls each module out of CD.WAD, and applies the .rel relocations at the module's
# load base, producing an image the recompiler can treat as an ordinary overlay. Without this, the
# first call into a module aborts with a recomp MISS. See docs/re-frontier.md RE-09.
HED_NAME = "CD.HED"
WAD_NAME = "CD.WAD"
SYSTEM_CNF = "SYSTEM.CNF"


def say(msg):
    sys.stderr.write(f"\033[1;36m[ensure-recomp]\033[0m {msg}\n")


def die(msg):
    sys.stderr.write(f"\033[1;31m[ensure-recomp] error:\033[0m {msg}\n")
    sys.exit(1)


def recomp_version():
    """The RECOMP_VERSION constant declared in the recompiler's emit.py, read TEXTUALLY so we don't
    import the whole recompiler just for one string."""
    src = (Path(ROOT) / RECOMP_DIR / "emit.py").read_text(encoding="utf-8")
    m = re.search(r'^RECOMP_VERSION\s*=\s*"([^"]+)"', src, re.MULTILINE)
    if not m:
        die(f"could not read RECOMP_VERSION from {RECOMP_DIR}/emit.py")
    return m.group(1)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--title", default="spiderman1")
    parser.add_argument("disc", nargs="?")
    return parser.parse_args(argv)


def resolve_disc(title: Title, argument: str | None):
    """Resolve and validate the disc selected by the shared tooling policy."""
    disc = resolve_disc_path(Path(ROOT), argument, os.environ, title.disc_env)
    if not disc or not os.path.isfile(disc):
        die(
            f"no disc image — pass it as ./run.sh <disc.chd>, set {title.disc_env}, "
            "or drop a *.chd here"
        )
    return disc


def find_discdump():
    cand = os.environ.get("PSXPORT_DISCDUMP", "")
    if cand and os.access(cand, os.X_OK):
        return cand
    # discdump is a FRAMEWORK tool, so it builds into the submodule's own build tree; the top-level
    # paths are kept too for a unified configure.
    for p in (
        "external/psxport/build/tools/discdump",
        "external/psxport/build/tools/discdump.exe",
        "build/tools/discdump",
        "build/tools/discdump.exe",
    ):
        full = os.path.join(ROOT, p)
        if os.access(full, os.X_OK):
            return full
    die(
        "discdump not built — run.sh builds it before calling ensure_recomp.py "
        "(cmake --build external/psxport/build --target discdump)"
    )


def extract(discdump, disc, disc_path, dest_dir):
    """Pull one file off the disc into dest_dir, if not already present. Never swallows discdump's
    diagnostic — a missing executable is a build-breaker, not a warning to bury."""
    out = os.path.join(ROOT, dest_dir, os.path.basename(disc_path))
    if os.path.isfile(out):
        return out
    os.makedirs(os.path.join(ROOT, dest_dir), exist_ok=True)
    r = subprocess.run(
        [discdump, "get", disc_path, disc, os.path.join(ROOT, dest_dir)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    if r.returncode != 0 or not os.path.isfile(out):
        err = (r.stderr or b"").decode(errors="replace").strip()
        tree = subprocess.run(
            [discdump, "list", disc],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        die(
            f"could not extract {disc_path} from the disc"
            + (f"\n  discdump: {err}" if err else "")
            + "\n  disc tree:\n"
            + (tree.stdout or b"").decode(errors="replace")
        )
    return out


def verify_selected_media(discdump: str, disc: str, selected: Title) -> None:
    """Read SYSTEM.CNF from this disc every run; a cached executable cannot select a title."""
    scratch = Path(ROOT) / "scratch"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="title-inspect-", dir=scratch) as directory:
        result = subprocess.run(
            [discdump, "get", SYSTEM_CNF, disc, directory],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
        )
        cnf = Path(directory) / SYSTEM_CNF
        if result.returncode or not cnf.is_file():
            diagnostic = result.stderr.decode(errors="replace").strip()
            die(f"could not inspect SYSTEM.CNF on selected media: {diagnostic}")
        try:
            actual = load_catalog(Path(ROOT)).from_system_cnf(
                cnf.read_text(encoding="ascii", errors="replace")
            )
        except TitleCatalogError as exc:
            die(str(exc))
        if actual.id != selected.id:
            die(
                f"selected title {selected.id} ({selected.serial}) refuses media booting "
                f"{actual.id} ({actual.serial})"
            )
        say(f"identity: SYSTEM.CNF boots {actual.serial} ({actual.label})")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def provision_executable(discdump: str, disc: str, title: Title) -> Path:
    """Freshly extract and authenticate the boot executable before trusting the scratch cache."""
    scratch = Path(ROOT) / "scratch"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="exe-identity-", dir=scratch) as directory:
        result = subprocess.run(
            [discdump, "get", title.serial, disc, directory],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
        )
        extracted = Path(directory) / title.serial
        if result.returncode or not extracted.is_file():
            diagnostic = result.stderr.decode(errors="replace").strip()
            die(f"could not extract {title.serial} from selected media: {diagnostic}")
        actual_size = extracted.stat().st_size
        if actual_size != title.file_size:
            die(
                f"{title.serial} size mismatch: expected {title.file_size} bytes, "
                f"selected media contains {actual_size} bytes"
            )
        actual_hash = file_sha256(extracted)
        if actual_hash != title.executable_sha256:
            die(
                f"{title.serial} SHA-256 mismatch: expected {title.executable_sha256}, "
                f"selected media contains {actual_hash}"
            )

        destination = Path(ROOT) / title.guest_executable
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file() or file_sha256(destination) != actual_hash:
            shutil.copyfile(extracted, destination)
        say(f"executable: {title.serial} SHA-256 {actual_hash} (USA identity verified)")
        return destination


def input_hash(title: Title, exe: Path, overlay_dir: Path | None):
    """SHA-256 over the game executable + this game's seed file + the recompiler sources."""
    h = hashlib.sha256()

    def feed(label, path):
        h.update(label.encode())
        with open(path, "rb") as f:
            h.update(f.read())

    feed(title.serial, exe)
    feed(str(title.seeds), Path(ROOT) / title.seeds)
    feed(
        f"titles/{title.id}/title.json", Path(ROOT) / "titles" / title.id / "title.json"
    )
    for src in RECOMP_SRCS:
        feed(f"framework/recomp/{Path(src).name}", Path(ROOT) / src)
    if title.runtime_modules:
        for src in MODULE_SRCS:
            feed(f"game/{src}", Path(ROOT) / src)
    # The relocated module images are recomp INPUTS exactly like the executable: change a load base
    # or a module's disc contents and the emitted substrate must change with it.
    ov = overlay_dir
    for name in sorted(os.listdir(ov)) if ov and os.path.isdir(ov) else []:
        if name.endswith(".bin"):
            feed(f"overlays/{title.id}/{name}", Path(ov) / name)
    return h.hexdigest()


def generated_complete(gen_dir: Path, gen_main: Path):
    """The generated set is complete iff the manifest exists and every TU it lists is present."""
    manifest = gen_dir / "rec_sources.cmake"
    for f in (
        manifest,
        gen_dir / "shard_disp.c",
        gen_main,
        gen_dir / "overlay_table.c",
    ):
        if not os.path.isfile(f):
            return False
    contents = manifest.read_text(encoding="utf-8")
    listed = re.findall(r"^\s*(\S+\.c)\s*$", contents, re.MULTILINE)
    return all(os.path.isfile(gen_dir / tu) for tu in listed)


def run_emit(title: Title, exe: Path, gen_main: Path, overlay_dir: Path | None):
    say(f"recompiling {title.serial} -> C (the execution substrate)…")
    cmd = [
        sys.executable,
        os.path.join(ROOT, f"{RECOMP_DIR}/emit.py"),
        str(exe),
        str(gen_main),
        "--seeds",
        str(Path(ROOT) / title.seeds),
    ]
    if overlay_dir:
        cmd.extend(["--overlays", str(overlay_dir)])
    if subprocess.run(cmd, check=False).returncode != 0:
        die("emit.py failed")


def main():
    args = parse_args(sys.argv[1:])
    try:
        title = load_catalog(Path(ROOT)).by_id(args.title)
    except TitleCatalogError as exc:
        die(str(exc))
    disc = resolve_disc(title, args.disc)
    say(f"disc: {disc}")
    discdump = find_discdump()
    verify_selected_media(discdump, disc, title)

    exe = provision_executable(discdump, disc, title)

    # Runtime-loaded modules: extract + relocate BEFORE the hash is taken, since the relocated images
    # are recomp inputs. Both archive files are large-ish, so extract() caches them in scratch/.
    overlay_dir = None
    if title.runtime_modules:
        wad_dir = Path(ROOT) / "scratch" / "wad" / title.id
        overlay_dir = Path(ROOT) / "scratch" / "overlays" / title.id
        hed = extract(discdump, disc, HED_NAME, str(wad_dir.relative_to(ROOT)))
        wad = extract(discdump, disc, WAD_NAME, str(wad_dir.relative_to(ROOT)))
        say("extracting runtime-loaded code modules from CD.WAD…")
        try:
            extract_modules.extract(hed, wad, str(overlay_dir))
        except (KeyError, ValueError, OSError) as exc:
            die(f"could not prepare the runtime-loaded modules: {exc}")

    gen_dir = Path(ROOT) / title.generated_directory
    gen_main = gen_dir / title.generated_main
    hash_file = gen_dir / ".recomp.hash"
    version_file = gen_dir / ".recomp_version"
    gen_dir.mkdir(parents=True, exist_ok=True)
    want = recomp_version() + ":" + input_hash(title, exe, overlay_dir)
    have = ""
    if hash_file.is_file():
        have = hash_file.read_text(encoding="ascii").strip()

    if (
        (not os.environ.get("PSXPORT_FORCE_RECOMP"))
        and have == want
        and generated_complete(gen_dir, gen_main)
    ):
        say("recomp up to date")
        return

    run_emit(title, exe, gen_main, overlay_dir)
    if not generated_complete(gen_dir, gen_main):
        die("emit.py finished but the generated set is incomplete")
    hash_file.write_text(want + "\n", encoding="ascii")
    version_file.write_text(recomp_version() + "\n", encoding="ascii")
    say("recomp complete")


if __name__ == "__main__":
    main()
