"""Keep hosted psxport checkout tied to the same pin as local builds."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parents[1] / ".github/workflows/ci.yml"
CLONE_COMMAND = "uv run --frozen python tools/psxport_sync.py --clone"


def validate_psxport_checkout(workflow: str) -> None:
    steps = re.split(r"(?m)^      - name: ", workflow)
    if len(steps) < 2:
        raise ValueError("CI workflow has no steps")
    if re.search(r"(?im)^\s*repository:\s*[^\s#]*psxport(?:\.git)?(?:\s*#.*)?$", workflow):
        raise ValueError("CI has a hardcoded psxport checkout; use psxport.pin")

    def step_index(marker: str) -> int:
        matches = [index for index, step in enumerate(steps) if marker in step]
        if len(matches) != 1:
            raise ValueError(f"CI needs exactly one step containing {marker!r}")
        return matches[0]

    environment = step_index("uv sync --frozen")
    clone = step_index(CLONE_COMMAND)
    restore = step_index("git -C external/psxport submodule update --init")
    verify = step_index("uv run --frozen python tools/verify.py")
    if not environment < clone < restore < verify:
        raise ValueError(
            "CI must create the locked environment, clone the pin, restore deps, then verify"
        )

    restore_step = steps[restore]
    for dependency in (
        "external/psycross",
        "vendor/beetle-psx",
        "vendor/lucent",
        "deps/libchdr",
    ):
        if dependency not in restore_step:
            raise ValueError(f"CI does not restore {dependency}")


class CiWorkflowTest(unittest.TestCase):
    def test_current_workflow_uses_recorded_pin(self) -> None:
        validate_psxport_checkout(WORKFLOW.read_text(encoding="utf-8"))

    def test_old_hardcoded_checkout_is_rejected(self) -> None:
        old_checkout = """      - name: Check out stale psxport
        uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1
        with:
          repository: SomeoneIsWorking/psxport
          ref: 9e104d9fe7d04043d98fe451732596d68e45022c
"""
        workflow = WORKFLOW.read_text(encoding="utf-8").replace(
            "      - name: Clone psxport at the recorded pin\n",
            old_checkout + "      - name: Clone psxport at the recorded pin\n",
        )
        with self.assertRaisesRegex(ValueError, "hardcoded psxport checkout"):
            validate_psxport_checkout(workflow)


if __name__ == "__main__":
    unittest.main()
