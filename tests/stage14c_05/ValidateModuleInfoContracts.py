#!/usr/bin/env python3
"""Validate Stage 14C-05 runtime information and deployment manifest."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any

from jsonschema import Draft202012Validator
from jsonschema.exceptions import ValidationError


EXPECTED_PROVIDES = (
    "model.import",
    "model.get_metadata",
    "model.release",
    "scene.apply_operation",
    "scene.get_snapshot",
    "scene.get_viewdata",
    "geometry.preflight",
    "geometry.collision",
    "geometry.repair",
    "slice.rgbwsv",
    "slice.rgbwsvt",
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report",
)

EXPECTED_SYNC = (
    "model.import",
    "model.get_metadata",
    "model.release",
    "scene.apply_operation",
    "scene.get_snapshot",
    "scene.get_viewdata",
    "geometry.preflight",
    "geometry.collision",
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report",
)

EXPECTED_WORKER = (
    "geometry.preflight",
    "geometry.repair",
    "slice.rgbwsv",
    "slice.rgbwsvt",
)


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def BuildValidator(path: Path) -> Draft202012Validator:
    schema = LoadJson(path)
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema)


def ExpectValid(
    validator: Draft202012Validator,
    value: Any,
    label: str,
) -> None:
    errors = list(validator.iter_errors(value))
    if errors:
        error = errors[0]
        raise AssertionError(f"{label}: {error.json_path}: {error.message}")


def ExpectInvalid(
    validator: Draft202012Validator,
    value: Any,
    label: str,
) -> None:
    try:
        validator.validate(value)
    except ValidationError:
        return
    raise AssertionError(f"{label} should be rejected")


def ImplementationVersion(component: dict[str, Any]) -> str:
    prerelease = component["preRelease"]
    return component["version"] + (f"-{prerelease}" if prerelease else "")


def BuildModuleInfo(buildConfig: str, version: str) -> dict[str, Any]:
    runtime = "MSVC-x64-MDd" if buildConfig == "Debug" else "MSVC-x64-MD"
    return {
        "schema": "slicesoft.module_info.1",
        "id": "slicer",
        "name": "SliceSoft Geometry Slicer",
        "version": version,
        "spi": 1,
        "runtime": runtime,
        "buildConfig": buildConfig,
        "provides": list(EXPECTED_PROVIDES),
        "produces": [
            {"contract": "p0.rgbwsv.2", "kind": "package"},
            {"contract": "p0.rgbwsvt.1", "kind": "package"},
        ],
        "capabilities": {
            "maxConcurrentJobs": 1,
            "cancelLatencyMs": 2000,
            "syncCapabilities": list(EXPECTED_SYNC),
            "workerCapabilities": list(EXPECTED_WORKER),
        },
    }


def RenderManifest(
    templatePath: Path,
    buildConfig: str,
    version: str,
) -> dict[str, Any]:
    runtime = "MSVC-x64-MDd" if buildConfig == "Debug" else "MSVC-x64-MD"
    text = templatePath.read_text(encoding="utf-8")
    text = text.replace("@SLICESOFT_MODULE_RUNTIME@", runtime)
    text = text.replace("@SLICESOFT_MODULE_BUILD_CONFIG@", buildConfig)
    text = text.replace("@SLICESOFT_SLICER_IMPLEMENTATION_VERSION@", version)
    if re.search(r"@[A-Z0-9_]+@", text):
        raise AssertionError("module.json.in contains an unresolved placeholder")
    return json.loads(text)


def RunProbe(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise AssertionError(f"module-info probe does not exist: {path}")
    result = subprocess.run(
        [str(path.resolve()), "--print-json"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.stderr:
        raise AssertionError(f"module-info probe wrote stderr: {result.stderr}")
    return json.loads(result.stdout)


def ValidateFrozenSources(repo: Path) -> None:
    dto = LoadJson(repo / "contracts/slicer_capability_dtos.json")
    dtoCapabilities = tuple(item["id"] for item in dto["capabilities"])
    if dtoCapabilities != EXPECTED_PROVIDES:
        raise AssertionError("15-capability DTO source drifted")

    syncHeader = (
        repo / "src/slicer_module/SyncCapabilityAdapter.h"
    ).read_text(encoding="utf-8")
    declaration = re.search(
        r"SyncCapabilities\s*\{(?P<body>.*?)\};",
        syncHeader,
        re.DOTALL,
    )
    if declaration is None:
        raise AssertionError("SyncCapabilities declaration is missing")
    syncCapabilities = tuple(
        re.findall(r'"([a-z0-9_.]+)"', declaration.group("body"))
    )
    if syncCapabilities != EXPECTED_SYNC:
        raise AssertionError("13 synchronous capabilities drifted")

    moduleSources = "\n".join(
        (repo / path).read_text(encoding="utf-8")
        for path in (
            "src/slicer_module/ModuleInfo.h",
            "src/slicer_module/ModuleInfo.cpp",
        )
    )
    if "Qt" in moduleSources or "PrintSDK" in moduleSources:
        raise AssertionError("ModuleInfo introduced a forbidden dependency")
    if "geometry.preflight.fast" in moduleSources:
        raise AssertionError("ModuleInfo invented a public preflight capability")


def ValidateCrossConsistency(moduleInfo: dict[str, Any], manifest: dict[str, Any]) -> None:
    for field in (
        "id",
        "name",
        "version",
        "spi",
        "runtime",
        "buildConfig",
        "provides",
        "produces",
    ):
        if moduleInfo[field] != manifest[field]:
            raise AssertionError(f"module info/manifest field drifted: {field}")


def ValidateTamperCases(
    infoValidator: Draft202012Validator,
    manifestValidator: Draft202012Validator,
    moduleInfo: dict[str, Any],
    manifest: dict[str, Any],
) -> None:
    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["runtime"] = "MSVC-x64-MD"
    ExpectInvalid(infoValidator, invalidInfo, "Debug runtime mismatch")

    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["buildConfig"] = "RelWithDebInfo"
    ExpectInvalid(infoValidator, invalidInfo, "unsupported build configuration")

    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["version"] = "0.1"
    ExpectInvalid(infoValidator, invalidInfo, "non-SemVer version")

    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["provides"][0] = "model.unknown"
    ExpectInvalid(infoValidator, invalidInfo, "capability replacement")

    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["provides"][1] = invalidInfo["provides"][0]
    ExpectInvalid(infoValidator, invalidInfo, "duplicate capability")

    invalidInfo = copy.deepcopy(moduleInfo)
    invalidInfo["unknown"] = True
    ExpectInvalid(infoValidator, invalidInfo, "unknown module-info field")

    invalidManifest = copy.deepcopy(manifest)
    invalidManifest["dll"] = "C:/modules/slicer_module.dll"
    ExpectInvalid(manifestValidator, invalidManifest, "absolute DLL path")

    invalidManifest = copy.deepcopy(manifest)
    invalidManifest["subprocess"]["exe"] = "../slicer_worker.exe"
    ExpectInvalid(manifestValidator, invalidManifest, "Worker path traversal")

    invalidManifest = copy.deepcopy(manifest)
    invalidManifest["delayLoad"] = True
    ExpectInvalid(manifestValidator, invalidManifest, "legacy delayLoad field")


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=Path.cwd(), type=Path)
    # P0FIX/P0-04：探针与构建配置改为【必填】。此前它们是可选参数，缺失时
    # Main() 会退化成 moduleInfo = expectedInfo 的自我比对，使"运行时自述漂移"
    # 这条断言恒真通过——那正是掩盖了 pm_module_info 违反自身 schema 的原因。
    # 不给默认值、不给回退分支：参数缺失就让 argparse 直接失败。
    parser.add_argument("--config", required=True, choices=("Debug", "Release"))
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--build-manifest", required=True, type=Path)
    arguments = parser.parse_args()
    repo = arguments.repo.resolve()

    infoValidator = BuildValidator(
        repo / "contracts/slicer_module_info.schema.json"
    )
    manifestValidator = BuildValidator(
        repo / "contracts/slicer_module_manifest.schema.json"
    )
    templatePath = repo / "src/slicer_module/module.json.in"

    # 运行时自述里的 version 带构建计数（形如 0.2.467-dev），与 version-manifest.json
    # 声明的核心版本（0.2.0-dev）不是同一个值。比对真实产物必须用构建清单里的已构建
    # 版本，否则会在 version 字段上假失败，与能力集无关。
    builtVersion = LoadJson(arguments.build_manifest)["components"]["slicer"]["version"]

    ValidateFrozenSources(repo)
    for buildConfig in ("Debug", "Release"):
        manifest = RenderManifest(templatePath, buildConfig, builtVersion)
        ExpectValid(manifestValidator, manifest, f"{buildConfig} manifest")
        if buildConfig != arguments.config:
            # 多配置生成器下一次只构建一个 config，另一个没有产物可探。
            # 明确跳过并打印，不得用期望值冒充真实产物。
            print(
                f"SKIP {buildConfig} runtime probe: not built in this configuration",
                file=sys.stderr,
            )
            continue
        expectedInfo = BuildModuleInfo(buildConfig, builtVersion)
        moduleInfo = RunProbe(arguments.probe)
        ExpectValid(infoValidator, moduleInfo, f"{buildConfig} runtime module info")
        ValidateCrossConsistency(moduleInfo, manifest)
        if moduleInfo != expectedInfo:
            raise AssertionError(f"{buildConfig} runtime module info drifted")

    debugInfo = BuildModuleInfo("Debug", builtVersion)
    debugManifest = RenderManifest(templatePath, "Debug", builtVersion)
    ValidateTamperCases(
        infoValidator,
        manifestValidator,
        debugInfo,
        debugManifest,
    )
    print("Stage 14C-05 module info and manifest contracts: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (AssertionError, KeyError, OSError, ValueError) as error:
        print(f"Stage 14C-05 contracts: FAIL: {error}")
        sys.exit(1)
