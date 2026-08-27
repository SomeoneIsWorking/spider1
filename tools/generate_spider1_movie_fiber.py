#!/usr/bin/env python3
"""Derive Spider-Man 1's resumable STR body from the local recompiled substrate.

The executable-derived substrate is intentionally untracked.  This tool keeps it that way: it
copies the authenticated retail player into the build tree, preserves every generated operation,
and replaces exactly the three blocking libetc VSync calls with the title-owned movie-field seam.
The original generated body remains compiled as the runtime-override super/oracle.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


FUNCTION_START = "void gen_func_8002AA0C(Core* c) {"
FUNCTION_END = "void gen_func_8002B28C(Core* c) {"
EXPECTED_RETURNS = ("8002AC8C", "8002AE1C", "8002AFEC")


def extract_player(source: str) -> str:
    start = source.find(FUNCTION_START)
    end = source.find(FUNCTION_END, start + len(FUNCTION_START))
    if start < 0 or end < 0:
        raise ValueError("generated substrate does not contain the measured 0x8002AA0C body")
    return source[start:end].rstrip() + "\n"


def transform_player(source: str) -> str:
    body = extract_player(source)
    body = body.replace(FUNCTION_START, "void spider1_native_movie_body(Core* c) {", 1)

    call_pattern = re.compile(
        r"c->r\[31\] = 0x(?P<return>[0-9A-F]{8})u;(?P<middle>[^\n]*\n[^\n]*?)"
        r"func_80084BE0\(c\);"
    )
    observed: list[str] = []

    def replace_call(match: re.Match[str]) -> str:
        return_pc = match.group("return")
        observed.append(return_pc)
        return (
            f"c->r[31] = 0x{return_pc}u;{match.group('middle')}"
            f"spider::spider1_movie_field(c, 0x{return_pc}u);"
        )

    body = call_pattern.sub(replace_call, body)
    if tuple(observed) != EXPECTED_RETURNS:
        raise ValueError(
            "STR VSync boundary drift: expected "
            f"{', '.join(EXPECTED_RETURNS)}, observed {', '.join(observed) or 'none'}"
        )
    if "func_80084BE0(c)" in body:
        raise ValueError("residual VSync call remains in the derived STR body")

    preamble = """// Generated at build time from the user's local recompiled SLUS_008.75 substrate.
// Do not edit: tools/generate_spider1_movie_fiber.py owns this derivative.
#include "core.h"
#include "rec_decls.h"
#include "spider1_movie_driver.h"

"""
    return preamble + body


def selftest() -> None:
    sample_calls = "\n".join(
        f"  c->r[31] = 0x{return_pc}u;\n"
        "  c->r[4] = c->r[0] + c->r[0]; rec_guest_instruction_ticks(c, 2u); "
        "func_80084BE0(c);"
        for return_pc in EXPECTED_RETURNS
    )
    sample = f"{FUNCTION_START}\n{sample_calls}\n}}\n\n{FUNCTION_END}\n}}\n"
    result = transform_player(sample)
    assert result.count("spider1_movie_field") == 3
    assert "func_80084BE0(c)" not in result
    assert "void gen_func_8002AA0C" not in result
    assert "void spider1_native_movie_body" in result

    try:
        transform_player(sample.replace("8002AE1C", "8002AE20"))
    except ValueError as error:
        assert "boundary drift" in str(error)
    else:
        raise AssertionError("changed return PC was accepted")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.selftest:
        selftest()
        return 0
    if args.input is None or args.output is None:
        raise ValueError("--input and --output are required unless --selftest is used")
    output = transform_player(args.input.read_text(encoding="utf-8"))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"generate_spider1_movie_fiber: {error}", file=sys.stderr)
        raise SystemExit(1)
