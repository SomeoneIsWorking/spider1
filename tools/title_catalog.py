#!/usr/bin/env python3
"""Serial-bound title identity shared by provisioning and the launcher."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path


class TitleCatalogError(ValueError):
    """The selected media or tracked title catalogue has no unambiguous identity."""


@dataclass(frozen=True)
class Title:
    id: str
    label: str
    serial: str
    disc_env: str
    target: str
    guest_executable: Path
    runtime_modules: bool
    file_size: int
    executable_sha256: str

    @classmethod
    def from_manifest(cls, path: Path) -> Title:
        data = json.loads(path.read_text(encoding="utf-8"))
        required = {
            "id",
            "label",
            "serial",
            "discEnv",
            "target",
            "guestExecutable",
            "runtimeModules",
            "fileSize",
            "executableSha256",
        }
        missing = sorted(required - data.keys())
        if missing:
            raise TitleCatalogError(f"{path}: missing field(s): {', '.join(missing)}")
        serial = str(data["serial"]).upper()
        if not re.fullmatch(r"[A-Z]{4}_[0-9]{3}\.[0-9]{2}", serial):
            raise TitleCatalogError(f"{path}: malformed PlayStation serial {serial!r}")
        executable_sha256 = str(data["executableSha256"]).lower()
        if not re.fullmatch(r"[0-9a-f]{64}", executable_sha256):
            raise TitleCatalogError(
                f"{path}: malformed executable SHA-256 {executable_sha256!r}"
            )
        file_size = data["fileSize"]
        if not isinstance(file_size, int) or isinstance(file_size, bool) or file_size < 0x800:
            raise TitleCatalogError(f"{path}: invalid executable file size {file_size!r}")
        runtime_modules = data["runtimeModules"]
        if not isinstance(runtime_modules, bool):
            raise TitleCatalogError(
                f"{path}: runtimeModules must be true or false, got {runtime_modules!r}"
            )
        return cls(
            id=str(data["id"]),
            label=str(data["label"]),
            serial=serial,
            disc_env=str(data["discEnv"]),
            target=str(data["target"]),
            guest_executable=Path(data["guestExecutable"]),
            runtime_modules=runtime_modules,
            file_size=file_size,
            executable_sha256=executable_sha256,
        )


class TitleCatalog:
    def __init__(self, titles: list[Title]):
        if not titles:
            raise TitleCatalogError("title catalogue is empty")
        self._by_id: dict[str, Title] = {}
        self._by_serial: dict[str, Title] = {}
        for title in titles:
            if title.id in self._by_id:
                raise TitleCatalogError(f"duplicate title id {title.id!r}")
            if title.serial in self._by_serial:
                raise TitleCatalogError(f"duplicate title serial {title.serial!r}")
            self._by_id[title.id] = title
            self._by_serial[title.serial] = title

    @classmethod
    def load(cls, root: Path) -> TitleCatalog:
        manifests = sorted((root / "titles").glob("*/title.json"))
        return cls([Title.from_manifest(path) for path in manifests])

    def by_id(self, title_id: str) -> Title:
        try:
            return self._by_id[title_id]
        except KeyError as exc:
            known = ", ".join(sorted(self._by_id))
            raise TitleCatalogError(
                f"unknown title {title_id!r}; known title ids: {known}"
            ) from exc

    def ids(self) -> tuple[str, ...]:
        return tuple(sorted(self._by_id))

    def from_system_cnf(self, contents: str) -> Title:
        matches = re.findall(
            r"(?im)^\s*BOOT\s*=\s*cdrom:\\?([^;\s]+)(?:;[0-9]+)?\s*$", contents
        )
        serials = {Path(match.replace("\\", "/")).name.upper() for match in matches}
        if len(serials) != 1:
            rendered = ", ".join(sorted(serials)) if serials else "none"
            raise TitleCatalogError(
                f"SYSTEM.CNF must name exactly one BOOT executable; found {rendered}"
            )
        serial = next(iter(serials))
        try:
            return self._by_serial[serial]
        except KeyError as exc:
            known = ", ".join(sorted(self._by_serial))
            raise TitleCatalogError(
                f"unsupported boot executable {serial!r}; supported serials: {known}"
            ) from exc


def load_catalog(root: Path) -> TitleCatalog:
    return TitleCatalog.load(root)
