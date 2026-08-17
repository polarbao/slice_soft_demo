#!/usr/bin/env python3
"""Validate the SliceSoft source, build, runtime query, and UI version snapshot."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any

from jsonschema import Draft202012Validator


PRERELEASE_IDENTIFIER = (
    r"(?:0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)"
)
SEMVER = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    rf"(?:-{PRERELEASE_IDENTIFIER}(?:\.{PRERELEASE_IDENTIFIER})*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def ImplementationVersion(component: dict[str, Any]) -> str:
    prerelease = component["preRelease"]
    return component["version"] + (f"-{prerelease}" if prerelease else "")


def Run(command: list[str]) -> str:
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout={result.stdout}\nstderr={result.stderr}"
        )
    if result.stderr:
        raise AssertionError(
            f"version query wrote stderr: {' '.join(command)}: {result.stderr}"
        )
    return result.stdout.strip()


def ValidateSource(repo: Path) -> tuple[str, str, str]:
    manifest = LoadJson(repo / "version-manifest.json")
    sourceSchema = LoadJson(
        repo / "contracts/slicesoft_version_manifest.schema.json"
    )
    Draft202012Validator.check_schema(sourceSchema)
    Draft202012Validator(sourceSchema).validate(manifest)
    if manifest["schemaVersion"] != 1:
        raise AssertionError("unsupported source manifest schema")
    if manifest["versionScheme"] != "semver-2.0.0":
        raise AssertionError("source manifest must use SemVer 2.0.0")
    if manifest["releasePolicy"] != "lockstep":
        raise AssertionError("source manifest must use the approved lockstep policy")

    app = manifest["components"]["application"]
    slicer = manifest["components"]["slicer"]
    if app["id"] != "slicesoft-app" or slicer["id"] != "slicer":
        raise AssertionError("stable version component IDs drifted")
    appVersion = ImplementationVersion(app)
    slicerVersion = ImplementationVersion(slicer)
    releaseVersion = ImplementationVersion(manifest["release"])
    if not SEMVER.fullmatch(appVersion) or not SEMVER.fullmatch(slicerVersion):
        raise AssertionError("implementation version is not valid SemVer")
    if appVersion != slicerVersion or appVersion != releaseVersion:
        raise AssertionError("lockstep release, application, and slicer versions drifted")

    vcpkg = LoadJson(repo / "vcpkg.json")
    if vcpkg["version-string"] != appVersion:
        raise AssertionError("vcpkg package metadata drifted from source manifest")

    for schemaName in (
        "slicer_module_info.schema.json",
        "slicer_module_manifest.schema.json",
    ):
        versionRule = LoadJson(repo / "contracts" / schemaName)["properties"]["version"]
        if "const" in versionRule:
            raise AssertionError(f"{schemaName} reintroduced a hard-coded version")

    hostSource = (repo / "apps/slicer_ui_host_sim/HostMainWindow.cpp").read_text(
        encoding="utf-8"
    )
    for marker in (
        "HostVersionInfo::ApplicationTitle()",
        "HostVersionInfo::ApplicationDiagnosticText()",
        "HostVersionInfo::SlicerVersionFromModuleInfo",
    ):
        if marker not in hostSource:
            raise AssertionError(f"Qt version display marker is missing: {marker}")
    return appVersion, slicerVersion, manifest["release"]["status"]


def ValidateBuild(
    repo: Path,
    path: Path,
    appVersion: str,
    slicerVersion: str,
    releaseStatus: str,
    expectedConfig: str,
) -> dict[str, Any]:
    build = LoadJson(path)
    buildSchema = LoadJson(
        repo / "contracts/slicesoft_build_manifest.schema.json"
    )
    Draft202012Validator.check_schema(buildSchema)
    Draft202012Validator(buildSchema).validate(build)
    if build["schema"] != "slicesoft.build.1":
        raise AssertionError("build manifest schema drifted")
    if build["releaseStatus"] != releaseStatus:
        raise AssertionError("build release status drifted from source manifest")
    if build["build"]["config"] != expectedConfig:
        raise AssertionError("build manifest configuration drifted")
    revision = build["source"]["revision"]
    state = build["source"]["state"]
    if revision != "unknown" and not re.fullmatch(r"[0-9a-f]{12}", revision):
        raise AssertionError("build revision must be 12 hex characters or unknown")
    if state not in {"clean", "dirty", "unknown"}:
        raise AssertionError("build source state is invalid")
    if revision == "unknown" and state != "unknown":
        raise AssertionError("unknown revision cannot claim clean or dirty")

    components = build["components"]
    expected = {
        "application": ("slicesoft-app", appVersion),
        "slicer": ("slicer", slicerVersion),
    }
    for name, (componentId, version) in expected.items():
        component = components[name]
        if component["id"] != componentId or component["version"] != version:
            raise AssertionError(f"build component drifted: {name}")
        fullVersion = component["fullBuildVersion"]
        if not SEMVER.fullmatch(fullVersion) or not fullVersion.startswith(version + "+"):
            raise AssertionError(f"invalid full build version: {name}")
    return build


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--config", required=True, choices=("Debug", "Release"))
    parser.add_argument("--build-manifest", required=True, type=Path)
    parser.add_argument("--module-manifest", required=True, type=Path)
    parser.add_argument("--module-probe", required=True, type=Path)
    parser.add_argument("--worker", required=True, type=Path)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--ui", required=True, type=Path)
    arguments = parser.parse_args()

    repo = arguments.repo.resolve()
    appVersion, slicerVersion, releaseStatus = ValidateSource(repo)
    build = ValidateBuild(
        repo,
        arguments.build_manifest.resolve(),
        appVersion,
        slicerVersion,
        releaseStatus,
        arguments.config,
    )

    moduleManifest = LoadJson(arguments.module_manifest.resolve())
    if moduleManifest["version"] != slicerVersion:
        raise AssertionError("module.json version drifted")

    moduleInfo = json.loads(Run([str(arguments.module_probe.resolve()), "--print-json"]))
    if moduleInfo["version"] != slicerVersion or moduleInfo["spi"] != 1:
        raise AssertionError("pm_module_info version or SPI drifted")

    workerInfo = json.loads(Run([str(arguments.worker.resolve()), "--contract-info"]))
    if workerInfo["engineVersion"] != slicerVersion:
        raise AssertionError("Worker discovery version drifted")
    if workerInfo["major"] != 1 or workerInfo["produces"] != ["p0.rgbwsv.2"]:
        raise AssertionError("Worker frozen contract drifted")

    appFullVersion = build["components"]["application"]["fullBuildVersion"]
    expectedQuery = f"SliceSoft {appVersion}\nbuild {appFullVersion}"
    if Run([str(arguments.cli.resolve()), "--version"]) != expectedQuery:
        raise AssertionError("slicer_cli --version drifted")
    if Run([str(arguments.ui.resolve()), "--version"]) != expectedQuery:
        raise AssertionError("slicer_ui_host_sim --version drifted")

    print(
        "SliceSoft version contracts: PASS "
        f"app={appVersion} slicer={slicerVersion} config={arguments.config}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (AssertionError, KeyError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"SliceSoft version contracts: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
