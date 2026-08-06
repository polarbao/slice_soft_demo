#!/usr/bin/env python3
"""Validate Stage 14C-04 synchronous light-capability routing through the SPI."""

from __future__ import annotations

import argparse
import ctypes
import json
from pathlib import Path
import re
import sys


PM_ERR_BUFFER_SMALL = -2
PM_ERR_INVALID_ARG = -3
EXPECTED_SYNC_CAPABILITIES = (
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


def Require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def ConfigureSpi(library: ctypes.CDLL) -> None:
    library.pm_spi_version.restype = ctypes.c_int
    library.pm_create.argtypes = [ctypes.c_char_p]
    library.pm_create.restype = ctypes.c_void_p
    library.pm_destroy.argtypes = [ctypes.c_void_p]
    library.pm_destroy.restype = None
    library.pm_submit.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    library.pm_submit.restype = ctypes.c_void_p
    for name in ("pm_poll", "pm_result"):
        function = getattr(library, name)
        function.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_int),
        ]
        function.restype = ctypes.c_int
    library.pm_release.argtypes = [ctypes.c_void_p]
    library.pm_release.restype = None
    library.pm_last_error.argtypes = [
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_int),
    ]
    library.pm_last_error.restype = ctypes.c_int


def ReadBuffer(function: object, handle: int | None = None) -> bytes:
    required = ctypes.c_int(-1)
    arguments = ([handle] if handle is not None else []) + [
        None,
        0,
        ctypes.byref(required),
    ]
    Require(function(*arguments) == PM_ERR_BUFFER_SMALL, "buffer probe failed")
    Require(required.value > 0, "buffer probe returned no data")
    output = ctypes.create_string_buffer(required.value + 1)
    arguments = ([handle] if handle is not None else []) + [
        output,
        len(output),
        ctypes.byref(required),
    ]
    Require(function(*arguments) == required.value, "buffer write length mismatch")
    return output.raw[: required.value]


def ReadJson(function: object, handle: int | None = None) -> dict:
    return json.loads(ReadBuffer(function, handle).decode("utf-8"))


def Submit(library: ctypes.CDLL, module: int, request: dict) -> int | None:
    encoded = json.dumps(request, ensure_ascii=False).encode("utf-8")
    return library.pm_submit(module, encoded)


def BuildScene(fixture: Path) -> dict:
    bounds = {
        "min": {"x": 0.0, "y": 0.0, "z": 0.0},
        "max": {"x": 1.0, "y": 1.0, "z": 0.0},
    }
    transform = {
        "translateXMm": 0.0,
        "translateYMm": 0.0,
        "rotateZDeg": 0.0,
        "uniformScale": 1.0,
        "mirrorX": False,
        "mirrorY": False,
    }
    return {
        "schema": "slicesoft.multimodel_scene.13b.1",
        "subjectType": "scene",
        "sceneId": "stage14c04-scene",
        "sceneRevision": 1,
        "buildVolume": {
            "source": "device_profile",
            "widthMm": 300.0,
            "heightMm": 100.0,
            "origin": "lower_left",
            "xDirection": "positive",
            "yDirection": "positive",
            "isFixture": False,
        },
        "layout": {
            "policy": "grid",
            "maxColumns": 1,
            "maxRows": 1,
            "columnGapMm": 10.0,
            "rowGapMm": 10.0,
            "spacingMode": "edge_clearance",
            "order": "row_major",
        },
        "materialBindingMode": "scene_profile_only",
        "resolvedProfileId": "stage14c04-profile",
        "resourceScopes": [
            {
                "resourceScopeId": "stage14c04-scope",
                "kind": "obj_directory",
                "rootPath": str(fixture.parent),
                "packagePath": "",
                "partIdentity": "",
            }
        ],
        "models": [
            {
                "modelId": "stage14c04-model",
                "sourcePath": str(fixture),
                "format": "obj",
                "resourceScopeId": "stage14c04-scope",
                "sourceHash": "stage14c04-source",
                "resourceHash": "stage14c04-resource",
                "displayName": "Stage 14C-04 fixture",
            }
        ],
        "instances": [
            {
                "instanceId": "stage14c04-instance",
                "modelId": "stage14c04-model",
                "sourceTransformIdentity": "stage14c04-transform",
                "requestedTransform": transform,
                "derivedLayoutTransform": transform,
                "effectiveTransform": transform,
                "visible": True,
                "locked": False,
                "transformRevision": 0,
                "sourceBboxMm": bounds,
                "effectiveBboxMm": bounds,
                "admissionStatus": "admitted",
                "resolvedProfileId": "stage14c04-profile",
            }
        ],
    }


def AssertTerminalJob(
    library: ctypes.CDLL,
    job: int,
    expectedState: str,
) -> dict:
    progress = ReadJson(library.pm_poll, job)
    result = ReadJson(library.pm_result, job)
    library.pm_release(job)
    Require(
        progress["state"] == expectedState,
        f"{progress.get('capability')} first poll was {progress['state']}, "
        f"expected {expectedState}: {result}",
    )
    Require(progress["percent"] == 100, "terminal progress was not 100 percent")
    return result


def AssertTerminalBinaryJob(library: ctypes.CDLL, job: int) -> bytes:
    progress = ReadJson(library.pm_poll, job)
    result = ReadBuffer(library.pm_result, job)
    library.pm_release(job)
    Require(progress["state"] == "succeeded", "binary job was not terminal success")
    Require(progress["percent"] == 100, "binary job progress was not complete")
    return result


def AssertReleasedJobInvalid(library: ctypes.CDLL, job: int) -> None:
    for function in (library.pm_poll, library.pm_result):
        required = ctypes.c_int(-1)
        Require(
            function(job, None, 0, ctypes.byref(required)) == PM_ERR_INVALID_ARG,
            "released job remained visible through the SPI",
        )


def ValidateSourceContract(repo: Path) -> None:
    adapter = (repo / "src/slicer_module/SyncCapabilityAdapter.h").read_text(
        encoding="utf-8"
    )
    declaration = re.search(
        r"SyncCapabilities\s*\{(?P<body>.*?)\};",
        adapter,
        re.DOTALL,
    )
    Require(declaration is not None, "syncCapabilities declaration is missing")
    actual = tuple(re.findall(r'"([a-z0-9_.]+)"', declaration.group("body")))
    Require(actual == EXPECTED_SYNC_CAPABILITIES, "sync capability set drifted")
    Require("geometry.preflight.fast" not in adapter, "invented preflight ID found")

    sceneAdapter = "\n".join(
        (repo / path).read_text(encoding="utf-8")
        for path in (
            "src/slicer_module/SceneCapabilityAdapter.h",
            "src/slicer_module/SceneCapabilityAdapter.cpp",
        )
    )
    Require(
        "CreateTexturedSceneViewDataProvider" in sceneAdapter,
        "scene.get_viewdata does not reuse the 14B-03A provider",
    )
    Require(
        "facade->GetViewData" in sceneAdapter,
        "scene.get_viewdata bypasses SceneFacade",
    )


def ValidateRuntime(library: ctypes.CDLL, fixture: Path) -> None:
    ConfigureSpi(library)
    Require(library.pm_spi_version() == 1, "SPI version changed")
    module = library.pm_create(None)
    Require(module is not None, "pm_create failed")
    try:
        fullJob = Submit(
            library,
            module,
            {
                "capability": "geometry.preflight",
                "mode": "full",
                "modelPath": str(fixture),
            },
        )
        Require(fullJob is None, "full preflight was accepted in-process")
        fullError = ReadJson(library.pm_last_error)
        Require("Worker" in fullError["message"], "full preflight lacks Worker routing")

        for invalidMode in ("slow", "FAST"):
            invalidModeJob = Submit(
                library,
                module,
                {
                    "capability": "geometry.preflight",
                    "mode": invalidMode,
                    "modelPath": str(fixture),
                },
            )
            Require(invalidModeJob is None, f"preflight mode={invalidMode} was accepted")
        missingModeJob = Submit(
            library,
            module,
            {"capability": "geometry.preflight", "modelPath": str(fixture)},
        )
        Require(missingModeJob is None, "preflight without mode was accepted")

        fakeJob = Submit(library, module, {"capability": "geometry.preflight.fast"})
        Require(fakeJob is None, "invented geometry.preflight.fast ID was accepted")

        workerJob = Submit(library, module, {"capability": "slice.rgbwsv"})
        Require(workerJob is None, "Worker-only slice capability was accepted")
        workerError = ReadJson(library.pm_last_error)
        Require("Worker" in workerError["message"], "Worker failure is not explicit")

        fastJob = Submit(
            library,
            module,
            {
                "capability": "geometry.preflight",
                "mode": "fast",
                "modelPath": str(fixture),
            },
        )
        Require(fastJob is not None, "fast preflight was not accepted")
        fastResult = AssertTerminalJob(library, fastJob, "succeeded")
        AssertReleasedJobInvalid(library, fastJob)
        Require(fastResult["ok"] is True, "fast preflight failed")
        for field in ("admission", "issues", "topology", "bboxMm", "outOfBounds"):
            Require(field in fastResult, f"fast preflight omitted {field}")

        importJob = Submit(
            library,
            module,
            {
                "capability": "model.import",
                "modelPath": str(fixture),
                "options": {"computeBBox": True, "extractMaterials": True},
            },
        )
        Require(importJob is not None, "model.import was not accepted")
        imported = AssertTerminalJob(library, importJob, "succeeded")
        Require(imported["ok"] is True, "model.import failed")
        modelId = imported["modelId"]

        metadataJob = Submit(
            library,
            module,
            {"capability": "model.get_metadata", "modelId": modelId},
        )
        Require(metadataJob is not None, "model.get_metadata was not accepted")
        metadata = AssertTerminalJob(library, metadataJob, "succeeded")
        Require(metadata["sourceDigest"] == imported["sourceDigest"], "metadata drifted")

        scene = BuildScene(fixture)
        operationJob = Submit(
            library,
            module,
            {
                "capability": "scene.apply_operation",
                "scene": scene,
                "operationId": "stage14c04-operation",
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
        Require(operationJob is not None, "scene.apply_operation was not routed")
        operation = AssertTerminalJob(library, operationJob, "succeeded")
        Require(operation["ok"] is True, "scene.apply_operation failed")
        sceneRevision = operation["newSceneRevision"]

        snapshotJob = Submit(
            library,
            module,
            {"capability": "scene.get_snapshot", "sceneId": "stage14c04-scene"},
        )
        Require(snapshotJob is not None, "scene.get_snapshot was not routed")
        snapshot = AssertTerminalJob(library, snapshotJob, "succeeded")
        Require(snapshot["sceneRevision"] == sceneRevision, "snapshot revision drifted")

        viewJob = Submit(
            library,
            module,
            {
                "capability": "scene.get_viewdata",
                "sceneId": "stage14c04-scene",
                "expectedSceneRevision": sceneRevision,
                "content": ["bbox", "outline", "surface_preview", "appearance"],
                "viewMode": "top",
                "texturePolicy": "require_if_present",
                "lod": "auto",
                "meshTransform": "local",
                "maxBytes": 4 * 1024 * 1024,
            },
        )
        Require(viewJob is not None, "scene.get_viewdata was not routed")
        viewData = AssertTerminalJob(library, viewJob, "succeeded")
        Require(viewData["viewMode"] == "top", "ViewData mode drifted")
        Require(viewData["coordinateSystem"] == "right_handed_z_up", "coordinate drift")
        preview = viewData["instances"][0]["surfacePreview"]
        Require(preview["pixelFormat"] == "rgba8_unorm", "preview format drifted")
        Require(preview["chunkCount"] > 0, "top surface preview blob is empty")

        blobJob = Submit(
            library,
            module,
            {
                "capability": "scene.get_viewdata",
                "operation": "read_blob",
                "blobId": preview["blobId"],
                "chunkIndex": 0,
            },
        )
        Require(blobJob is not None, "ViewData read_blob was not routed")
        blob = AssertTerminalBinaryJob(library, blobJob)
        Require(len(blob) > 0, "ViewData blob result is empty")

        threeDJob = Submit(
            library,
            module,
            {
                "capability": "scene.get_viewdata",
                "sceneId": "stage14c04-scene",
                "expectedSceneRevision": sceneRevision,
                "content": ["bbox", "outline", "mesh", "appearance"],
                "viewMode": "three_d",
                "texturePolicy": "require_if_present",
                "lod": "lod0",
                "meshTransform": "local",
                "maxBytes": 4 * 1024 * 1024,
            },
        )
        Require(threeDJob is not None, "three_d ViewData was not routed")
        threeD = AssertTerminalJob(library, threeDJob, "succeeded")
        Require(threeD["instances"][0]["mesh"], "three_d mesh is missing")
        Require(
            threeD["appearances"][0]["uvConvention"] == "u_right_v_up",
            "appearance UV convention drifted",
        )

        collisionJob = Submit(
            library,
            module,
            {
                "capability": "geometry.collision",
                "scene": scene,
                "expectedSceneRevision": 1,
            },
        )
        Require(collisionJob is not None, "geometry.collision was not routed")
        collision = AssertTerminalJob(library, collisionJob, "succeeded")
        Require(collision["collisions"] == [], "single model unexpectedly collided")

        releaseJob = Submit(
            library,
            module,
            {"capability": "model.release", "modelId": modelId},
        )
        Require(releaseJob is not None, "model.release was not accepted")
        released = AssertTerminalJob(library, releaseJob, "succeeded")
        Require(released["released"] is True, "model.release failed")

        invalidPackage = str(fixture.parent)
        packageRequests = (
            {"capability": "package.verify", "packageDir": invalidPackage},
            {"capability": "package.get_summary", "packageDir": invalidPackage},
            {
                "capability": "package.get_layer_descriptor",
                "packageDir": invalidPackage,
                "layerIndex": 0,
            },
            {
                "capability": "package.render_layer_preview",
                "packageDir": invalidPackage,
                "layerIndex": 0,
                "mode": "single_channel",
                "channels": ["S"],
                "maxWidthPx": 64,
                "outputPath": str(fixture.parent / "unused.png"),
            },
            {
                "capability": "package.read_report",
                "packageDir": invalidPackage,
                "reportName": "slice_report",
            },
        )
        for packageRequest in packageRequests:
            packageJob = Submit(library, module, packageRequest)
            Require(packageJob is not None, f"{packageRequest['capability']} was not routed")
            packageResult = AssertTerminalJob(library, packageJob, "failed")
            Require(packageResult["ok"] is False, "invalid package did not fail closed")
    finally:
        library.pm_destroy(module)


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--repo", default=Path.cwd(), type=Path)
    arguments = parser.parse_args()
    repo = arguments.repo.resolve()
    fixture = repo / "tests/fixtures/stage14b/model_with_normals.obj"
    Require(arguments.library.is_file(), "slicer module library does not exist")
    Require(fixture.is_file(), "model fixture does not exist")
    ValidateSourceContract(repo)
    ValidateRuntime(ctypes.CDLL(str(arguments.library.resolve())), fixture.resolve())
    print("Stage 14C-04 synchronous capability wiring: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (AssertionError, KeyError, OSError, ValueError) as error:
        print(f"Stage 14C-04 synchronous capability wiring: FAIL: {error}")
        sys.exit(1)
