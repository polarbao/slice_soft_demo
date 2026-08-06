#!/usr/bin/env python3
"""Validate the frozen Stage 14D-07 engine-conformance definition."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
from typing import Any


EXPECTED_THEME_IDS = [f"E-{index:02d}" for index in range(1, 9)]
EXPECTED_STATUSES = ["pass", "fail", "blocked", "not_run"]
EXPECTED_INVARIANTS = {
    "spiVersion": 1,
    "exportCount": 11,
    "capabilityCount": 15,
    "packageSchema": "p0.rgbwsv.2",
    "channels": ["R", "G", "B", "W", "S", "V"],
    "bitDepth": 8,
    "polarity": "black_is_print",
}


def LoadJson(path: pathlib.Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def ComputeSha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def Require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def ValidateContract(document: dict[str, Any]) -> None:
    Require(
        document.get("schema") == "slicesoft.engine_conformance.14d_07.1",
        "conformance schema identity is invalid",
    )
    Require(document.get("version") == "1.0", "conformance version must be 1.0")
    Require(document.get("statusValues") == EXPECTED_STATUSES, "status values changed")
    Require(
        document.get("overallRule") == "all_e01_through_e08_pass",
        "overall PASS rule changed",
    )
    Require(document.get("frozenInvariants") == EXPECTED_INVARIANTS, "frozen invariants changed")

    themes = document.get("themes")
    Require(isinstance(themes, list), "themes must be an array")
    Require([theme.get("id") for theme in themes] == EXPECTED_THEME_IDS, "E-01..E-08 order changed")
    for theme in themes:
        Require(bool(theme.get("title")), f"{theme.get('id')} title is empty")
        Require(bool(theme.get("comparisons")), f"{theme.get('id')} comparisons are empty")
        Require(bool(theme.get("passRule")), f"{theme.get('id')} PASS rule is empty")
        Require(isinstance(theme.get("blockedBy"), list), f"{theme.get('id')} blockers must be explicit")


def ValidateFixtures(repoRoot: pathlib.Path, document: dict[str, Any]) -> set[str]:
    Require(
        document.get("schema") == "slicesoft.engine_conformance.fixtures.14d_07.1",
        "fixture schema identity is invalid",
    )
    fixtures = document.get("fixtures")
    Require(isinstance(fixtures, list) and fixtures, "fixture list must not be empty")
    fixtureIds: set[str] = set()
    for fixture in fixtures:
        fixtureId = fixture.get("id")
        Require(isinstance(fixtureId, str) and fixtureId, "fixture id is invalid")
        Require(fixtureId not in fixtureIds, f"duplicate fixture id: {fixtureId}")
        fixtureIds.add(fixtureId)
        if fixture.get("kind") != "asset_set":
            Require(bool(fixture.get("generator")), f"generated fixture lacks generator: {fixtureId}")
            continue
        files = fixture.get("files")
        Require(isinstance(files, list) and files, f"asset fixture is empty: {fixtureId}")
        for item in files:
            relativePath = pathlib.PurePosixPath(item.get("path", ""))
            Require(
                not relativePath.is_absolute() and ".." not in relativePath.parts,
                f"unsafe fixture path: {relativePath}",
            )
            assetPath = repoRoot.joinpath(*relativePath.parts)
            Require(assetPath.is_file(), f"fixture asset is missing: {relativePath}")
            Require(
                ComputeSha256(assetPath) == item.get("sha256"),
                f"fixture digest changed without approval: {relativePath}",
            )
    return fixtureIds


def ValidateReferences(contract: dict[str, Any], fixtureIds: set[str]) -> None:
    for theme in contract["themes"]:
        for fixtureId in theme.get("fixtureIds", []):
            Require(fixtureId in fixtureIds, f"unknown fixture {fixtureId} in {theme['id']}")


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    repoRoot = arguments.repo_root.resolve()
    contract = LoadJson(repoRoot / "contracts" / "slicer_engine_conformance_v1.json")
    fixtures = LoadJson(
        repoRoot / "tests" / "stage14d_07" / "fixtures" / "fixture_identities.json"
    )
    ValidateContract(contract)
    fixtureIds = ValidateFixtures(repoRoot, fixtures)
    ValidateReferences(contract, fixtureIds)
    print(
        "Stage 14D-07 conformance definition: PASS "
        "(E-01..E-08 frozen; execution is a separate gate)"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"Stage 14D-07 conformance definition: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
