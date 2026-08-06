#!/usr/bin/env python3
"""Exercise Stage 14C-04 lifetime, concurrency, and texture failure paths."""

from __future__ import annotations

import argparse
import ctypes
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys

from ValidateSyncCapabilityWiring import (
    AssertTerminalJob,
    BuildScene,
    ConfigureSpi,
    ReadJson,
    Require,
    Submit,
)


PM_ERR_INVALID_ARG = -3


def ImportModel(library: ctypes.CDLL, module: int, fixture: Path) -> str:
    job = Submit(
        library,
        module,
        {
            "capability": "model.import",
            "modelPath": str(fixture),
            "options": {"computeBBox": True, "extractMaterials": True},
        },
    )
    Require(job is not None, "model.import was not accepted")
    return AssertTerminalJob(library, job, "succeeded")["modelId"]


def ValidateConcurrentJobs(library: ctypes.CDLL, fixture: Path) -> None:
    module = library.pm_create(None)
    Require(module is not None, "concurrency module creation failed")
    try:
        modelId = ImportModel(library, module, fixture)

        def QueryMetadata(index: int) -> None:
            job = Submit(
                library,
                module,
                {"capability": "model.get_metadata", "modelId": modelId},
            )
            Require(job is not None, f"concurrent submit {index} failed")
            progress = ReadJson(library.pm_poll, job)
            result = ReadJson(library.pm_result, job)
            Require(progress["state"] == "succeeded", "concurrent poll not terminal")
            Require(result["modelId"] == modelId, "concurrent result crossed jobs")
            library.pm_release(job)

        with ThreadPoolExecutor(max_workers=8) as executor:
            futures = [executor.submit(QueryMetadata, index) for index in range(32)]
            for future in futures:
                future.result()

        releaseJob = Submit(
            library,
            module,
            {"capability": "model.release", "modelId": modelId},
        )
        Require(releaseJob is not None, "model.release was not accepted")
        AssertTerminalJob(library, releaseJob, "succeeded")
    finally:
        library.pm_destroy(module)


def ValidateDestroyCleansJobs(library: ctypes.CDLL, fixture: Path) -> None:
    module = library.pm_create(None)
    Require(module is not None, "cleanup module creation failed")
    job = Submit(
        library,
        module,
        {
            "capability": "geometry.preflight",
            "mode": "fast",
            "modelPath": str(fixture),
        },
    )
    Require(job is not None, "cleanup preflight was not accepted")
    Require(ReadJson(library.pm_poll, job)["state"] == "succeeded", "poll not terminal")
    Require(ReadJson(library.pm_result, job)["ok"] is True, "result not retained")
    library.pm_destroy(module)
    for function in (library.pm_poll, library.pm_result):
        required = ctypes.c_int(-1)
        Require(
            function(job, None, 0, ctypes.byref(required)) == PM_ERR_INVALID_ARG,
            "destroyed module left a live job handle",
        )


def ValidateMissingTextureFails(
    library: ctypes.CDLL,
    fixture: Path,
) -> None:
    module = library.pm_create(None)
    Require(module is not None, "texture module creation failed")
    try:
        ImportModel(library, module, fixture)
        scene = BuildScene(fixture)
        operationJob = Submit(
            library,
            module,
            {
                "capability": "scene.apply_operation",
                "scene": scene,
                "operationId": "stage14c04-missing-texture",
                "currentSceneRevision": 1,
                "expectedSceneRevision": 1,
                "operations": [
                    {
                        "type": "translate",
                        "instanceId": "stage14c04-instance",
                        "deltaMm": [0.0, 0.0, 0.0],
                    }
                ],
            },
        )
        Require(operationJob is not None, "missing-texture scene was not accepted")
        operation = AssertTerminalJob(library, operationJob, "succeeded")
        viewJob = Submit(
            library,
            module,
            {
                "capability": "scene.get_viewdata",
                "sceneId": "stage14c04-scene",
                "expectedSceneRevision": operation["newSceneRevision"],
                "content": ["bbox", "outline", "surface_preview", "appearance"],
                "viewMode": "top",
                "texturePolicy": "require_if_present",
                "lod": "auto",
                "meshTransform": "local",
                "maxBytes": 4 * 1024 * 1024,
            },
        )
        Require(viewJob is not None, "missing-texture ViewData was not accepted")
        result = AssertTerminalJob(library, viewJob, "failed")
        diagnostic = json.dumps(result, ensure_ascii=False).lower()
        Require(result["ok"] is False, "missing texture returned successful gray ViewData")
        Require("texture" in diagnostic, "missing texture failure was not explicit")
    finally:
        library.pm_destroy(module)


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--repo", default=Path.cwd(), type=Path)
    arguments = parser.parse_args()
    repo = arguments.repo.resolve()
    fixture = repo / "tests/fixtures/stage14b/model_with_normals.obj"
    missingTexture = (
        repo / "samples/models/textured/fixtures/missing_texture_small.obj"
    )
    Require(arguments.library.is_file(), "slicer module library does not exist")
    Require(fixture.is_file(), "model fixture does not exist")
    Require(missingTexture.is_file(), "missing-texture fixture does not exist")
    library = ctypes.CDLL(str(arguments.library.resolve()))
    ConfigureSpi(library)
    ValidateConcurrentJobs(library, fixture.resolve())
    ValidateDestroyCleansJobs(library, fixture.resolve())
    ValidateMissingTextureFails(library, missingTexture.resolve())
    print("Stage 14C-04 synchronous capability safety: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (AssertionError, KeyError, OSError, ValueError) as error:
        print(f"Stage 14C-04 synchronous capability safety: FAIL: {error}")
        sys.exit(1)
