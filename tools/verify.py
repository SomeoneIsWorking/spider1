#!/usr/bin/env python3
"""Run Spider's exact-pinned PSXPort consumer verification."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PSXPORT = (ROOT / "external" / "psxport").resolve()
sys.path.insert(0, str(PSXPORT / "tools"))

from port.consumer_verify import (
    ConsumerVerifyConfig,
    run_consumer_verification,
)


def main() -> int:
    """Configure, build, test, and inspect the active Spider product boundary."""
    build = ROOT / "build" / "consumer-verify"
    return run_consumer_verification(
        ConsumerVerifyConfig(
            name="Spider-Man PSX native/Lightrec products",
            root=ROOT,
            build=build,
            psxport=PSXPORT,
            product=build / "bin" / "spiderman_port",
            cmake_module=ROOT / "CMakeLists.txt",
            test_regex=".",
            cmake_definitions=(
                "-DBUILD_TESTING=ON",
                "-DSPIDER_TITLES=spiderman1;spiderman2",
            ),
            python=Path(sys.executable),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
