#!/usr/bin/env python3

import argparse
import re
from collections import Counter
from pathlib import Path


BASE_PREFIXES = (
    "src/third_party/miniz/",
    "src/slicer_core/api/",
    "src/slicer_core/importers/",
    "src/slicer_core/layout/",
    "src/slicer_core/model/",
    "src/slicer_core/scene/",
)

BASE_EXACT_STEMS = {
    "src/slicer_core/diagnostics/Diagnostics",
    "src/slicer_core/diagnostics/ProductionAdmissionPolicy",
    "src/slicer_core/diagnostics/ValidationIssue",
    "src/slicer_core/config/OutputResolution",
    "src/slicer_core/geometry/SceneModelTriangleMeshAdapter",
    "src/slicer_core/geometry/MeshTopologyDiagnostics",
    "src/slicer_core/geometry/TransformedModelAdapter",
    "src/slicer_core/geometry/TriangleMeshData",
    "src/slicer_core/json_value",
    "src/slicer_core/model",
    "src/slicer_core/output/rgbwsv/RgbwsvPackage",
    "src/slicer_core/output/rgbwsv/RgbwsvSceneExtension",
    "src/slicer_core/output/rgbwsvt/RgbwsvtPackageReader",
    "src/slicer_core/output/rgbwsvt/RgbwsvtPackageSchemaReader",
    "src/slicer_core/output/rgbwsvt/RgbwsvtTiffIo",
    "src/slicer_core/materials/transfer/TransferChannelError",
    "src/slicer_core/preview/MaterialPreviewComposer",
    "src/slicer_core/preview/ProductionLayerRef",
    "src/slicer_core/preview/TiffLayerCache",
    "src/slicer_core/preview/TiffLayerSource",
    "src/slicer_core/reports/ReportBase",
    "src/slicer_core/reports/ReportSchema",
    "src/slicer_core/SliceRunTelemetry",
    "src/slicer_core/reports/ReportSchemaValidator",
    "src/slicer_core/rip_reader",
    "src/slicer_core/system/Sha256",
    "src/slicer_core/system/Sha256Internal",
    "src/slicer_core/TiffPackBitsReadInternal",
    "src/slicer_core/TiffReadApi",
    "src/slicer_core/TiffReadStructureInternal",
    "src/slicer_core/texture_image",
}

BASE_EXACT_SOURCES = {
    "src/slicer_core/tiff_io.cpp",
}

ENGINE_EXACT_SOURCES = {
    "src/slicer_core/model/ModelLoadConfigAdapter.cpp",
    "src/slicer_core/scene/SceneEffectiveConfig",
}

KNOWN_BASE_TO_ENGINE_INCLUDES: set[tuple[str, str]] = set()


def ParseArguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate the Stage 14B base/engine feasibility assignment."
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Print the deterministic file-level assignment.",
    )
    return parser.parse_args()


def NormalizePath(path: str) -> str:
    return path.replace("\\", "/")


def ReadSlicerCoreSources(cmakePath: Path) -> list[str]:
    content = cmakePath.read_text(encoding="utf-8")
    match = re.search(r"set\(SLICESOFT_CORE_SOURCES\s+(.*?)\n\)", content, re.DOTALL)
    if match is None:
        raise AssertionError("SLICESOFT_CORE_SOURCES was not found in CMakeLists.txt")
    sources = [
        NormalizePath(line.strip())
        for line in match.group(1).splitlines()
        if line.strip()
    ]
    if not sources:
        raise AssertionError("slicer_core source list is empty")
    return sources


def SourceStem(source: str) -> str:
    return str(Path(source).with_suffix("")).replace("\\", "/")


def AssignLayer(source: str) -> str:
    stem = SourceStem(source)
    if source in ENGINE_EXACT_SOURCES or stem in ENGINE_EXACT_SOURCES:
        return "engine"
    if source in BASE_EXACT_SOURCES:
        return "base"
    if stem in BASE_EXACT_STEMS:
        return "base"
    if any(source.startswith(prefix) for prefix in BASE_PREFIXES):
        return "base"
    return "engine"


def ReadProjectIncludes(repoRoot: Path, source: str) -> list[str]:
    sourcePath = repoRoot / source
    if sourcePath.suffix not in {".cpp", ".h"}:
        return []
    content = sourcePath.read_text(encoding="utf-8")
    includes = re.findall(r'^#include\s+"([^"]+)"', content, re.MULTILINE)
    return [
        "src/" + NormalizePath(includePath)
        for includePath in includes
        if includePath.startswith("slicer_core/")
    ]


def Main() -> int:
    arguments = ParseArguments()
    repoRoot = Path(__file__).resolve().parents[2]
    sources = ReadSlicerCoreSources(repoRoot / "CMakeLists.txt")
    sourceSet = set(sources)
    missingSources = [source for source in sources if not (repoRoot / source).exists()]
    if missingSources:
        raise AssertionError(f"CMake lists missing source files: {missingSources}")

    assignments = {source: AssignLayer(source) for source in sources}
    baseToEngineIncludes = set()
    for source, sourceLayer in assignments.items():
        if sourceLayer != "base":
            continue
        for includePath in ReadProjectIncludes(repoRoot, source):
            if includePath not in sourceSet:
                continue
            if assignments[includePath] == "engine":
                baseToEngineIncludes.add((source, includePath))

    unexpectedEdges = baseToEngineIncludes - KNOWN_BASE_TO_ENGINE_INCLUDES
    missingKnownEdges = KNOWN_BASE_TO_ENGINE_INCLUDES - baseToEngineIncludes
    if unexpectedEdges:
        raise AssertionError(
            f"unexpected base -> engine include edges: {sorted(unexpectedEdges)}"
        )
    if missingKnownEdges:
        raise AssertionError(
            "the documented extraction list drifted: "
            f"{sorted(missingKnownEdges)}"
        )

    modelSource = repoRoot / "src" / "slicer_core" / "model.cpp"
    modelIncludes = ReadProjectIncludes(repoRoot, "src/slicer_core/model.cpp")
    # MATOPQ G-01（用户 2026-08-31 授权）：MTL 不透明度解析下沉新增 base 层解析助手。
    # 该头与 ObjFaceParser.h 同层（BASE_PREFIXES 含 src/slicer_core/model/）、同目录、
    # 同命名空间、同角色，未产生 base -> engine 边；且 model.cpp 因此由 1982 缩减至 1941 行，
    # 属本合同期望的抽取方向而非依赖累积。授权留痕见
    # docs/slice/DOC/DOC_DECISION_MATOPQ_材质不透明度识别与光油通道映射专项.md 第 12 节。
    expectedModelIncludes = [
        "src/slicer_core/model.h",
        "src/slicer_core/model/ObjFaceParser.h",
        "src/slicer_core/model/MtlMaterialParser.h",
    ]
    if modelIncludes != expectedModelIncludes:
        raise AssertionError(
            "model.cpp acquired a project dependency outside the frozen base parser boundary: "
            f"{modelIncludes}"
        )
    if "OpenVdb" in modelSource.read_text(encoding="utf-8"):
        raise AssertionError("model.cpp must not depend on OpenVDB")
    if "slicer_core/config.h" in (repoRoot / "src/slicer_core/model.h").read_text(
        encoding="utf-8"
    ):
        raise AssertionError("model.h must use the narrow ModelLoadConfig contract")

    if arguments.list:
        for source in sources:
            print(f"{assignments[source]:6} {source}")

    counts = Counter(assignments.values())
    print(
        "Stage 14B layering feasibility: PASS "
        f"(sources={len(sources)}, base={counts['base']}, "
        f"engine={counts['engine']}, extractionEdges={len(baseToEngineIncludes)})"
    )
    print("model.import assignment: base (narrow import config extracted)")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
