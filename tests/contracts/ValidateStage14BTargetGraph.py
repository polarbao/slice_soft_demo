#!/usr/bin/env python3

import argparse
from pathlib import Path

from ValidateStage14BLayeringFeasibility import (
    AssignLayer,
    ReadSlicerCoreSources,
)


def ParseArguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate the configured Stage 14B base/engine graph."
    )
    parser.add_argument("--assignment", type=Path, required=True)
    return parser.parse_args()


def Main() -> int:
    arguments = ParseArguments()
    repoRoot = Path(__file__).resolve().parents[2]
    cmakePath = repoRoot / "CMakeLists.txt"
    cmake = cmakePath.read_text(encoding="utf-8")
    helper = (repoRoot / "cmake" / "SliceSoftCoreLayering.cmake").read_text(
        encoding="utf-8"
    )

    for required in (
        "add_library(slicer_base STATIC",
        "add_library(slicer_engine STATIC",
        "add_library(slicer_core INTERFACE)",
        "target_link_libraries(slicer_engine PUBLIC slicer_base)",
        "target_link_libraries(slicer_core INTERFACE slicer_engine)",
    ):
        if required not in cmake:
            raise AssertionError(f"Stage 14B target graph misses: {required}")
    if "SliceSoftPartitionCoreSources" not in helper or "engineExactSources" not in helper:
        raise AssertionError("Stage 14B layering helper is incomplete")
    if "target_link_libraries(slicer_base" in cmake and "slicer_engine" in cmake.split(
        "target_link_libraries(slicer_base", 1
    )[1].split(")", 1)[0]:
        raise AssertionError("slicer_base must not link slicer_engine")

    if not arguments.assignment.exists():
        raise AssertionError(f"configured assignment is missing: {arguments.assignment}")
    configured: dict[str, str] = {}
    for line in arguments.assignment.read_text(encoding="utf-8").splitlines():
        layer, source = line.split(" ", 1)
        if source in configured:
            raise AssertionError(f"source is compiled by two layers: {source}")
        configured[source] = layer

    sources = ReadSlicerCoreSources(cmakePath)
    if set(configured) != set(sources):
        raise AssertionError("configured target sources do not cover the core source list exactly")
    drift = {
        source: (AssignLayer(source), configured[source])
        for source in sources
        if AssignLayer(source) != configured[source]
    }
    if drift:
        raise AssertionError(f"configured base/engine assignment drifted: {drift}")

    baseCount = sum(layer == "base" for layer in configured.values())
    engineCount = sum(layer == "engine" for layer in configured.values())
    print(
        "Stage 14B target graph: PASS "
        f"(base={baseCount}, engine={engineCount}, duplicateSources=0)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
