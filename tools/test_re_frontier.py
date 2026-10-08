"""pytest wrapper so `re_frontier.py selftest` runs on the repo test path."""
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
