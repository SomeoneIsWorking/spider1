"""One authoritative disc-path selection policy for Spider-Man tooling."""

from __future__ import annotations

import re
from collections.abc import Mapping
from pathlib import Path


class DiscPathError(ValueError):
    """Disc selection is ambiguous before the media identifies its title."""


def _env_value(path: Path, key: str) -> str:
    if not path.is_file():
        return ""
    pattern = re.compile(rf"^\s*{re.escape(key)}\s*=\s*(.+?)\s*$")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    return ""


def resolve_disc(
    root: Path,
    argument: str | None,
    environ: Mapping[str, str],
    game_env_key: str = "PSXPORT_SPIDERMAN_DISC",
) -> str:
    """Return CLI > game env > .env game/generic > sorted CHD drop-in, or empty."""
    disc = argument or environ.get(game_env_key, "")
    if not disc:
        disc = _env_value(root / ".env", game_env_key)
    if not disc:
        disc = _env_value(root / ".env", "PSXPORT_DISC")
    if disc:
        return disc

    candidates = sorted(
        path for path in root.iterdir() if path.suffix.lower() == ".chd"
    )
    return str(candidates[0]) if candidates else ""


def _one_title_disc(values: list[tuple[str, str]]) -> str:
    selected = [(key, value) for key, value in values if value]
    paths = {value for _, value in selected}
    if len(paths) > 1:
        keys = ", ".join(key for key, _ in selected)
        raise DiscPathError(
            f"multiple title-specific disc paths are set ({keys}); pass one disc path "
            "to ./run.sh"
        )
    return next(iter(paths), "")


def resolve_unidentified_disc(
    root: Path,
    argument: str | None,
    environ: Mapping[str, str],
    title_env_keys: list[str],
) -> str:
    """Resolve media before SYSTEM.CNF has selected a title."""
    if argument:
        return argument

    disc = _one_title_disc([(key, environ.get(key, "")) for key in title_env_keys])
    if disc:
        return disc

    env_file = root / ".env"
    disc = _one_title_disc([(key, _env_value(env_file, key)) for key in title_env_keys])
    if disc:
        return disc

    disc = environ.get("PSXPORT_DISC", "") or _env_value(env_file, "PSXPORT_DISC")
    if disc:
        return disc

    candidates = sorted(
        path for path in root.iterdir() if path.suffix.lower() == ".chd"
    )
    return str(candidates[0]) if candidates else ""
