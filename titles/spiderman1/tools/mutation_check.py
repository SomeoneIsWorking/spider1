#!/usr/bin/env python3
"""Mutation-check the SLUS_008.75 guest-widescreen owner: do the tests actually FIRE?

WHY. A green suite says the assertions currently hold. It does not say any assertion can fail, and
on this owner the assertions are the ONLY thing standing between a widening and a subtly wrong
picture: a projection that is a zoom, a depth band shifted by a horizontal margin, a guest window
that grows every frame, a 4:3 leg that is not byte-identical to retail. Each of those is a one-token
edit away. This harness makes the edits and requires the suite to notice.

THE BASELINE IS BUILT FIRST, and that ordering is the point rather than a formality. The Tomba! 1
harness in this workspace found a flaw in ITSELF mid-run: it mutated a source file, rebuilt, and ran
a test binary that had not been relinked, so a mutant it had already "killed" was reported killed by
the previous build. So this one refuses to start unless it has compiled the UNMODIFIED sources
itself, in its own directory, and watched them pass. A mutant is only ever compared against a binary
this script produced.

IT NEVER TOUCHES THE TREE. Each mutant is compiled from copies in scratch/mutation/<name>/, so an
interrupted run cannot leave a broken owner in the repository, and the test object is compiled once
and reused — it is the thing under test's harness, not the thing under test.

A KILL IS A NON-ZERO EXIT, which deliberately includes the owner's own refusals: a mutant that makes
the owner ABORT on a path the tests exercise has been caught, and the report says so rather than
scoring it as a miss.

    uv run --frozen python titles/spiderman1/tools/mutation_check.py
    uv run --frozen python titles/spiderman1/tools/mutation_check.py --list
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT / "build" / "consumer-verify"
OWNER_CPP = ROOT / "titles" / "spiderman1" / "spider1_widescreen.cpp"
OWNER_H = ROOT / "titles" / "spiderman1" / "spider1_widescreen.h"
TEST_CPP = ROOT / "tests" / "spider1_widescreen_test.cpp"
WORK = ROOT / "scratch" / "mutation"
TEST_OBJECT = "CMakeFiles/spider1_widescreen_test.dir/tests/spider1_widescreen_test.cpp.o"
OWNER_OBJECT_SUFFIX = "titles/spiderman1/spider1_widescreen.cpp.o"


@dataclass(frozen=True)
class Mutant:
    name: str
    why: str
    # (file, exact old text, exact new text). `old` must appear exactly once, or the harness
    # REFUSES the mutant: a mutation that silently applied nowhere is a kill rate computed over
    # nothing.
    edit: tuple[str, str, str]

    def apply(self, directory: Path) -> None:
        target = directory / Path(self.edit[0]).name
        text = target.read_text()
        old, new = self.edit[1], self.edit[2]
        if text.count(old) != 1:
            raise SystemExit(
                f"mutation_check: REFUSING mutant {self.name!r}: its anchor appears "
                f"{text.count(old)} time(s) in {target.name}, expected exactly 1"
            )
        target.write_text(text.replace(old, new))


def mutants() -> list[Mutant]:
    cpp = "spider1_widescreen.cpp"
    header = "spider1_widescreen.h"
    return [
        Mutant(
            "margin-off-by-one",
            "A one-pixel error in the margin still centres nothing correctly. The widened window and "
            "the plan's centre would disagree by one, which is the owner's own refusal.",
            (cpp,
             "return wrapField(static_cast<std::uint32_t>(value) + margin);",
             "return wrapField(static_cast<std::uint32_t>(value) + margin + 1);"),
        ),
        Mutant(
            "not-idempotent-window",
            "Adding the margin to the value just READ is the cumulative-growth defect issue 0022 "
            "recorded (512 -> 684 -> 856). The second publication must be a no-op.",
            (cpp,
             "core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, shifted(retail_.horizontalFar));",
             "core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, shifted(far));"),
        ),
        Mutant(
            "substitute-h-instead-of-ofx",
            "The classic wrong widening: scale the focal length instead of moving the projection "
            "centre. That is a ZOOM, and it changes H, which must stay bit-identical.",
            (cpp,
             "  const std::uint16_t publishedSpan =\n",
             "  core.mem_w16(record + Spider1ViewportOffset::kScreenDistance,\n"
             "               static_cast<std::uint16_t>(span * 3 / 4));\n"
             "  const std::uint16_t publishedSpan =\n"),
        ),
        Mutant(
            "shift-the-depth-band",
            "Re-introduce the defect this work removed: move the record's DEPTH window by a "
            "horizontal margin. It is a different axis, compared against GTE IR1/SZ, and the "
            "publication re-asserts it anyway.",
            (cpp,
             "core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, shifted(retail_.horizontalNear));\n"
             "\n  retail(core);",
             "core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, shifted(retail_.horizontalNear));\n"
             "  core.mem_w16(record + Spider1ViewportOffset::kDepthLower,\n"
             "               static_cast<std::uint16_t>(retail_.horizontalFar + margin));\n"
             "\n  retail(core);"),
        ),
        Mutant(
            "four-three-writes-a-byte",
            "4:3 must be the IDENTITY, byte for byte. Any write on that leg is a regression even "
            "when the value it would write looks harmless.",
            (cpp,
             "    retail(core);\n    return;\n  }",
             "    retail(core);\n"
             "    core.mem_w16(record + Spider1ViewportOffset::kCentreX,\n"
             "                 static_cast<std::uint16_t>(centreX + 1));\n"
             "    return;\n  }"),
        ),
        Mutant(
            "draw-clip-widens-by-two",
            "The draw clip takes RECT.w from the plan exactly. A rounding fudge here widens the "
            "canvas without widening the frustum, which is the off-centre defect in a different "
            "guise.",
            (cpp,
             "core.mem_w16(environment + 4, static_cast<std::uint16_t>(latched.guestDrawWidth));",
             "core.mem_w16(environment + 4, static_cast<std::uint16_t>(latched.guestDrawWidth + 2));"),
        ),
        Mutant(
            "draw-clip-writes-the-height",
            "RECT.h is the guest's, not the owner's. Writing the width into the height slot would "
            "leave the width untouched and silently clip nothing.",
            (cpp,
             "core.mem_w16(environment + 4, static_cast<std::uint16_t>(latched.guestDrawWidth));",
             "core.mem_w16(environment + 6, static_cast<std::uint16_t>(latched.guestDrawWidth));"),
        ),
        Mutant(
            "auto-means-sixteen-nine",
            "ASPECT_AUTO resolves against the live sink inside the framework's plan builder. "
            "Folding it to 16:9 here claims a widening on a headless run that has no wide sink.",
            (cpp,
             "  case ASPECT_AUTO:\n    return PresentationAspect::MatchSink;",
             "  case ASPECT_AUTO:\n    return PresentationAspect::Wide16x9;"),
        ),
        Mutant(
            "refuse-kseg0",
            "The first live run refused 0x8009A6E4 as 'not guest memory' because the bound compared "
            "a KSEG0 address against a physical offset. Reverting the bound must be caught.",
            (cpp,
             "  if (address >= kKseg0Base && address - kKseg0Base < kMainRamBytes) {\n    return true;\n  }",
             "  if (address < kMainRamBytes) {\n    return true;\n  }"),
        ),
        Mutant(
            "no-frame-boundary-relatch",
            "The measured defect: a plan latched while the display extent was 320 describes a "
            "428-wide canvas for a 512-wide scene, and the presenter then refuses the widening. "
            "Dropping the re-latch restores exactly that.",
            (cpp,
             "  if (!drawWidthMeasured()) {\n    return;\n  }\n  const std::uint32_t record = core.mem_r32(kViewportRecordCell);",
             "  if (!drawWidthMeasured()) {\n    return;\n  }\n  return;\n  const std::uint32_t record = core.mem_r32(kViewportRecordCell);"),
        ),
        Mutant(
            "frame-boundary-needs-a-published-window",
            "The same re-latch, gated on a window this owner has not widened yet. The host canvas is "
            "derived from the live display extent, not from the guest's window, so requiring the "
            "window leaves the canvas stale for the frames before the first publication.",
            (cpp,
             "  const GuestProjectionPlan latched = relatch(\n"
             "      core,\n"
             "      measuredGeometry(fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar),",
             "  if (!retailCaptured_) {\n"
             "    return;\n"
             "  }\n"
             "  const GuestProjectionPlan latched = relatch(\n"
             "      core,\n"
             "      measuredGeometry(fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar),"),
        ),
        Mutant(
            "frame-boundary-writes-the-wrong-centre",
            "The synchroniser writes the plan's centre, not a centre of its own. A one-off here "
            "would desynchronise the per-vertex CR24 re-assertion from the plan.",
            (cpp,
             "    core.mem_w16(record + Spider1ViewportOffset::kCentreX,\n"
             "                 static_cast<std::uint16_t>(latched.projectionCenterX));",
             "    core.mem_w16(record + Spider1ViewportOffset::kCentreX,\n"
             "                 static_cast<std::uint16_t>(latched.projectionCenterX + 1));"),
        ),
        Mutant(
            "geometry-measures-the-draw-width",
            "The plan's projection extent is the guest's own WINDOW, not its draw clip. Measuring "
            "the wrong one yields a margin for a different projection and the title's derivation "
            "then disagrees with the plan.",
            (cpp,
             "measuredGeometry(span, verticalSpan, measuredDrawWidth_)",
             "measuredGeometry(measuredDrawWidth_, verticalSpan, measuredDrawWidth_)"),
        ),
        Mutant(
            "published-addresses-move",
            "The two override addresses are measured guest facts. Installing the projection "
            "override anywhere else widens nothing at all, and nothing else in the suite would "
            "notice.",
            (header,
             "inline constexpr std::uint32_t kProjectionPublication = 0x80075D0Cu;",
             "inline constexpr std::uint32_t kProjectionPublication = 0x80075D10u;"),
        ),
    ]


def owner_compile_command() -> list[str]:
    """The production compile line for the owner, read from the build tree — not re-typed here."""
    entries = json.loads((BUILD / "compile_commands.json").read_text())
    for entry in entries:
        if entry["file"] == str(OWNER_CPP):
            return shlex.split(entry["command"])
    raise SystemExit(f"mutation_check: no compile command for {OWNER_CPP} in {BUILD}")


def link_inputs() -> tuple[list[str], list[str]]:
    """(object inputs, libraries/flags) for the test executable, read from the build tree."""
    ninja = (BUILD / "build.ninja").read_text().splitlines()
    objects: list[str] = []
    flags: list[str] = []
    libraries: list[str] = []
    for index, line in enumerate(ninja):
        if line.startswith("build spider1_widescreen_test:"):
            head, _, tail = line.partition("|")
            objects = [tok for tok in shlex.split(head) if tok.endswith(".o")]
            objects += [tok for tok in shlex.split(tail) if tok.endswith((".a", ".so"))]
            for follower in ninja[index + 1 : index + 6]:
                key, _, value = follower.strip().partition(" =")
                if key == "LINK_FLAGS":
                    flags = shlex.split(value)
                elif key == "LINK_LIBRARIES":
                    libraries = shlex.split(value)
            break
    if not objects or not libraries:
        raise SystemExit("mutation_check: could not read the spider1_widescreen_test link line")
    # LINK_FLAGS FIRST: in CMake's own line it precedes LINK_LIBRARIES, and the order decides whether
    # `-lm` is seen before or after the archives that need it. A harness that reorders them links a
    # DIFFERENT program than the build does and then reports a baseline pass that says nothing about
    # the tree — the stale-binary failure wearing a different hat.
    return objects, flags + libraries


def build(directory: Path, label: str) -> Path:
    """Compile the owner from `directory` and link it with the already-built test object."""
    directory.mkdir(parents=True, exist_ok=True)
    binary = directory / "test"
    compile_command = owner_compile_command()
    # Replace the source, the output and force the mutated copy's own directory to win the quoted
    # `#include "spider1_widescreen.h"`.
    out: list[str] = []
    skip = False
    for token in compile_command:
        if skip:
            skip = False
            continue
        if token == "-o":
            skip = True
            continue
        if token == "-c" or token.startswith("-MD"):
            continue
        if token == str(OWNER_CPP):
            continue
        if token == "-MF":
            skip = True
            continue
        if token == f"{OWNER_CPP}.o" or token.endswith("spider1_widescreen.cpp.o"):
            continue
        out.append(token)
    # The include goes AFTER the compiler token: it is argv[0], so a flag in front of it becomes
    # the executable. The mutated directory also wins on its own, because a quoted include resolves
    # against the including file's directory first.
    command = out[:1] + ["-I" + str(directory)] + out[1:] + [
        "-c", str(directory / "spider1_widescreen.cpp"), "-o", str(directory / "owner.o")]
    result = subprocess.run(command, capture_output=True, text=True, cwd=BUILD)
    if result.returncode:
        print(result.stdout[-4000:])
        print(result.stderr[-4000:])
        raise SystemExit(f"mutation_check: {label} failed to compile")

    objects, libraries = link_inputs()
    test_object = BUILD / TEST_OBJECT
    if not test_object.is_file():
        raise SystemExit(f"mutation_check: no test object at {test_object}; build the target first")
    # Deduplicated because the link line names the test object explicitly AND again in its object
    # list, and a twice-linked main() is a link error that has nothing to do with the mutant.
    resolved = [str(directory / "owner.o"), str(test_object)]
    resolved += [str(BUILD / obj) if not obj.startswith("/") else obj for obj in objects
    # The owner's OWN object is what the mutant replaces; the test's object stays.
    # Linking both would measure the pristine owner instead of the mutant.
                 if obj.endswith(".o") and not obj.endswith(OWNER_OBJECT_SUFFIX)]
    resolved += [str(BUILD / lib) if not lib.startswith("/") else lib for lib in objects
                 if not lib.endswith(".o")]
    for token in libraries:
        resolved.append(token if token.startswith("-") or token.startswith("/")
                        else str(BUILD / token))
    seen: set[str] = set()
    unique = [token for token in resolved if not (token in seen or seen.add(token))]
    link = [shutil.which("clang++") or "clang++", "-o", str(binary)] + unique
    result = subprocess.run(link, capture_output=True, text=True, cwd=BUILD)
    if result.returncode:
        print(result.stdout[-4000:])
        print(result.stderr[-4000:])
        raise SystemExit(f"mutation_check: {label} failed to link")
    return binary


def run(binary: Path) -> tuple[int, str]:
    result = subprocess.run([str(binary)], capture_output=True, text=True,
                            cwd=str(ROOT), timeout=300)
    return result.returncode, result.stdout + result.stderr


def failing_tests(output: str) -> list[str]:
    return [
        line.split("test ", 1)[1].strip()
        for line in output.splitlines()
        if line.startswith("test ") and "  FAIL" in line
    ] or ([line.strip() for line in output.splitlines() if line.strip().startswith("FAIL ")])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    population = mutants()
    if args.list:
        for mutant in population:
            print(f"{mutant.name}: {mutant.why}")
        return 0

    if not OWNER_CPP.is_file() or not TEST_CPP.is_file():
        raise SystemExit("mutation_check: the owner or its test is missing")
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)

    # THE BASELINE, built by this script from the unmodified sources, before any mutation. Without
    # it a "kill" could be a stale binary, which is the exact flaw the Tomba! 1 harness hit.
    baseline_dir = WORK / "baseline"
    baseline_dir.mkdir()
    shutil.copy(OWNER_CPP, baseline_dir / "spider1_widescreen.cpp")
    shutil.copy(OWNER_H, baseline_dir / "spider1_widescreen.h")
    code, output = run(build(baseline_dir, "baseline"))
    baseline_summary = next((line for line in output.splitlines() if "tests passed" in line), "")
    print(f"baseline: built from the unmodified sources -> exit {code}  {baseline_summary}")
    if code != 0:
        print(output[-4000:])
        print("mutation_check: REFUSING to score mutants against a baseline that does not pass")
        return 1
    baseline_checks = baseline_summary

    killed: list[str] = []
    survived: list[str] = []
    for mutant in population:
        directory = WORK / mutant.name
        directory.mkdir()
        shutil.copy(OWNER_CPP, directory / "spider1_widescreen.cpp")
        shutil.copy(OWNER_H, directory / "spider1_widescreen.h")
        try:
            mutant.apply(directory)
        except SystemExit as exc:
            print(f"  SKIPPED {mutant.name}: {exc}")
            survived.append(mutant.name)
            continue
        code, output = run(build(directory, mutant.name))
        if code == 0:
            print(f"  SURVIVED {mutant.name}")
            survived.append(mutant.name)
            continue
        names = failing_tests(output)
        summary = next((line for line in output.splitlines() if "tests passed" in line), "")
        detail = ", ".join(names) if names else "the owner's own refusal (abort)"
        print(f"  killed    {mutant.name}: {detail}  [{summary.strip() or 'aborted'}]")
        killed.append(mutant.name)

    total = len(population)
    print(f"\nmutation_check: baseline {baseline_checks.strip()}")
    print(f"mutation_check: {len(killed)} of {total} mutant(s) killed")
    for name in survived:
        print(f"mutation_check: SURVIVED {name}")
    shutil.rmtree(WORK)
    return 0 if not survived else 1


if __name__ == "__main__":
    sys.exit(main())
