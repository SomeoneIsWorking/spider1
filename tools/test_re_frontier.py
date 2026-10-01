"""pytest wrapper so `re_frontier.py selftest` runs on the repo test path.

The real assertions live in the tool (`tools/re_frontier.py selftest`) so they
also run from a bare shell and can be pointed at another build with `--tool`.
This file exists so that a plain `pytest` from the repo root exercises them.

What it covers: a write (`set`, then `add`) against a prose-bearing roadmap must
leave every non-blank line of that roadmap in place, and the edit must actually
land — see docs/issues/0003, where the previous writer dropped 1846 lines of
hand-written rationale in one call.
"""
import os
import subprocess
import sys

TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "re_frontier.py")


def _selftest(*extra):
    r = subprocess.run([sys.executable, TOOL, "selftest", *extra],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "selftest OK" in r.stdout, r.stdout + r.stderr
    return r


def test_write_preserves_prose_embedded_fixture():
    _selftest()


def test_write_preserves_prose_on_this_repos_real_roadmap():
    roadmap = os.path.join(os.path.dirname(os.path.dirname(TOOL)),
                           "docs", "re-frontier.md")
    if not os.path.isfile(roadmap):
        raise AssertionError(f"{roadmap} is missing — this test checked NOTHING")
    _selftest("--corpus", roadmap, "--entry", "RE-12")
