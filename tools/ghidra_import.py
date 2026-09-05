#!/usr/bin/env python3
"""Rebuild the disposable headless Ghidra project from a captured RAM image."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


class ImportError(RuntimeError):
    """The requested analysis project cannot be built."""


def rebuild_project(root: Path, analyze_headless: str = "analyzeHeadless") -> None:
    ram = root / "scratch" / "bin" / "spiderman" / "ram.bin"
    project = root / "scratch" / "ghidra"
    if not ram.is_file():
        raise ImportError("run tools/redump_ram.py first")
    if project.exists():
        shutil.rmtree(project)
    project.mkdir(parents=True)
    result = subprocess.run(
        [
            analyze_headless,
            str(project),
            "spider1",
            "-import",
            str(ram),
            "-processor",
            "MIPS:LE:32:default",
            "-loader",
            "BinaryLoader",
            "-loader-baseAddr",
            "0x80000000",
        ],
        cwd=root,
        check=False,
    )
    if result.returncode:
        raise ImportError(f"analyzeHeadless exited {result.returncode}")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    try:
        rebuild_project(root)
    except ImportError as exc:
        print(f"ghidra_import: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
