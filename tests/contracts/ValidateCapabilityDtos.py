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
    if contract["contractVersion"] != "1.3":
        raise AssertionError("expected the heavy-operation identity contract")
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
    RequirePaths(
        byId["scene.apply_operation"],
        "responseFields",
        {
            "buildVolume.widthMm",
            "buildVolume.heightMm",
            "buildVolume.zLimitMm",
            "warnings",
        },
    )
    RequirePaths(
        byId["scene.get_snapshot"],
        "responseFields",
        {
            "buildVolume.widthMm",
            "buildVolume.heightMm",
            "buildVolume.zLimitMm",
        },
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
            "viewMode",
            "texturePolicy",
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
            "instances[].textureStatus",
            "instances[].appearanceIdentity",
            "instances[].surfacePreview.appearanceIdentity",
            "instances[].surfacePreview.localBoundsMm",
            "instances[].surfacePreview.pixelFormat",
            "instances[].surfacePreview.colorSpace",
            "instances[].surfacePreview.blobId",
            "instances[].mesh.meshIdentity",
            "instances[].mesh.buffers.position.format",
            "instances[].mesh.buffers.normal.format",
            "instances[].mesh.buffers.texcoord0.format",
            "instances[].mesh.buffers.index.format",
            "instances[].mesh.submeshes[].materialId",
            "instances[].mesh.blobId",
            "instances[].mesh.chunkBytes",
            "instances[].mesh.chunkCount",
            "appearances",
            "appearances[].appearanceIdentity",
            "appearances[].materials[].baseColorFactor",
            "appearances[].materials[].baseColorTextureId",
            "appearances[].textures[].textureIdentity",
            "appearances[].textures[].pixelFormat",
            "appearances[].textures[].colorSpace",
            "appearances[].textures[].blobId",
            "truncated",
            "truncationReason",
            "binaryChunk",
        },
    )
    if set(viewData["errors"]) < {
        "PM-SLICER-INPUT-0001",
        "PM-SLICER-INPUT-0002",
        "PM-SLICER-VIEWDATA-STALE",
        "PM-SLICER-VIEWDATA-BUDGET",
    }:
        raise AssertionError("viewdata errors are incomplete")

    preflight = byId["geometry.preflight"]
    RequirePaths(
        preflight,
        "requestFields",
        {
            "scene",
            "sceneHash",
            "expectedSceneRevision",
            "profile",
            "profileHash",
            "targetMode",
            "buildVolume",
        },
    )
    RequirePaths(
        preflight,
        "responseFields",
        {
            "sceneId",
            "sceneRevision",
            "sceneHash",
            "authoritative",
            "complete",
            "cancelled",
            "checkedModelCount",
            "checkedInstanceCount",
            "blockedInstanceCount",
            "skippedInstanceCount",
            "instances",
            "instances[].modelId",
            "instances[].instanceId",
            "instances[].transformHash",
            "instances[].issues",
            "collisions",
            "outOfBoundsInstances",
        },
    )
    preflightRequest = {
        field["path"]: field for field in preflight["requestFields"]
    }
    for path in (
        "scene",
        "sceneHash",
        "expectedSceneRevision",
        "profile",
        "profileHash",
        "targetMode",
    ):
        if preflightRequest[path].get("requiredFor") != "full":
            raise AssertionError(f"full preflight identity is not conditional: {path}")

    repair = byId["geometry.repair"]
    RequirePaths(
        repair,
        "requestFields",
        {
            "modelPath",
            "modelFormat",
            "outputPath",
            "profile",
            "profileHash",
            "sourceResourceScope",
            "repairOutputFormat",
            "policy",
            "requireStrictPass",
        },
    )
    repairRequest = {field["path"]: field for field in repair["requestFields"]}
    for path in ("modelFormat", "repairOutputFormat"):
        if repairRequest[path].get("const") != "obj":
            raise AssertionError(f"repair v1 output format drifted: {path}")
    RequirePaths(
        repair,
        "responseFields",
        {
            "evidence.assetWritten",
            "evidence.assetReimported",
            "evidence.strictComplete",
            "evidence.strictPass",
            "evidence.attributesPreserved",
        },
    )

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
    if invariants["viewModes"] != ["top", "three_d"]:
        raise AssertionError("dual-view modes drifted")
    if not invariants["textureRequiredIfPresent"]:
        raise AssertionError("textured models must remain textured in both views")
    if not invariants["noSilentTextureFallback"]:
        raise AssertionError("texture failures must not silently become flat shading")
    if not invariants["noNewBlobCapability"] or not invariants["noNewAbiExport"]:
        raise AssertionError("viewdata must reuse the frozen ABI surface")

    viewDataRules = contract["viewDataRules"]
    if viewDataRules["top"]["requiredContent"] != [
        "surface_preview",
        "appearance",
    ]:
        raise AssertionError("top view must retain its textured surface preview")
    if not viewDataRules["top"]["outlineOnlyKeepsSurfacePreview"]:
        raise AssertionError("top outline-only LOD must not remove its texture preview")
    if viewDataRules["three_d"]["requiredContent"] != ["mesh", "appearance"]:
        raise AssertionError("3D view must retain mesh and appearance data")
    if viewDataRules["three_d"]["outlineOnlyAllowed"]:
        raise AssertionError("3D textured rendering cannot downgrade to outline-only")
    if viewDataRules["texturedInstanceRequires"] != [
        "texcoord0",
        "submeshes",
        "materials",
        "textures",
    ]:
        raise AssertionError("textured instance requirements drifted")
    expectedAppearanceRules = {
        "collection": "appearances[]",
        "identityField": "appearances[].appearanceIdentity",
        "instanceReference": "instances[].appearanceIdentity",
        "previewReference": "instances[].surfacePreview.appearanceIdentity",
        "materialResolutionScope": "instance_appearance",
    }
    if viewDataRules["multiModelAppearance"] != expectedAppearanceRules:
        raise AssertionError("multi-model appearance references are ambiguous")

    uiViewSpec = LoadJson(repoRoot / "contracts" / "slicer_ui_view_spec.json")
    if uiViewSpec["contractVersion"] != "1.0" or uiViewSpec["units"] != "mm":
        raise AssertionError("UI view contract version or unit drifted")
    uiSettings = uiViewSpec["settings"]
    if uiSettings["allowedViewModes"] != ["top", "three_d"]:
        raise AssertionError("the reference host must expose exactly two view modes")
    if uiSettings["defaultViewMode"] not in uiSettings["allowedViewModes"]:
        raise AssertionError("default view mode must be selectable")
    uiViews = uiViewSpec["viewModes"]
    if uiViews["top"]["projection"] != "orthographic":
        raise AssertionError("top view must remain orthographic")
    if uiViews["top"]["requiredTextureContent"] != [
        "surface_preview",
        "appearance",
    ]:
        raise AssertionError("top view texture contract drifted")
    if set(uiViews["three_d"]["requiredTextureContent"]) != {
        "mesh",
        "texcoord0",
        "submeshes",
        "materials",
        "textures",
    }:
        raise AssertionError("3D view texture contract drifted")
    uiGrid = uiViewSpec["grid"]
    if uiGrid["extentSource"] != "scene.buildVolume":
        raise AssertionError("grid extent must come from the device build volume")
    if uiGrid["coordinateFrame"] != {
        "originSource": "scene.buildVolume.origin",
        "xDirectionSource": "scene.buildVolume.xDirection",
        "yDirectionSource": "scene.buildVolume.yDirection",
        "unresolvedBehavior": (
            "show_product_fallback_with_unresolved_diagnostic_not_production_truth"
        ),
    }:
        raise AssertionError("grid coordinate frame or unresolved behavior drifted")
    if uiGrid["minorSpacingMm"] != 1.0 or uiGrid["majorSpacingMm"] != 10.0:
        raise AssertionError("the 1 mm / 10 mm grid specification drifted")
    texturePresentation = uiViewSpec["texturePresentation"]
    if not texturePresentation["requiredIfModelDeclaresTexture"]:
        raise AssertionError("both views must retain declared model textures")
    if texturePresentation["silentGrayFallbackAllowed"]:
        raise AssertionError("texture failures cannot silently become gray models")
    if texturePresentation["displayAssistsModifyTexturePixels"]:
        raise AssertionError("white-texture assists cannot rewrite texture pixels")
    if texturePresentation["displayAssistsModifyProductionOutput"]:
        raise AssertionError("UI display assists cannot change production output")
    switchInvariants = uiViewSpec["switchInvariants"]
    if switchInvariants["moduleCallsDuringSwitch"] != 0:
        raise AssertionError("view switching must remain a host-local operation")
    for key in (
        "preserveSceneRevision",
        "preserveSelection",
        "preserveInstanceTransforms",
        "preserveJobState",
        "reuseMeshAndTextureCaches",
    ):
        if not switchInvariants[key]:
            raise AssertionError(f"view switch invariant drifted: {key}")

    print("15 capability DTOs plus heavy-operation identity contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
