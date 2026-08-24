"""Both-answer tests for serial-driven Spider title selection."""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from title_catalog import Title, TitleCatalogError, load_catalog


def expect_refusal(catalog, contents: str, fragment: str) -> None:
    try:
        catalog.from_system_cnf(contents)
    except TitleCatalogError as exc:
        assert fragment in str(exc), (fragment, str(exc))
        return
    raise AssertionError(f"selection unexpectedly accepted {contents!r}")


def main() -> int:
    catalog = load_catalog(ROOT)
    checks = 0

    checks += 1
    assert catalog.from_system_cnf("BOOT=cdrom:\\SLUS_008.75;1\n").id == "spiderman1"
    checks += 1
    assert (
        catalog.from_system_cnf("BOOT = cdrom:\\SLUS_013.78;1\r\n").id == "spiderman2"
    )
    checks += 1
    assert catalog.from_system_cnf("boot=cdrom:SLUS_013.78;1\n").serial == "SLUS_013.78"
    checks += 1
    assert catalog.by_id("spiderman1").executable_sha256.startswith("d2270e35")
    checks += 1
    assert catalog.by_id("spiderman2").executable_sha256.startswith("dbe6c3f3")
    checks += 2
    assert catalog.by_id("spiderman1").file_size == 749568
    assert catalog.by_id("spiderman2").file_size == 786432

    checks += 1
    expect_refusal(catalog, "TCB = 4\n", "exactly one BOOT")
    checks += 1
    expect_refusal(
        catalog, "BOOT=cdrom:\\SLUS_999.99;1\n", "unsupported boot executable"
    )
    checks += 1
    expect_refusal(
        catalog,
        "BOOT=cdrom:\\SLUS_008.75;1\nBOOT=cdrom:\\SLUS_013.78;1\n",
        "exactly one BOOT",
    )
    checks += 1
    try:
        catalog.by_id("spiderman")
    except TitleCatalogError as exc:
        assert "unknown title" in str(exc)
    else:
        raise AssertionError("human-facing generic name selected a title")

    checks += 1
    manifest = json.loads(
        (ROOT / "titles" / "spiderman2" / "title.json").read_text(encoding="utf-8")
    )
    manifest["executableSha256"] = "not-a-hash"
    with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
        malformed = Path(directory) / "title.json"
        malformed.write_text(json.dumps(manifest), encoding="utf-8")
        try:
            Title.from_manifest(malformed)
        except TitleCatalogError as exc:
            assert "malformed executable SHA-256" in str(exc)
        else:
            raise AssertionError("malformed executable hash accepted")

    checks += 1
    manifest = json.loads(
        (ROOT / "titles" / "spiderman2" / "title.json").read_text(encoding="utf-8")
    )
    manifest["fileSize"] = True
    with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
        malformed = Path(directory) / "title.json"
        malformed.write_text(json.dumps(manifest), encoding="utf-8")
        try:
            Title.from_manifest(malformed)
        except TitleCatalogError as exc:
            assert "invalid executable file size" in str(exc)
        else:
            raise AssertionError("invalid executable file size accepted")

    checks += 1
    manifest["fileSize"] = 786432
    manifest["runtimeModules"] = "false"
    with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
        malformed = Path(directory) / "title.json"
        malformed.write_text(json.dumps(manifest), encoding="utf-8")
        try:
            Title.from_manifest(malformed)
        except TitleCatalogError as exc:
            assert "runtimeModules must be true or false" in str(exc)
        else:
            raise AssertionError("string runtimeModules value accepted")

    print(f"title catalog: PASS — {checks} supported/refusal checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
