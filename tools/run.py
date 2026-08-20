#!/usr/bin/env python3
"""Build and launch the default Spider-Man native PC port target."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Mapping, Sequence
from pathlib import Path

from disc_path import resolve_disc

ROOT = Path(__file__).resolve().parents[1]
EXE = Path("scratch/bin/spiderman/SLUS_008.75")
PORT = Path("scratch/bin/spiderman_port")
CYAN = "\033[1;36m"
RED = "\033[1;31m"
RESET = "\033[0m"


class LauncherError(RuntimeError):
    """A user-actionable launcher refusal."""


def say(message: str) -> None:
    print(f"{CYAN}[run]{RESET} {message}", flush=True)


def refuse(message: str) -> int:
    print(f"{RED}[run] error:{RESET} {message}", file=sys.stderr, flush=True)
    return 1


def run(command: Sequence[str], *, env: Mapping[str, str] | None = None, quiet: bool = False) -> None:
    result = subprocess.run(
        list(command),
        cwd=ROOT,
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
    env: Mapping[str, str] | None = None,
    quiet: bool = False,
) -> None:
    try:
        run(command, env=env, quiet=quiet)
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


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise LauncherError(f"{name} not found")
    return path


def compiler_is_clang(compiler: str) -> bool:
    try:
        result = subprocess.run(
            [compiler, "--version"], cwd=ROOT, text=True, capture_output=True, check=False
        )
    except OSError:
        return False
    return result.returncode == 0 and "clang" in (result.stdout + result.stderr).lower()


def cpu_count() -> int:
    return os.cpu_count() or 4


def framework_revision(framework: Path) -> tuple[str, bool]:
    revision = command_output(["git", "-C", str(framework), "rev-parse", "--short", "HEAD"])
    status = command_output(["git", "-C", str(framework), "status", "--porcelain"])
    return revision or "?", bool(status)


def announce_framework(framework_text: str, framework: Path) -> None:
    revision, dirty = framework_revision(framework)
    suffix = " +dirty" if dirty else ""
    if framework_text == "external/psxport":
        say(f"framework: external/psxport -> {framework.resolve()} @ {revision}{suffix}")
        return
    say(
        f"framework: *** {framework_text} *** (DEV CLONE {revision}{suffix}) — NOT the recorded pin"
    )


def sync_submodules() -> None:
    script = ROOT / "external/psxport/scripts/sync-submodules.sh"
    if shutil.which("git") and script.is_file():
        run_or_refuse(["bash", str(script)], "submodule sync failed")
        return
    say("WARNING: external/psxport/scripts/sync-submodules.sh is absent — the framework's nested ")
    say("         submodules (vendor/beetle-psx, vendor/lucent) were NOT synced.")


def discdump_commands(framework: Path, cc: str, cxx: str, jobs: int) -> list[list[str]]:
    build = framework / "build"
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
        ],
        ["cmake", "--build", str(build), "-j", str(jobs), "--target", "discdump"],
    ]


def port_commands(framework: Path, cc: str, cxx: str, jobs: int) -> list[list[str]]:
    return [
        [
            "cmake",
            "-S",
            ".",
            "-B",
            "build",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPSXPORT_DIR={framework.resolve()}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
        ],
        ["cmake", "--build", "build", "-j", str(jobs), "--target", "spiderman_port"],
    ]


def launch_environment(
    environ: Mapping[str, str], framework_text: str, disc: str
) -> dict[str, str]:
    result = dict(environ)
    if result.get("PSXPORT_NOWINDOW", ""):
        result["PSXPORT_VK_HEADLESS"] = "1"
    else:
        result["PSXPORT_VK_WINDOW"] = "1"
    if not result.get("PSXPORT_ASSET_DIR"):
        result["PSXPORT_ASSET_DIR"] = framework_text
    result["PSXPORT_SPIDERMAN_DISC"] = disc
    return result


def selftest() -> int:
    checks = 0
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
                return refuse(f"launcher selftest failed: disc precedence case {checks}")

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
            return refuse("launcher selftest failed: missing-disc refusal discriminator")

    fake_framework = Path("external/psxport")
    commands = port_commands(fake_framework, "clang", "clang++", 7)
    checks += 1
    if commands[1][-2:] != ["--target", "spiderman_port"]:
        return refuse("launcher selftest failed: default build target")
    checks += 1
    if "-DCMAKE_CXX_COMPILER=clang++" not in commands[0]:
        return refuse("launcher selftest failed: Clang C++ configure")

    headless = launch_environment({"PSXPORT_NOWINDOW": "1"}, "external/psxport", "game.chd")
    windowed = launch_environment(
        {"PSXPORT_ASSET_DIR": ""}, "external/psxport", "game.chd"
    )
    checks += 1
    if headless.get("PSXPORT_VK_HEADLESS") != "1" or "PSXPORT_VK_WINDOW" in headless:
        return refuse("launcher selftest failed: headless launch environment")
    checks += 1
    if windowed.get("PSXPORT_VK_WINDOW") != "1" or windowed["PSXPORT_ASSET_DIR"] != "external/psxport":
        return refuse("launcher selftest failed: windowed launch environment")

    print(f"launcher selftest: PASS — {checks} positive/refusal checks")
    return 0


def launch(argv: Sequence[str]) -> int:
    os.chdir(ROOT)
    require_tool("cmake")
    python = require_tool("python3")
    require_tool("pkg-config")
    result = subprocess.run(["pkg-config", "--exists", "sdl3"], cwd=ROOT, check=False)
    if result.returncode:
        raise LauncherError(
            "SDL3 not found (Linux: SDL3-devel / libsdl3-dev; macOS: brew install sdl3)"
        )

    cc = os.environ.get("CC") or "clang"
    cxx = os.environ.get("CXX") or "clang++"
    if not compiler_is_clang(cc):
        raise LauncherError(f"CC={cc} is not Clang")
    if not compiler_is_clang(cxx):
        raise LauncherError(f"CXX={cxx} is not Clang")
    jobs = cpu_count()

    run_or_refuse([python, "tools/psxport_sync.py", "--auto"], "could not resolve external/psxport")
    framework_text = os.environ.get("PSXPORT_DIR") or "external/psxport"
    framework = Path(framework_text)
    if not (framework / "cmake/psxport.cmake").is_file():
        raise LauncherError(f"PSXPORT_DIR={framework_text} is not a psxport checkout")
    announce_framework(framework_text, framework)
    sync_submodules()

    argument = argv[0] if argv else None
    disc = resolve_disc(ROOT, argument, os.environ)
    if not disc or not Path(disc).is_file():
        raise LauncherError(
            "no disc image — pass it as ./run.sh <disc.chd>, set PSXPORT_SPIDERMAN_DISC, "
            "or drop a *.chd here"
        )
    say(f"disc: {disc}")

    say("building libchdr + discdump…")
    configure_discdump, build_discdump = discdump_commands(framework, cc, cxx, jobs)
    run_or_refuse(configure_discdump, "psxport cmake configure failed", quiet=True)
    run_or_refuse(build_discdump, "discdump build failed", quiet=True)
    discdump = framework / "build/tools/discdump"
    if not os.access(discdump, os.X_OK):
        discdump = framework / "build/tools/discdump.exe"
    if not os.access(discdump, os.X_OK):
        raise LauncherError("discdump build failed")

    Path("generated").mkdir(parents=True, exist_ok=True)
    Path("scratch/bin").mkdir(parents=True, exist_ok=True)
    provision_env = dict(os.environ)
    provision_env["PSXPORT_DISCDUMP"] = str(discdump)
    run_or_refuse(
        [python, "tools/ensure_recomp.py", disc],
        "recomp provisioning failed",
        env=provision_env,
    )
    if not EXE.is_file():
        raise LauncherError(f"ensure_recomp.py did not produce {EXE}")

    say(f"building the native port (CMake -j{jobs})…")
    configure_port, build_port = port_commands(framework, cc, cxx, jobs)
    run_or_refuse(configure_port, "cmake configure failed", quiet=True)
    run_or_refuse(build_port, "port build failed")

    say("launching Spider-Man (native PC port)…")
    launch_env = launch_environment(os.environ, framework_text, disc)
    os.execvpe(str(PORT), [str(PORT), str(EXE)], launch_env)
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
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
