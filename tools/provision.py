"""Authenticate and extract the selected title executable without emitting host code."""

from __future__ import annotations

import hashlib
import shutil
import subprocess
import tempfile
from pathlib import Path

from title_catalog import Title


class ProvisionError(RuntimeError):
    """Selected media cannot produce the authenticated runtime image."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _extract(discdump: Path, disc: Path, serial: str, destination: Path) -> None:
    result = subprocess.run(
        [str(discdump), "get", serial, str(disc), str(destination)],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode or not (destination / serial).is_file():
        detail = result.stderr.strip() or "discdump produced no executable"
        raise ProvisionError(f"could not extract {serial} from selected media: {detail}")


def _verify(path: Path, title: Title) -> None:
    size = path.stat().st_size
    if size != title.file_size:
        raise ProvisionError(
            f"{title.serial} size mismatch: expected {title.file_size} bytes, got {size}"
        )
    digest = _sha256(path)
    if digest != title.executable_sha256:
        raise ProvisionError(
            f"{title.serial} SHA-256 mismatch: expected {title.executable_sha256}, got {digest}"
        )


def provision_executable(root: Path, discdump: Path, disc: Path, title: Title) -> Path:
    """Publish one verified executable atomically; never derive executable host code."""
    scratch = root / "scratch" / "provision"
    scratch.mkdir(parents=True, exist_ok=True)
    output = root / title.guest_executable
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{title.id}-", dir=scratch) as temporary:
        staging = Path(temporary)
        _extract(discdump, disc, title.serial, staging)
        extracted = staging / title.serial
        _verify(extracted, title)
        publish = output.with_suffix(output.suffix + ".new")
        shutil.copyfile(extracted, publish)
        publish.replace(output)
    return output
