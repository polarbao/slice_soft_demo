#!/usr/bin/env python3

import json
from pathlib import Path
from typing import Any


EXPECTED_CAPABILITIES = [
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
    "package.verify",
    "package.get_summary",
    "package.get_layer_descriptor",
    "package.render_layer_preview",
    "package.read_report",
]


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def FieldPaths(capability: dict[str, Any], key: str) -> set[str]:
    return {field["path"] for field in capability[key]}


def RequirePaths(
    capability: dict[str, Any], key: str, requiredPaths: set[str]
) -> None:
    missingPaths = requiredPaths - FieldPaths(capability, key)
    if missingPaths:
        raise AssertionError(
            f"{capability['id']} {key} misses fields: {sorted(missingPaths)}"
        )


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    contract = LoadJson(repoRoot / "contracts" / "slicer_capability_dtos.json")
    capabilities = contract["capabilities"]
    capabilityIds = [capability["id"] for capability in capabilities]

    if capabilityIds != EXPECTED_CAPABILITIES:
        raise AssertionError("the ordered 15-capability surface drifted")
    if len(set(capabilityIds)) != 15:
        raise AssertionError("capability IDs must be unique")
    if contract["forbiddenCapabilities"] != ["scene.layout"]:
        raise AssertionError("packing policy must remain outside the module")

    errorCodes = {
        entry["code"]
        for entry in LoadJson(repoRoot / "contracts" / "slicer_error_codes.json")[
            "codes"
        ]
    }
    for capability in capabilities:
        if not capability["requestFields"] or not capability["responseFields"]:
            raise AssertionError(f"{capability['id']} has an empty DTO")
        RequirePaths(capability, "requestFields", {"capability"})
        for field in capability["requestFields"] + capability["responseFields"]:
            if "path" not in field or "type" not in field:
                raise AssertionError(f"{capability['id']} has an incomplete field spec")
        unknownCodes = set(capability["errors"]) - errorCodes
        if unknownCodes:
            raise AssertionError(
                f"{capability['id']} references unknown errors: {sorted(unknownCodes)}"
            )

    byId = {capability["id"]: capability for capability in capabilities}
    RequirePaths(
        byId["scene.apply_operation"],
        "requestFields",
        {"operationId", "currentSceneRevision", "expectedSceneRevision"},
    )
    viewData = byId["scene.get_viewdata"]
    if viewData["operations"] != ["query", "read_blob"]:
        raise AssertionError("viewdata blob retrieval must remain a sub-operation")
    RequirePaths(
        viewData,
        "requestFields",
        {
            "expectedSceneRevision",
            "content",
            "lod",
            "meshTransform",
            "maxBytes",
            "blobId",
            "chunkIndex",
        },
    )
    RequirePaths(
        viewData,
        "responseFields",
        {
            "viewdataIdentity",
            "coordinateSystem",
            "byteOrder",
            "instances[].worldMatrix",
            "instances[].mesh.meshIdentity",
            "instances[].mesh.buffers.position.format",
            "instances[].mesh.buffers.normal.format",
            "instances[].mesh.buffers.index.format",
            "instances[].mesh.blobId",
            "instances[].mesh.chunkBytes",
            "instances[].mesh.chunkCount",
            "truncated",
            "truncationReason",
            "binaryChunk",
        },
    )
    if set(viewData["errors"]) < {
        "PM-SLICER-VIEWDATA-STALE",
        "PM-SLICER-VIEWDATA-BUDGET",
    }:
        raise AssertionError("viewdata errors are incomplete")

    invariants = contract["protocolInvariants"]
    expectedInvariants = {
        "packageSchema": "p0.rgbwsv.2",
        "channels": ["R", "G", "B", "W", "S", "V"],
        "bitDepth": 8,
        "polarity": "black_is_print",
        "printValue": 0,
        "emptyValue": 255,
    }
    for key, value in expectedInvariants.items():
        if invariants[key] != value:
            raise AssertionError(f"protocol invariant drifted: {key}")
    if set(invariants["workerOnly"]) != {"slice.rgbwsv", "geometry.repair"}:
        raise AssertionError("worker-only capability boundary drifted")
    if not invariants["noNewBlobCapability"] or not invariants["noNewAbiExport"]:
        raise AssertionError("viewdata must reuse the frozen ABI surface")

    print("15 capability DTOs and ViewData mesh contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
