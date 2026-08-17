#!/usr/bin/env python3
"""Exercise clean, dirty, unknown, and invalid build-version generation."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def Run(command: list[str], *, cwd: Path | None = None, expectSuccess: bool = True) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    if expectSuccess and result.returncode != 0:
        raise AssertionError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout={result.stdout}\nstderr={result.stderr}"
        )
    if not expectSuccess and result.returncode == 0:
        raise AssertionError(f"command unexpectedly passed: {' '.join(command)}")
    return result.stdout.strip()


def Generate(cmake: Path, script: Path, source: Path, output: Path, config: str):
    command = [
        str(cmake),
        f"-DSOURCE_DIR={source}",
        f"-DOUTPUT_DIR={output}",
        f"-DCONFIG={config}",
        "-DAPP_ID=slicesoft-app",
        "-DAPP_NAME=SliceSoft",
        "-DAPP_VERSION=0.2.0-dev",
        "-DSLICER_ID=slicer",
        "-DSLICER_NAME=SliceSoft Geometry Slicer",
        "-DSLICER_VERSION=0.2.0-dev",
        "-DRELEASE_POLICY=lockstep",
        "-DRELEASE_STATUS=development",
        "-DTARGET_TRIPLET=x64-windows",
        "-DTIFF_BACKEND=libtiff",
        "-DOPENVDB_ENABLED=false",
        "-P",
        str(script),
    ]
    Run(command)
    with (output / "slicesoft_build_manifest.json").open(
        "r", encoding="utf-8"
    ) as stream:
        return json.load(stream)


def AssertState(manifest, expectedState: str, expectedRevision: str | None = None):
    source = manifest["source"]
    if source["state"] != expectedState:
        raise AssertionError(
            f"expected source state {expectedState}, got {source['state']}"
        )
    if expectedRevision is not None and source["revision"] != expectedRevision:
        raise AssertionError("generated revision does not match the fixture repository")
    fullVersion = manifest["components"]["application"]["fullBuildVersion"]
    if f".{expectedState}." not in fullVersion:
        raise AssertionError("full build version omitted the source state")


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True, type=Path)
    parser.add_argument("--generator-script", required=True, type=Path)
    parser.add_argument("--parser-script", required=True, type=Path)
    parser.add_argument("--source-manifest", required=True, type=Path)
    arguments = parser.parse_args()

    cmake = arguments.cmake.resolve()
    script = arguments.generator_script.resolve()
    parserScript = arguments.parser_script.resolve()
    sourceManifest = arguments.source_manifest.resolve()
    with tempfile.TemporaryDirectory(prefix="slicesoft-version-fixtures-") as temp:
        root = Path(temp)
        parserDriver = root / "parse-version.cmake"
        parserDriver.write_text(
            f'include("{parserScript.as_posix()}")\n'
            'slicesoft_load_version_manifest("${MANIFEST}")\n',
            encoding="utf-8",
        )
        Run(
            [
                str(cmake),
                f"-DMANIFEST={sourceManifest}",
                "-P",
                str(parserDriver),
            ],
            cwd=root,
        )
        with sourceManifest.open("r", encoding="utf-8-sig") as stream:
            validSourceManifest = json.load(stream)
        invalidManifests = []
        for mutation in ("status", "compatibility", "prerelease"):
            candidate = json.loads(json.dumps(validSourceManifest))
            if mutation == "status":
                candidate["release"]["status"] = "nonsense"
            elif mutation == "compatibility":
                candidate["compatibility"]["contracts"][0] = "slicer-module.spi.v2"
            else:
                candidate["release"]["preRelease"] = "01"
                candidate["components"]["application"]["preRelease"] = "01"
                candidate["components"]["slicer"]["preRelease"] = "01"
            invalidPath = root / f"invalid-{mutation}.json"
            invalidPath.write_text(json.dumps(candidate), encoding="utf-8")
            invalidManifests.append(invalidPath)
        for invalidManifest in invalidManifests:
            Run(
                [
                    str(cmake),
                    f"-DMANIFEST={invalidManifest}",
                    "-P",
                    str(parserDriver),
                ],
                cwd=root,
                expectSuccess=False,
            )

        repository = root / "repository"
        repository.mkdir()
        Run(["git", "init", "--quiet"], cwd=repository)
        Run(["git", "config", "user.email", "version-test@slicesoft.local"], cwd=repository)
        Run(["git", "config", "user.name", "SliceSoft Version Test"], cwd=repository)
        tracked = repository / "tracked.txt"
        tracked.write_text("clean\n", encoding="ascii")
        Run(["git", "add", "tracked.txt"], cwd=repository)
        Run(["git", "commit", "--quiet", "-m", "fixture"], cwd=repository)
        revision = Run(
            ["git", "rev-parse", "--short=12", "HEAD"], cwd=repository
        ).lower()

        clean = Generate(cmake, script, repository, root / "clean", "Release")
        AssertState(clean, "clean", revision)
        if clean["build"]["runtime"] != "MSVC-x64-MD":
            raise AssertionError("Release fixture used the wrong runtime identity")

        tracked.write_text("dirty\n", encoding="ascii")
        dirty = Generate(cmake, script, repository, root / "dirty", "Debug")
        AssertState(dirty, "dirty", revision)
        if dirty["build"]["runtime"] != "MSVC-x64-MDd":
            raise AssertionError("Debug fixture used the wrong runtime identity")

        nonRepository = root / "not-a-repository"
        nonRepository.mkdir()
        unknown = Generate(cmake, script, nonRepository, root / "unknown", "Release")
        AssertState(unknown, "unknown", "unknown")

        invalidCommand = [
            str(cmake),
            f"-DSOURCE_DIR={repository}",
            f"-DOUTPUT_DIR={root / 'invalid'}",
            "-DCONFIG=RelWithDebInfo",
            "-DAPP_ID=slicesoft-app",
            "-DAPP_NAME=SliceSoft",
            "-DAPP_VERSION=0.2.0-dev",
            "-DSLICER_ID=slicer",
            "-DSLICER_NAME=SliceSoft Geometry Slicer",
            "-DSLICER_VERSION=0.2.0-dev",
            "-DRELEASE_POLICY=lockstep",
            "-DRELEASE_STATUS=development",
            "-DTARGET_TRIPLET=x64-windows",
            "-DTIFF_BACKEND=libtiff",
            "-DOPENVDB_ENABLED=false",
            "-P",
            str(script),
        ]
        Run(invalidCommand, expectSuccess=False)

    print(
        "SliceSoft build-version fixtures: PASS "
        "manifest-fail-closed clean dirty unknown invalid-config"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (AssertionError, OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"SliceSoft build-version fixtures: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
