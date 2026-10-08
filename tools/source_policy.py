#!/usr/bin/env python3
"""Resolve the shared scanner and apply Spider's source-boundary manifest."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = Path(__file__).with_name("source_policy.json")
SHARED_CANDIDATES = (
    ROOT / "external" / "re-harness" / "tools" / "source_boundary.py",
    ROOT.parents[1] / "shared" / "re-harness" / "tools" / "source_boundary.py",
    Path.home() / ".codex" / "bin" / "source_boundary.py",
    Path.home() / ".claude" / "bin" / "source_boundary.py",
)


def load_shared_scanner():
    for candidate in SHARED_CANDIDATES:
        if not candidate.is_file():
            continue
        spec = importlib.util.spec_from_file_location("re_harness_source_boundary", candidate)
        if spec is None or spec.loader is None:
            continue
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    attempted = "\n".join(f"  - {path}" for path in SHARED_CANDIDATES)
    raise SystemExit(f"source_policy: shared scanner not found; attempted:\n{attempted}")


if __name__ == "__main__":
    raise SystemExit(load_shared_scanner().main(ROOT, MANIFEST))
