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


def SplitSemVer(value: str) -> tuple[str, str, str, str]:
    """拆出 MAJOR、MINOR、PATCH 与预发布标识。"""
    core, _, prerelease = value.partition("-")
    parts = core.split(".")
    if len(parts) != 3:
        raise AssertionError(f"version is not a SemVer core triple: {value}")
    return parts[0], parts[1], parts[2], prerelease


def SameMajorMinorAndPrerelease(actual: str, expected: str) -> bool:
    """PATCH 由 git 派生，故只比对源清单真正治理的部分。"""
    actualMajor, actualMinor, actualPatch, actualPre = SplitSemVer(actual)
    expectedMajor, expectedMinor, _, expectedPre = SplitSemVer(expected)
    if not actualPatch.isdigit():
        raise AssertionError(f"derived patch is not a non-negative integer: {actual}")
    return (
        actualMajor == expectedMajor
        and actualMinor == expectedMinor
        and actualPre == expectedPre
    )


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
    # vcpkg.json 的 version-string 是依赖解析用的包标识，不随 PATCH 每次提交而动，
    # 故与源清单比对 MAJOR.MINOR 与预发布标识即可。
    if not SameMajorMinorAndPrerelease(vcpkg["version-string"], appVersion):
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
        # PATCH 由 git 派生（自最近 v* 标签起的提交数），源清单只维护 MAJOR.MINOR，
        # 故此处比对 MAJOR.MINOR 与预发布标识，PATCH 另行校验为非负整数。
        # 若在此逐字相等，每提交一次该断言就会红一次——它守的是「构建产物与源清单
        # 同源」，不是「PATCH 恒定」。
        if component["id"] != componentId:
            raise AssertionError(f"build component id drifted: {name}")
        if not SameMajorMinorAndPrerelease(component["version"], version):
            raise AssertionError(
                f"build component drifted: {name} "
                f"({component['version']} vs {version})"
            )
    # 锁步仍逐字校验：两个组件必须派生出同一个 PATCH，否则说明它们并非同一次构建产出。
    if components["application"]["version"] != components["slicer"]["version"]:
        raise AssertionError("build components broke the lockstep version policy")
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

    # 以下产物【都来自同一次构建】，彼此必须逐字一致；与源清单则只需 MAJOR.MINOR
    # 与预发布标识一致（PATCH 由 git 派生，见 SameMajorMinorAndPrerelease）。
    # 故基准取自构建清单而非源清单——拿源清单的 PATCH 去比对构建产物，
    # 每提交一次就会红一次，而那并不表示任何东西漂移了。
    builtSlicerVersion = build["components"]["slicer"]["version"]
    builtAppVersion = build["components"]["application"]["version"]

    moduleManifest = LoadJson(arguments.module_manifest.resolve())
    if moduleManifest["version"] != builtSlicerVersion:
        raise AssertionError(
            "module.json version drifted: "
            f"{moduleManifest['version']} != {builtSlicerVersion}"
        )

    moduleInfo = json.loads(Run([str(arguments.module_probe.resolve()), "--print-json"]))
    if moduleInfo["version"] != builtSlicerVersion or moduleInfo["spi"] != 1:
        raise AssertionError(
            "pm_module_info version or SPI drifted: "
            f"{moduleInfo['version']} != {builtSlicerVersion}"
        )

    workerInfo = json.loads(Run([str(arguments.worker.resolve()), "--contract-info"]))
    if workerInfo["engineVersion"] != builtSlicerVersion:
        raise AssertionError(
            "Worker discovery version drifted: "
            f"{workerInfo['engineVersion']} != {builtSlicerVersion}"
        )
    # 冻结承诺是「六通道 p0.rgbwsv.2 必须仍然产出」，不是「只能产出它」。
    # MATVOL-T 起 worker 同时宣称 p0.rgbwsvt.1，与方案 A 的口径一致
    # （见 DOC_DECISION_MATVOL_T_冻结契约file_contract_v1变更处置.md §8）：
    # 六通道为必需，额外协议允许出现。原先的相等断言会把「新增能力」误判为「冻结漂移」。
    if workerInfo["major"] != 1 or "p0.rgbwsv.2" not in workerInfo["produces"]:
        raise AssertionError("Worker frozen contract drifted")

    appFullVersion = build["components"]["application"]["fullBuildVersion"]
    expectedQuery = f"SliceSoft {builtAppVersion}\nbuild {appFullVersion}"
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
