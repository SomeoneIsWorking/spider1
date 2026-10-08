"""Exercise the shipping executable provisioner on acceptance and refusal paths."""

from __future__ import annotations

import hashlib
import sys
import tempfile
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from provision import ProvisionError, provision_executable
from title_catalog import load_catalog


def write_discdump(path: Path) -> None:
    path.write_text(
        """#!/usr/bin/env python3
import pathlib
import shutil
import sys

_, operation, serial, disc, destination = sys.argv
if operation != "get":
    raise SystemExit(2)
source = pathlib.Path(disc)
if source.name == "missing.bin":
    raise SystemExit(0)
shutil.copyfile(source, pathlib.Path(destination) / serial)
""",
        encoding="utf-8",
    )
    path.chmod(0o755)


def expect_refusal(call, fragment: str) -> None:
    try:
        call()
    except ProvisionError as error:
        assert fragment in str(error), (fragment, str(error))
        return
    raise AssertionError(f"provisioning unexpectedly accepted the {fragment!r} case")


def main() -> int:
    source_bytes = b"PS-X EXE" + bytes(range(256)) * 8
    digest = hashlib.sha256(source_bytes).hexdigest()
    base = load_catalog(ROOT).by_id("spiderman1")
    (ROOT / "scratch").mkdir(exist_ok=True)

    with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as temporary:
        fixture = Path(temporary)
        project = fixture / "project"
        project.mkdir()
        discdump = fixture / "discdump.py"
        write_discdump(discdump)
        disc = fixture / "disc.bin"
        disc.write_bytes(source_bytes)
        title = replace(
            base,
            guest_executable=Path("scratch/assets/title/SLUS_008.75"),
            file_size=len(source_bytes),
            executable_sha256=digest,
        )

        output = provision_executable(project, discdump, disc, title)
        assert output.read_bytes() == source_bytes
        checks = 1

        previous = b"previous authenticated executable"
        output.write_bytes(previous)
        wrong_hash = replace(title, executable_sha256="0" * 64)
        expect_refusal(
            lambda: provision_executable(project, discdump, disc, wrong_hash),
            "SHA-256 mismatch",
        )
        assert output.read_bytes() == previous
        checks += 1

        missing = fixture / "missing.bin"
        missing.touch()
        expect_refusal(
            lambda: provision_executable(project, discdump, missing, title),
            "produced no executable",
        )
        assert output.read_bytes() == previous
        checks += 1

    print(f"provision: PASS — {checks} acceptance/refusal/atomicity checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
