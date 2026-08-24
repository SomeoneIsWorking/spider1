"""Locked Python entry point for the Spider native-port launcher."""

from __future__ import annotations

import importlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
main = importlib.import_module("run").main


if __name__ == "__main__":
    raise SystemExit(main())
