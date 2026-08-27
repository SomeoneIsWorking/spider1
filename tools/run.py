#!/usr/bin/env python3
"""Build and launch the default Spider-Man native PC port target."""

from __future__ import annotations

import hashlib
import os
import runpy
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Mapping, Sequence
from pathlib import Path

from disc_path import DiscPathError, resolve_disc, resolve_unidentified_disc
from launcher_dependencies import (
    DependencyError,
    Requirement,
    install_instructions,
    platform_family,
    require_native_dependencies,
)
from title_catalog import Title, TitleCatalogError, load_catalog

ROOT = Path(__file__).resolve().parents[1]
CYAN = "\033[1;36m"
RED = "\033[1;31m"
RESET = "\033[0m"
USAGE = """Usage: ./run.sh [--prepare-only] [disc.chd]

Build and launch the detected Neversoft Spider title from a user-supplied disc.

Options:
  --prepare-only  Provision and build the selected title without launching it.
  -h, --help      Show this help and exit.
"""


class LauncherError(RuntimeError):
    """A user-actionable launcher refusal."""


def say(message: str) -> None:
    print(f"{CYAN}[run]{RESET} {message}", flush=True)


def refuse(message: str) -> int:
    print(f"{RED}[run] error:{RESET} {message}", file=sys.stderr, flush=True)
    return 1


def run(
    command: Sequence[str],
    *,
    cwd: Path = ROOT,
    env: Mapping[str, str] | None = None,
    quiet: bool = False,
) -> None:
    result = subprocess.run(
        list(command),
        cwd=cwd,
        env=dict(env) if env is not None else None,
        stdout=subprocess.DEVNULL if quiet else None,
        check=False,
    )
    if result.returncode:
        raise LauncherError("")


def run_or_refuse(
    command: Sequence[str],
    failure: str,
    *,
    cwd: Path = ROOT,
    env: Mapping[str, str] | None = None,
    quiet: bool = False,
) -> None:
    try:
        run(command, cwd=cwd, env=env, quiet=quiet)
    except LauncherError as exc:
        raise LauncherError(failure) from exc


def command_output(command: Sequence[str]) -> str | None:
    try:
        result = subprocess.run(
            list(command), cwd=ROOT, text=True, capture_output=True, check=False
        )
    except OSError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def cpu_count() -> int:
    return os.cpu_count() or 4


def framework_revision(framework: Path) -> tuple[str, bool]:
    revision = command_output(
        ["git", "-C", str(framework), "rev-parse", "--short", "HEAD"]
    )
    status = command_output(["git", "-C", str(framework), "status", "--porcelain"])
    return revision or "?", bool(status)


def announce_framework(framework_text: str, framework: Path) -> None:
    revision, dirty = framework_revision(framework)
    suffix = " +dirty" if dirty else ""
    if framework_text == "external/psxport":
        say(
            f"framework: external/psxport -> {framework.resolve()} @ {revision}{suffix}"
        )
        return
    say(
        f"framework: *** {framework_text} *** (DEV CLONE {revision}{suffix}) — NOT the recorded pin"
    )


def submodule_sync_invocation(framework: Path) -> tuple[list[str], Path]:
    """Return the framework-owned sync command and its repository root."""
    framework_root = framework.resolve()
    return ["bash", str(framework_root / "scripts/sync-submodules.sh")], framework_root


def sync_submodules(framework: Path) -> None:
    command, framework_root = submodule_sync_invocation(framework)
    script = Path(command[1])
    if shutil.which("git") and script.is_file():
        run_or_refuse(
            command,
            "submodule sync failed",
            cwd=framework_root,
        )
        return
    say(
        "WARNING: external/psxport/scripts/sync-submodules.sh is absent — the framework's nested "
    )
    say("         submodules (vendor/beetle-psx, vendor/lucent) were NOT synced.")


def player_build_root(cc: str, cxx: str) -> Path:
    """Return an isolated build root without classifying compiler identity."""
    contract = f"{cc}\0{cxx}\0BUILD_TESTING=OFF\0PSXPORT_BUILD_TESTS=OFF"
    key = hashlib.sha256(contract.encode()).hexdigest()[:12]
    return Path("scratch/build/player") / key


def discdump_commands(
    framework: Path,
    build: Path,
    cc: str,
    cxx: str,
    python: str,
    jobs: int,
) -> list[list[str]]:
    return [
        [
            "cmake",
            "-S",
            str(framework),
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DPython3_EXECUTABLE={python}",
            "-DBUILD_TESTING=OFF",
            "-DPSXPORT_BUILD_TESTS=OFF",
        ],
        ["cmake", "--build", str(build), "-j", str(jobs), "--target", "discdump"],
    ]


def port_commands(
    framework: Path,
    build: Path,
    cc: str,
    cxx: str,
    python: str,
    jobs: int,
    title: Title,
) -> list[list[str]]:
    return [
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPSXPORT_DIR={framework.resolve()}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DPython3_EXECUTABLE={python}",
            "-DBUILD_TESTING=OFF",
            f"-DSPIDER_TITLES={title.id}",
        ],
        ["cmake", "--build", str(build), "-j", str(jobs), "--target", title.target],
    ]


def launch_environment(
    environ: Mapping[str, str], framework_text: str, disc: str, title: Title
) -> dict[str, str]:
    framework = Path(framework_text)
    if not framework.is_absolute():
        framework = ROOT / framework
    policy = runpy.run_path(str(framework / "tools/port/launch_environment.py"))
    result = policy["player_environment"](environ)
    if not result.get("PSXPORT_ASSET_DIR"):
        result["PSXPORT_ASSET_DIR"] = framework_text
    result[title.disc_env] = disc
    return result


def detect_title(discdump: Path, disc: str) -> Title:
    scratch = ROOT / "scratch"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="launcher-title-", dir=scratch
    ) as directory:
        result = subprocess.run(
            [str(discdump), "get", "SYSTEM.CNF", disc, directory],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        cnf = Path(directory) / "SYSTEM.CNF"
        if result.returncode or not cnf.is_file():
            raise LauncherError(
                f"could not inspect SYSTEM.CNF: {result.stderr.strip()}"
            )
        try:
            return load_catalog(ROOT).from_system_cnf(
                cnf.read_text(encoding="ascii", errors="replace")
            )
        except TitleCatalogError as exc:
            raise LauncherError(str(exc)) from exc


def bootstrap_contract(root: Path) -> str:
    required = ("bootstrap.py", "pyproject.toml", "uv.lock")
    missing = [name for name in required if not (root / name).is_file()]
    if missing:
        return f"missing locked bootstrap file(s): {', '.join(missing)}"
    try:
        shim = (root / "run.sh").read_text(encoding="utf-8")
    except OSError:
        return "run.sh is missing"
    if "uv run --frozen python bootstrap.py" not in shim or "python3" in shim:
        return "run.sh does not enter through frozen uv"
    return ""


def parse_launch_arguments(argv: Sequence[str]) -> tuple[bool, str | None]:
    arguments = list(argv)
    prepare_only = False
    if "--prepare-only" in arguments:
        prepare_only = True
        arguments.remove("--prepare-only")
    if len(arguments) > 1 or "--prepare-only" in arguments:
        raise LauncherError("usage: ./run.sh [--prepare-only] [disc.chd]")
    if arguments and arguments[0].startswith("-"):
        raise LauncherError(f"unknown launcher option: {arguments[0]}")
    return prepare_only, arguments[0] if arguments else None


def selftest() -> int:
    checks = 0
    checks += 1
    if error := bootstrap_contract(ROOT):
        return refuse(f"launcher selftest failed: {error}")
    checks += 1
    if parse_launch_arguments(["--prepare-only", "game.chd"]) != (
        True,
        "game.chd",
    ):
        return refuse("launcher selftest failed: prepare-only argument parsing")
    checks += 1
    try:
        parse_launch_arguments(["--prepare-only", "--prepare-only"])
    except LauncherError:
        pass
    else:
        return refuse("launcher selftest failed: duplicate prepare-only accepted")

    scratch = ROOT / "scratch"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="run-selftest-", dir=scratch) as temporary:
        root = Path(temporary)
        cli = root / "cli.chd"
        env_disc = root / "env.chd"
        file_disc = root / "file.chd"
        fallback = root / "a.chd"
        for path in (cli, env_disc, file_disc, fallback):
            path.touch()
        for name in ("bootstrap.py", "pyproject.toml", "uv.lock"):
            (root / name).touch()
        (root / "run.sh").write_text(
            '#!/bin/sh\nexec python3 "$(dirname "$0")/tools/run.py" "$@"\n',
            encoding="utf-8",
        )
        checks += 1
        if bootstrap_contract(root) != "run.sh does not enter through frozen uv":
            return refuse("launcher selftest failed: broken bootstrap shim accepted")

        (root / ".env").write_text(
            f"PSXPORT_SPIDERMAN_DISC={file_disc}\nPSXPORT_DISC=ignored.chd\n",
            encoding="utf-8",
        )

        cases = [
            (str(cli), {"PSXPORT_SPIDERMAN_DISC": str(env_disc)}, str(cli)),
            (None, {"PSXPORT_SPIDERMAN_DISC": str(env_disc)}, str(env_disc)),
            (None, {}, str(file_disc)),
        ]
        for argument, environ, expected in cases:
            checks += 1
            if resolve_disc(root, argument, environ) != expected:
                return refuse(
                    f"launcher selftest failed: disc precedence case {checks}"
                )

        (root / ".env").write_text(
            f"PSXPORT_DISC={file_disc}\n",
            encoding="utf-8",
        )
        checks += 1
        if resolve_disc(root, None, {}) != str(file_disc):
            return refuse("launcher selftest failed: generic .env disc fallback")

        (root / ".env").unlink()
        checks += 1
        if resolve_disc(root, None, {}) != str(fallback):
            return refuse("launcher selftest failed: sorted CHD drop-in")
        for path in root.glob("*.chd"):
            path.unlink()
        checks += 1
        if resolve_disc(root, None, {}):
            return refuse(
                "launcher selftest failed: missing-disc refusal discriminator"
            )

        checks += 1
        if resolve_unidentified_disc(
            root,
            None,
            {"PSXPORT_SPIDERMAN2_DISC": str(env_disc)},
            ["PSXPORT_SPIDERMAN_DISC", "PSXPORT_SPIDERMAN2_DISC"],
        ) != str(env_disc):
            return refuse("launcher selftest failed: Enter Electro disc environment")

        checks += 1
        try:
            resolve_unidentified_disc(
                root,
                None,
                {
                    "PSXPORT_SPIDERMAN_DISC": str(env_disc),
                    "PSXPORT_SPIDERMAN2_DISC": str(file_disc),
                },
                ["PSXPORT_SPIDERMAN_DISC", "PSXPORT_SPIDERMAN2_DISC"],
            )
        except DiscPathError:
            pass
        else:
            return refuse(
                "launcher selftest failed: ambiguous title disc environments accepted"
            )

    fake_framework = Path("external/psxport")
    sync_command, sync_root = submodule_sync_invocation(fake_framework)
    checks += 1
    if (
        sync_root != fake_framework.resolve()
        or Path(sync_command[1]).parent.parent != sync_root
    ):
        return refuse("launcher selftest failed: submodule sync is not framework-owned")
    catalog = load_catalog(ROOT)
    spider1 = catalog.by_id("spiderman1")
    spider2 = catalog.by_id("spiderman2")
    commands = port_commands(
        fake_framework,
        Path("scratch/build/player/toolchain/spiderman1"),
        "/usr/bin/cc",
        "/usr/bin/c++",
        "/locked/python",
        7,
        spider1,
    )
    checks += 1
    if commands[1][-2:] != ["--target", "spiderman_port"]:
        return refuse("launcher selftest failed: default build target")
    checks += 1
    if "-DCMAKE_CXX_COMPILER=/usr/bin/c++" not in commands[0]:
        return refuse("launcher selftest failed: C++ compiler propagation")
    checks += 1
    if "-DPython3_EXECUTABLE=/locked/python" not in commands[0]:
        return refuse("launcher selftest failed: locked Python propagation")
    checks += 1
    if commands[0][commands[0].index("-B") + 1] == "build" or not commands[0][
        commands[0].index("-B") + 1
    ].startswith("scratch/build/player/"):
        return refuse("launcher selftest failed: product build is not isolated")
    checks += 1
    if "-DBUILD_TESTING=OFF" not in commands[0] or any(
        "ctest" in argument.lower() for command in commands for argument in command
    ):
        return refuse("launcher selftest failed: product path can run tests")
    checks += 1
    spider2_commands = port_commands(
        fake_framework,
        Path("scratch/build/player/toolchain/spiderman2"),
        "/opt/gcc",
        "/opt/g++",
        "/locked/python",
        7,
        spider2,
    )
    if (
        spider2_commands[1][-1] != "enter_electro_port"
        or "-DSPIDER_TITLES=spiderman2" not in spider2_commands[0]
    ):
        return refuse("launcher selftest failed: Enter Electro target selection")

    checks += 1
    tools_commands = discdump_commands(
        fake_framework,
        Path("scratch/build/player/toolchain/framework"),
        "/opt/gcc",
        "/opt/g++",
        "/locked/python",
        7,
    )
    if (
        tools_commands[0][tools_commands[0].index("-B") + 1]
        != "scratch/build/player/toolchain/framework"
        or "-DBUILD_TESTING=OFF" not in tools_commands[0]
        or "-DPSXPORT_BUILD_TESTS=OFF" not in tools_commands[0]
        or any(
            "ctest" in argument.lower()
            for command in tools_commands
            for argument in command
        )
    ):
        return refuse("launcher selftest failed: disc tool build is not isolated")

    checks += 1
    fedora = install_instructions(
        [
            Requirement("cxx", "a C++ compiler"),
            Requirement("sdl3", "SDL3 development files"),
        ],
        "fedora",
    )
    if "sudo dnf install gcc-c++ SDL3-devel" not in fedora:
        return refuse("launcher selftest failed: Fedora dependency command")
    checks += 1
    if platform_family("linux", 'ID="ubuntu"\nID_LIKE="debian"\n') != "debian":
        return refuse("launcher selftest failed: Linux platform detection")
    checks += 1
    available = {
        "cmake",
        "git",
        "pkg-config",
        "glslc",
        "gcc",
        "g++",
    }
    native = require_native_dependencies(
        {"CC": "gcc", "CXX": "g++"},
        which=lambda name: f"/fake/{name}" if name in available else None,
        pkg_exists=lambda _tool, _module: True,
        platform_name="linux",
        os_release='ID="fedora"\n',
    )
    if native.cc != "/fake/gcc" or native.cxx != "/fake/g++":
        return refuse(
            "launcher selftest failed: compiler selection without identity policy"
        )
    checks += 1
    without_glslc = available - {"glslc"}
    try:
        require_native_dependencies(
            {"CC": "gcc", "CXX": "g++"},
            which=lambda name: f"/fake/{name}" if name in without_glslc else None,
            pkg_exists=lambda _tool, _module: True,
            platform_name="linux",
            os_release='ID="fedora"\n',
        )
    except DependencyError as exc:
        if "sudo dnf install glslc" not in str(exc):
            return refuse("launcher selftest failed: wrong missing-glslc refusal")
    else:
        return refuse("launcher selftest failed: missing glslc accepted")

    player_from_agent_shell = launch_environment(
        {
            "PSXPORT_NOWINDOW": "1",
            "PSXPORT_VK_HEADLESS": "1",
            "PSXPORT_NOAUDIO": "1",
            "PSXPORT_NOPACE": "1",
        },
        "external/psxport",
        "game.chd",
        spider1,
    )
    windowed = launch_environment(
        {"PSXPORT_ASSET_DIR": ""}, "external/psxport", "game.chd", spider2
    )
    checks += 1
    if player_from_agent_shell.get("PSXPORT_VK_WINDOW") != "1" or any(
        key in player_from_agent_shell
        for key in (
            "PSXPORT_NOWINDOW",
            "PSXPORT_VK_HEADLESS",
            "PSXPORT_NOAUDIO",
            "PSXPORT_NOPACE",
        )
    ):
        return refuse("launcher selftest failed: player launch environment")
    checks += 1
    if (
        windowed.get("PSXPORT_VK_WINDOW") != "1"
        or windowed["PSXPORT_ASSET_DIR"] != "external/psxport"
    ):
        return refuse("launcher selftest failed: windowed launch environment")
    checks += 1
    if (
        windowed.get("PSXPORT_SPIDERMAN2_DISC") != "game.chd"
        or "PSXPORT_SPIDERMAN_DISC" in windowed
    ):
        return refuse("launcher selftest failed: title-specific disc environment")

    print(f"launcher selftest: PASS — {checks} positive/refusal checks")
    return 0


def launch(argv: Sequence[str]) -> int:
    os.chdir(ROOT)
    prepare_only, argument = parse_launch_arguments(argv)
    try:
        native = require_native_dependencies()
    except DependencyError as exc:
        raise LauncherError(str(exc)) from exc
    python = sys.executable
    cc = native.cc
    cxx = native.cxx
    jobs = cpu_count()
    build_root = player_build_root(cc, cxx)

    run_or_refuse(
        [python, "tools/psxport_sync.py", "--auto"],
        "could not resolve external/psxport",
    )
    framework_text = os.environ.get("PSXPORT_DIR") or "external/psxport"
    framework = Path(framework_text)
    if not (framework / "cmake/psxport.cmake").is_file():
        raise LauncherError(f"PSXPORT_DIR={framework_text} is not a psxport checkout")
    announce_framework(framework_text, framework)
    sync_submodules(framework)

    catalog = load_catalog(ROOT)
    try:
        disc = resolve_unidentified_disc(
            ROOT,
            argument,
            os.environ,
            [catalog.by_id(title_id).disc_env for title_id in catalog.ids()],
        )
    except DiscPathError as exc:
        raise LauncherError(str(exc)) from exc
    if not disc or not Path(disc).is_file():
        raise LauncherError(
            "no disc image — pass it as ./run.sh <disc.chd>, set a title-specific disc "
            "environment variable, or drop one *.chd here"
        )
    say(f"disc: {disc}")

    say("building libchdr + discdump…")
    framework_build = build_root / "framework"
    configure_discdump, build_discdump = discdump_commands(
        framework, framework_build, cc, cxx, python, jobs
    )
    run_or_refuse(configure_discdump, "psxport cmake configure failed", quiet=True)
    run_or_refuse(build_discdump, "discdump build failed", quiet=True)
    discdump = framework_build / "tools/discdump"
    if not os.access(discdump, os.X_OK):
        discdump = framework_build / "tools/discdump.exe"
    if not os.access(discdump, os.X_OK):
        raise LauncherError("discdump build failed")

    title = detect_title(discdump, disc)
    say(f"title: {title.label} ({title.serial})")

    title.generated_directory.mkdir(parents=True, exist_ok=True)
    Path("scratch/bin").mkdir(parents=True, exist_ok=True)
    provision_env = dict(os.environ)
    provision_env["PSXPORT_DISCDUMP"] = str(discdump)
    run_or_refuse(
        [python, "tools/ensure_recomp.py", "--title", title.id, disc],
        "recomp provisioning failed",
        env=provision_env,
    )
    if not title.guest_executable.is_file():
        raise LauncherError(
            f"ensure_recomp.py did not produce {title.guest_executable}"
        )

    say(f"building the native port (CMake -j{jobs})…")
    port_build = build_root / title.id
    configure_port, build_port = port_commands(
        framework, port_build, cc, cxx, python, jobs, title
    )
    run_or_refuse(configure_port, "cmake configure failed", quiet=True)
    run_or_refuse(build_port, "port build failed")

    if prepare_only:
        say(f"{title.label} is built and ready at {title.port_executable}")
        return 0

    say(f"launching {title.label} (native PC port)…")
    launch_env = launch_environment(os.environ, framework_text, disc, title)
    os.execvpe(
        str(title.port_executable),
        [str(title.port_executable), str(title.guest_executable)],
        launch_env,
    )
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if arguments in (["-h"], ["--help"]):
        print(USAGE, end="")
        return 0
    if arguments == ["--selftest"]:
        return selftest()
    try:
        return launch(arguments)
    except LauncherError as exc:
        return refuse(str(exc))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
