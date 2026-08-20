"""One authoritative disc-path selection policy for Spider-Man tooling."""

from __future__ import annotations

import re
from collections.abc import Mapping
from pathlib import Path


def _env_value(path: Path, key: str) -> str:
    if not path.is_file():
        return ""
    pattern = re.compile(rf"^\s*{re.escape(key)}\s*=\s*(.+?)\s*$")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    return ""


def resolve_disc(root: Path, argument: str | None, environ: Mapping[str, str]) -> str:
    """Return CLI > game env > .env game/generic > sorted CHD drop-in, or empty."""
    disc = argument or environ.get("PSXPORT_SPIDERMAN_DISC", "")
    if not disc:
        disc = _env_value(root / ".env", "PSXPORT_SPIDERMAN_DISC")
    if not disc:
        disc = _env_value(root / ".env", "PSXPORT_DISC")
    if disc:
        return disc

    candidates = sorted(path for path in root.iterdir() if path.suffix.lower() == ".chd")
    return str(candidates[0]) if candidates else ""
