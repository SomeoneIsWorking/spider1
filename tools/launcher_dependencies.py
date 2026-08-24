"""Native dependency preflight for the shipping launcher.

This module owns package-manager policy separately from launcher orchestration so
the refusal paths can be tested without inspecting or changing the host.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class NativeTools:
    cmake: str
    git: str
    pkg_config: str
    glslc: str
    cc: str
    cxx: str


@dataclass(frozen=True)
class Requirement:
    key: str
    label: str


TOOL_REQUIREMENTS = (
    Requirement("cmake", "CMake"),
    Requirement("git", "Git"),
    Requirement("pkg_config", "pkg-config"),
    Requirement("glslc", "glslc (Vulkan shader compiler)"),
    Requirement("cc", "a C compiler"),
    Requirement("cxx", "a C++ compiler"),
)

PKG_CONFIG_REQUIREMENTS = (
    Requirement("sdl3", "SDL3 development files"),
    Requirement("sdl3-image", "SDL3_image development files"),
    Requirement("freetype2", "FreeType development files"),
    Requirement("zlib", "zlib development files"),
    Requirement("libzstd", "zstd development files"),
    Requirement("openssl", "OpenSSL development files"),
)

PACKAGE_MAP = {
    "fedora": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkgconf-pkg-config",
        "glslc": "glslc",
        "cc": "gcc",
        "cxx": "gcc-c++",
        "sdl3": "SDL3-devel",
        "sdl3-image": "SDL3_image-devel",
        "freetype2": "freetype-devel",
        "zlib": "zlib-devel",
        "libzstd": "libzstd-devel",
        "openssl": "openssl-devel",
    },
    "debian": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkg-config",
        "glslc": "glslc",
        "cc": "build-essential",
        "cxx": "build-essential",
        "sdl3": "libsdl3-dev",
        "sdl3-image": "libsdl3-image-dev",
        "freetype2": "libfreetype-dev",
        "zlib": "zlib1g-dev",
        "libzstd": "libzstd-dev",
        "openssl": "libssl-dev",
    },
    "macos": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkg-config",
        "glslc": "shaderc",
        "sdl3": "sdl3",
        "sdl3-image": "sdl3_image",
        "freetype2": "freetype",
        "zlib": "zlib",
        "libzstd": "zstd",
        "openssl": "openssl",
    },
}


class DependencyError(RuntimeError):
    """Native prerequisites are missing and must be installed by the user."""


def _read_os_release(path: Path = Path("/etc/os-release")) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def platform_family(platform_name: str, os_release: str) -> str:
    if platform_name == "darwin":
        return "macos"
    if platform_name.startswith("win"):
        return "windows"
    if platform_name.startswith("linux"):
        values: dict[str, str] = {}
        for line in os_release.splitlines():
            key, separator, value = line.partition("=")
            if separator:
                values[key] = value.strip().strip('"').lower()
        identities = {values.get("ID", ""), *values.get("ID_LIKE", "").split()}
        if identities & {"fedora", "rhel", "centos"}:
            return "fedora"
        if identities & {"debian", "ubuntu"}:
            return "debian"
    return "unknown"


def _unique(values: list[str]) -> list[str]:
    return list(dict.fromkeys(value for value in values if value))


def install_instructions(missing: list[Requirement], family: str) -> str:
    labels = ", ".join(requirement.label for requirement in missing)
    keys = [requirement.key for requirement in missing]
    lines = [f"missing native dependencies: {labels}"]

    if family in {"fedora", "debian", "macos"}:
        packages = _unique([PACKAGE_MAP[family].get(key, "") for key in keys])
        compiler_missing = bool({"cc", "cxx"} & set(keys))
        if family == "fedora" and packages:
            lines.extend(
                [
                    "Install them yourself, then rerun:",
                    f"  sudo dnf install {' '.join(packages)}",
                ]
            )
        elif family == "debian" and packages:
            lines.extend(
                [
                    "Install them yourself, then rerun:",
                    f"  sudo apt install {' '.join(packages)}",
                ]
            )
        elif family == "macos":
            lines.append("Install them yourself, then rerun:")
            if packages:
                lines.append(f"  brew install {' '.join(packages)}")
            if compiler_missing:
                lines.append("  xcode-select --install")
        return "\n".join(lines)

    if family == "windows":
        lines.extend(
            [
                "Install the supported Windows toolchain yourself, then rerun from Git Bash:",
                "  winget install Kitware.CMake Git.Git KhronosGroup.VulkanSDK",
                '  winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools"',
                "  vcpkg install sdl3 sdl3-image freetype zlib zstd openssl",
            ]
        )
        return "\n".join(lines)

    lines.append(
        "This Linux distribution is not mapped; install the named development packages with its "
        "native package manager, or report the distribution/version so an exact command can be added."
    )
    return "\n".join(lines)


def _pkg_exists(pkg_config: str, module: str) -> bool:
    return (
        subprocess.run(
            [pkg_config, "--exists", module],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        ).returncode
        == 0
    )


def require_native_dependencies(
    environ: Mapping[str, str] = os.environ,
    *,
    which: Callable[[str], str | None] = shutil.which,
    pkg_exists: Callable[[str, str], bool] = _pkg_exists,
    platform_name: str = sys.platform,
    os_release: str | None = None,
) -> NativeTools:
    names = {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkg-config",
        "glslc": "glslc",
        "cc": environ.get("CC") or "cc",
        "cxx": environ.get("CXX") or "c++",
    }
    resolved = {key: which(name) for key, name in names.items()}
    missing = [
        requirement
        for requirement in TOOL_REQUIREMENTS
        if resolved[requirement.key] is None
    ]

    pkg_config = resolved["pkg_config"]
    if pkg_config is not None:
        missing.extend(
            requirement
            for requirement in PKG_CONFIG_REQUIREMENTS
            if not pkg_exists(pkg_config, requirement.key)
        )

    if missing:
        family = platform_family(
            platform_name, _read_os_release() if os_release is None else os_release
        )
        raise DependencyError(install_instructions(missing, family))

    return NativeTools(
        cmake=resolved["cmake"] or "",
        git=resolved["git"] or "",
        pkg_config=pkg_config or "",
        glslc=resolved["glslc"] or "",
        cc=resolved["cc"] or "",
        cxx=resolved["cxx"] or "",
    )
