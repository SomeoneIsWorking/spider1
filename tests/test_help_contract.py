"""Prove launcher and native executable help require no game assets or disc discovery."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check_help(command: list[str], label: str) -> None:
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("PSXPORT_")
    }
    result = subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"{label} --help exited {result.returncode}: {result.stderr.strip()}"
        )
    if "Usage:" not in result.stdout or "--help" not in result.stdout:
        raise AssertionError(f"{label} --help did not print its usage contract")
    forbidden = ("no disc image", "identity", "PSXPORT_ASSET_DIR", "could not resolve")
    combined = f"{result.stdout}\n{result.stderr}"
    if any(marker in combined for marker in forbidden):
        raise AssertionError(f"{label} --help performed launch-time discovery: {combined}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    arguments = parser.parse_args()

    checks = (
        ([str(ROOT / "run.sh"), "-h"], "launcher -h"),
        ([str(ROOT / "run.sh"), "--help"], "launcher --help"),
        ([arguments.executable, "-h"], "native executable -h"),
        ([arguments.executable, "--help"], "native executable --help"),
    )
    for command, label in checks:
        check_help(command, label)
    print("help contract: PASS — 4 launcher/executable paths exit 0 before asset discovery")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
