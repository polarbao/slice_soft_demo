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


def FieldSpec(
    capability: dict[str, Any], key: str, path: str
) -> dict[str, Any]:
    matches = [field for field in capability[key] if field["path"] == path]
    if len(matches) != 1:
        raise AssertionError(
            f"{capability['id']} {key} expected one {path}, found {len(matches)}"
        )
    return matches[0]


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    contract = LoadJson(repoRoot / "contracts" / "slicer_capability_dtos.json")
    if contract["contractVersion"] != "1.13":
        raise AssertionError("expected the XYZ translation contract")
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
    for modelCapability in ("model.import", "model.get_metadata"):
        RequirePaths(
            byId[modelCapability],
            "responseFields",
            {
                "appearanceStatus",
                "singleMaterialOnly",
                "appearanceDetail",
            },
        )
    RequirePaths(
        byId["scene.apply_operation"],
        "requestFields",
        {
            "operationId",
            "sceneContext",
            "sceneContext.resolvedProfileId",
            "sceneContext.buildVolume",
            "sceneContext.buildVolume.source",
            "sceneContext.buildVolume.widthMm",
            "sceneContext.buildVolume.heightMm",
            "sceneContext.buildVolume.zLimitMm",
            "sceneContext.buildVolume.origin",
            "sceneContext.buildVolume.xDirection",
            "sceneContext.buildVolume.yDirection",
            "sceneContext.buildVolume.isFixture",
            "currentSceneRevision",
            "expectedSceneRevision",
            "operations[].modelId",
            "operations[].assignInstanceId",
            "operations[].initialTransform",
            "operations[].initialTransform.translateXMm",
            "operations[].initialTransform.translateYMm",
            "operations[].initialTransform.translateZMm",
            "operations[].initialTransform.rotateXDeg",
            "operations[].initialTransform.rotateYDeg",
            "operations[].initialTransform.rotateZDeg",
            "operations[].initialTransform.uniformScale",
            "operations[].initialTransform.mirrorX",
            "operations[].initialTransform.mirrorY",
            "operations[].initialTransform.landOnBuildPlate",
            "operations[].layout",
            "operations[].layout.policy",
            "operations[].layout.maxColumns",
            "operations[].layout.maxRows",
            "operations[].layout.columnGapMm",
            "operations[].layout.rowGapMm",
            "operations[].layout.spacingMode",
            "operations[].layout.order",
        },
    )
    RequirePaths(
        byId["scene.apply_operation"],
        "responseFields",
        {
            "sceneHandle",
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
            "meshAttributeFormat",
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
            "instances[].meshIdentity",
            "instances[].mesh.meshIdentity",
            "instances[].mesh.buffers.position.format",
            "instances[].mesh.buffers.normal.format",
            "instances[].mesh.buffers.texcoord0.format",
            "instances[].mesh.buffers.index.format",
            "instances[].mesh.submeshes[].materialId",
            "instances[].mesh.blobId",
            "instances[].mesh.chunkBytes",
            "instances[].mesh.chunkCount",
            "meshes",
            "meshes[].meshIdentity",
            "meshes[].lod",
            "meshes[].buffers.position.format",
            "meshes[].buffers.normal.format",
            "meshes[].buffers.texcoord0.format",
            "meshes[].buffers.index.format",
            "meshes[].submeshes[].materialId",
            "meshes[].blobId",
            "meshes[].chunkBytes",
            "meshes[].chunkCount",
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

    meshAttributeFormat = FieldSpec(
        viewData, "requestFields", "meshAttributeFormat"
    )
    if meshAttributeFormat != {
        "path": "meshAttributeFormat",
        "type": "enum:float32|float16",
        "required": False,
        "default": "float32",
    }:
        raise AssertionError("mesh attribute format compatibility drifted")
    expectedFormats = {
        "instances[].mesh.buffers.position.format": "enum:float32x3|float16x3",
        "instances[].mesh.buffers.normal.format": "enum:float32x3|float16x3",
        "instances[].mesh.buffers.texcoord0.format": "enum:float32x2|float16x2",
        "meshes[].buffers.position.format": "enum:float32x3|float16x3",
        "meshes[].buffers.normal.format": "enum:float32x3|float16x3",
        "meshes[].buffers.texcoord0.format": "enum:float32x2|float16x2",
    }
    for path, expectedType in expectedFormats.items():
        if FieldSpec(viewData, "responseFields", path).get("type") != expectedType:
            raise AssertionError(f"mesh attribute response format drifted: {path}")
    encoding = contract["viewDataRules"]["meshAttributeEncoding"]
    if encoding != {
        "requestField": "meshAttributeFormat",
        "supported": ["float32", "float16"],
        "defaultWhenAbsent": "float32",
        "float16Components": ["position", "normal", "texcoord0"],
        "float16Quantizer": "meshopt_quantizeHalf",
        "budgetUsesSerializedWireBytes": True,
        "meshIdentityIncludesEncoding": True,
    }:
        raise AssertionError("mesh attribute encoding rules drifted")

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
    sceneOperation = byId["scene.apply_operation"]
    operationType = FieldSpec(
        sceneOperation, "requestFields", "operations[].type"
    )
    expectedOperationType = (
        "enum:addInstance|removeInstance|applyGridLayout|translate|rotateX|rotateY|rotateZ|uniformScale|mirror|landOnBuildPlate"
    )
    if operationType != {
        "path": "operations[].type",
        "type": expectedOperationType,
        "required": True,
    }:
        raise AssertionError("scene operation type enum drifted")

    instanceId = FieldSpec(
        sceneOperation, "requestFields", "operations[].instanceId"
    )
    if instanceId != {
        "path": "operations[].instanceId",
        "type": "string",
        "requiredFor": "removeInstance|translate|rotateX|rotateY|rotateZ|uniformScale|mirror|landOnBuildPlate",
    }:
        raise AssertionError("instanceId conditional requirement drifted")
    modelId = FieldSpec(sceneOperation, "requestFields", "operations[].modelId")
    if modelId.get("requiredFor") != "addInstance":
        raise AssertionError("addInstance must require modelId")
    responseSceneHandle = FieldSpec(
        sceneOperation, "responseFields", "sceneHandle"
    )
    if responseSceneHandle.get("requiredFor") != "implicit_scene_create":
        raise AssertionError("implicit scene creation must return sceneHandle")

    legacyFields = {
        "operations[].deltaMm": ("number[3]", False),
        "operations[].degrees": ("number", False),
        "operations[].factor": ("number", False),
        "operations[].axis": ("enum:x|y", False),
    }
    for path, expected in legacyFields.items():
        field = FieldSpec(sceneOperation, "requestFields", path)
        if (field["type"], field["required"]) != expected:
            raise AssertionError(f"legacy scene operation field drifted: {path}")

    operationRules = contract["sceneOperationRules"]
    if operationRules["applicationOrder"] != "request_order":
        raise AssertionError("scene operations must preserve request order")
    if operationRules["atomic"] is not True:
        raise AssertionError("scene operations must be atomic")
    implicitCreation = operationRules["implicitSceneCreation"]
    expectedCreationRequirements = [
        "sceneHandle_absent",
        "scene_absent_or_empty_object",
        "operations_contains_addInstance",
        "sceneContext_valid",
        "currentSceneRevision_equals_0",
        "expectedSceneRevision_equals_0",
    ]
    if implicitCreation["requires"] != expectedCreationRequirements:
        raise AssertionError("implicit scene creation conditions drifted")
    if implicitCreation["responseRequires"] != ["sceneHandle"]:
        raise AssertionError("implicit scene creation response drifted")
    if (
        implicitCreation["legacyOperationOnlyRequestWithoutScene"]
        != "preserve_previous_failure"
        or implicitCreation["nonEmptyInlineScene"] != "preserve_v1.4_behavior"
    ):
        raise AssertionError("legacy scene request compatibility drifted")
    sceneContext = operationRules["sceneContext"]
    if sceneContext != {
        "requiredOnlyFor": "implicit_scene_create",
        "forbiddenWith": ["sceneHandle", "non_empty_inline_scene"],
        "profileSource": "host_authority",
        "buildVolumeSource": "device_profile",
        "fixtureBuildVolumeAllowed": False,
        "includedInOperationFingerprint": True,
    }:
        raise AssertionError("implicit sceneContext authority rules drifted")

    requirements = operationRules["operationRequirements"]
    if requirements["addInstance"]["required"] != ["modelId"]:
        raise AssertionError("addInstance required fields drifted")
    if requirements["addInstance"]["forbidden"] != ["instanceId"]:
        raise AssertionError("addInstance instance identity source is ambiguous")
    if requirements["removeInstance"]["required"] != ["instanceId"]:
        raise AssertionError("removeInstance required fields drifted")
    if requirements["removeInstance"]["modelRelease"] != "not_implied":
        raise AssertionError("removeInstance must not release the model")
    for rotation in ("rotateX", "rotateY"):
        if requirements[rotation] != {"required": ["instanceId", "degrees"]}:
            raise AssertionError(f"{rotation} contract drifted")
    if requirements["translate"] != {
        "required": ["instanceId", "deltaMm"],
        "axes": "xyz",
        "nonZeroZDisablesPersistentLanding": True,
    }:
        raise AssertionError("XYZ translation contract drifted")
    if requirements["landOnBuildPlate"] != {
        "required": ["instanceId"],
        "targetPlane": "z_equals_zero",
        "persistentForSubsequentTransforms": True,
        "resetsTranslateZMm": True,
        "reference": "significant_connected_components",
    }:
        raise AssertionError("explicit build-plate landing contract drifted")
    gridLayout = requirements["applyGridLayout"]
    if gridLayout["batchMode"] != "sole_operation":
        raise AssertionError("applyGridLayout must remain an atomic sole operation")
    if gridLayout["capacityLimit"] != 22:
        raise AssertionError("applyGridLayout capacity drifted")
    if gridLayout["instanceOrder"] != "scene_order":
        raise AssertionError("applyGridLayout instance order drifted")
    if not gridLayout["hiddenInstancesOccupyCells"]:
        raise AssertionError("hidden instances must occupy grid cells")
    if not gridLayout["lockedInstancesRemainInPlace"]:
        raise AssertionError("locked instances must retain their placement")
    sessionLifetime = operationRules["sessionLifetime"]
    if sessionLifetime != {
        "scope": "pm_module_t",
        "releasedBy": "pm_destroy",
        "pm_releaseClosesScene": False,
        "explicitPerSceneClose": False,
    }:
        raise AssertionError("scene session lifetime drifted")
    compatibility = operationRules["backwardCompatibility"]
    if compatibility["legacyOperationTypes"] != [
        "translate",
        "rotateZ",
        "uniformScale",
        "mirror",
    ]:
        raise AssertionError("legacy scene operation order drifted")
    if compatibility["addedOperationTypes"] != [
        "rotateX",
        "rotateY",
        "landOnBuildPlate",
    ]:
        raise AssertionError("XYZ transform extension drifted")
    if compatibility["legacyRequestResponseSemantics"] != "unchanged":
        raise AssertionError("legacy request compatibility must remain explicit")
    if (
        compatibility["standaloneSceneLayoutCapability"]
        != "forbidden_use_scene.apply_operation"
    ):
        raise AssertionError("standalone scene.layout must remain forbidden")
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

    sliceCapability = byId["slice.rgbwsv"]
    sliceRequest = {
        field["path"]: field for field in sliceCapability["requestFields"]
    }
    backend = sliceRequest["options.backend"]
    if backend.get("type") != "enum:worker" or backend.get("default") != "worker":
        raise AssertionError("slice backend must default to the sole Worker carrier")
    if not backend.get("caseSensitive"):
        raise AssertionError("slice backend matching must remain case-sensitive")
    if set(backend.get("rejectedValues", [])) != {"inprocess", "auto"}:
        raise AssertionError("legacy backend aliases must fail closed")
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
    if invariants["sliceBackend"] != {
        "field": "options.backend",
        "default": "worker",
        "allowed": ["worker"],
        "noInProcessFallback": True,
    }:
        raise AssertionError("slice Worker-only backend invariant drifted")
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
    expectedMeshRules = {
        "collection": "meshes[]",
        "identityField": "meshes[].meshIdentity",
        "instanceReference": "instances[].meshIdentity",
        "canonicalPlacement": "top_level",
        "legacyCompatibilityAlias": "instances[].mesh",
        "localReuse": "one_mesh_per_model_content_and_lod",
        "worldReuse": "by_mesh_identity_only",
        "blobStorage": "once_per_mesh_identity",
    }
    if viewDataRules["multiInstanceMesh"] != expectedMeshRules:
        raise AssertionError("multi-instance mesh reuse rules are ambiguous")
    expectedTruncationReasons = {
        "separator": ";",
        "meshSimplified": [
            "mesh_simplified_lod1_for_max_bytes",
            "mesh_simplified_lod2_for_max_bytes",
        ],
        "meshDecimatedLegacy": [
            "mesh_decimated_lod1_for_max_bytes",
            "mesh_decimated_lod2_for_max_bytes",
        ],
        "currentProviderEmitsMeshDecimated": False,
        "textureResolutionReduced": "texture_resolution_reduced_for_max_bytes",
        "topPreviewResolutionReduced": (
            "top_preview_resolution_reduced_for_max_bytes"
        ),
    }
    if viewDataRules["truncationReasons"] != expectedTruncationReasons:
        raise AssertionError("ViewData truncation reason taxonomy drifted")

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

    print("15 capability DTOs plus XYZ translation v1.13 contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
