#!/usr/bin/env python3

import json
from pathlib import Path


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    manifestPath = repoRoot / "contracts" / "third_party_distribution_manifest.json"
    with manifestPath.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)

    noticePath = repoRoot / manifest["releaseFiles"]["notice"]
    noticeText = noticePath.read_text(encoding="utf-8")
    expectedComponents = {"miniz", "libtiff", "assimp", "meshoptimizer"}
    actualComponents = {entry["name"] for entry in manifest["components"]}
    if actualComponents != expectedComponents:
        raise AssertionError("third-party component inventory drifted")

    for component in manifest["components"]:
        licensePath = repoRoot / component["licenseFile"]
        if not licensePath.is_file() or licensePath.stat().st_size < 500:
            raise AssertionError(f"missing full license: {component['name']}")
        if component["name"].lower() not in noticeText.lower():
            raise AssertionError(f"NOTICE omits component: {component['name']}")

    minizText = (repoRoot / "licenses" / "miniz.txt").read_text(encoding="utf-8")
    if "Permission is hereby granted" not in minizText:
        raise AssertionError("miniz MIT text is incomplete")
    if "public domain" not in minizText:
        raise AssertionError("miniz public-domain dedication is incomplete")

    tiffText = (repoRoot / "licenses" / "libtiff.txt").read_text(encoding="utf-8")
    if "Sam Leffler" not in tiffText or "Lempel-Ziv" not in tiffText:
        raise AssertionError("LibTIFF notices are incomplete")

    assimpText = (repoRoot / "licenses" / "assimp.txt").read_text(encoding="utf-8")
    if "assimp team" not in assimpText or "Poly2Tri" not in assimpText:
        raise AssertionError("Assimp notices are incomplete")

    meshoptimizerText = (
        repoRoot / "licenses" / "meshoptimizer.txt"
    ).read_text(encoding="utf-8")
    if "Arseny Kapoulkine" not in meshoptimizerText:
        raise AssertionError("meshoptimizer copyright is incomplete")
    if "Permission is hereby granted" not in meshoptimizerText:
        raise AssertionError("meshoptimizer MIT text is incomplete")

    if not all(manifest["releaseGate"].values()):
        raise AssertionError("release compliance gate must be fail-closed")

    print("third-party NOTICE and license inventory: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
