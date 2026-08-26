#!/usr/bin/env python3

import argparse
import copy
import json
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator
from jsonschema.exceptions import ValidationError


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def ValidatePositive(
    validator: Draft202012Validator, instance: Any, label: str
) -> None:
    errors = sorted(validator.iter_errors(instance), key=lambda error: list(error.path))
    if errors:
        details = "\n".join(f"  - {error.json_path}: {error.message}" for error in errors)
        raise AssertionError(f"{label} should validate:\n{details}")


def ValidateNegative(
    validator: Draft202012Validator, instance: Any, label: str
) -> None:
    try:
        validator.validate(instance)
    except ValidationError:
        return
    raise AssertionError(f"{label} should be rejected")


def BuildValidator(schemaPath: Path) -> Draft202012Validator:
    schema = LoadJson(schemaPath)
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema)


def ValidateManifestContracts(repoRoot: Path) -> None:
    validator = BuildValidator(repoRoot / "contracts" / "p0.rgbwsv.2.schema.json")
    legacyManifest = LoadJson(
        repoRoot / "tests" / "packages" / "bad" / "bad_missing_layer" / "manifest.json"
    )
    ValidatePositive(validator, legacyManifest, "legacy p0.rgbwsv.2 manifest")

    realManifestPath = repoRoot / "output" / "UiSmokeOverlayRgbwv" / "manifest.json"
    if realManifestPath.exists():
        ValidatePositive(
            validator,
            LoadJson(realManifestPath),
            "real UI smoke p0.rgbwsv.2 manifest",
        )

    manifestWithWhiteSemantics = copy.deepcopy(legacyManifest)
    manifestWithWhiteSemantics["whiteSemantics"] = "opaque"
    ValidatePositive(
        validator,
        manifestWithWhiteSemantics,
        "manifest with optional whiteSemantics",
    )

    invalidOrder = copy.deepcopy(legacyManifest)
    invalidOrder["tiff"]["channelOrder"] = ["R", "G", "B", "W", "V", "S"]
    ValidateNegative(validator, invalidOrder, "manifest with invalid channel order")

    invalidWhiteSemantics = copy.deepcopy(legacyManifest)
    invalidWhiteSemantics["whiteSemantics"] = "sentinel"
    ValidateNegative(
        validator,
        invalidWhiteSemantics,
        "manifest with invalid whiteSemantics",
    )

    transferValidator = BuildValidator(
        repoRoot / "contracts" / "p0.rgbwsvt.1.schema.json"
    )
    transferManifest = copy.deepcopy(legacyManifest)
    transferManifest["schema"] = "p0.rgbwsvt.1"
    transferManifest["schemaVersion"] = "p0.rgbwsvt.1"
    transferManifest["productionAcceptance"] = "rgbwsvt_candidate_unvalidated"
    transferManifest["tiff"]["channelCount"] = 7
    transferManifest["tiff"]["channelOrder"] = ["R", "G", "B", "W", "S", "V", "T"]
    emptyChannelStats = {
        "printPixels": 0,
        "fullPrintPixels": 0,
        "partialPrintPixels": 0,
        "emptyPixels": 1,
        "minValue": 255,
        "maxValue": 255,
    }
    transferManifest["tiff"]["channelStats"] = {
        channel: copy.deepcopy(emptyChannelStats)
        for channel in ["R", "G", "B", "W", "S", "V", "T"]
    }
    transferManifest["tiff"]["statisticsSource"] = "persisted_tiff_bytes"
    ValidatePositive(
        transferValidator,
        transferManifest,
        "p0.rgbwsvt.1 manifest",
    )

    transferWithoutAcceptance = copy.deepcopy(transferManifest)
    del transferWithoutAcceptance["productionAcceptance"]
    ValidateNegative(
        transferValidator,
        transferWithoutAcceptance,
        "RGBWSVT manifest without production acceptance",
    )

    transferUnknownAcceptance = copy.deepcopy(transferManifest)
    transferUnknownAcceptance["productionAcceptance"] = "unknown"
    ValidateNegative(
        transferValidator,
        transferUnknownAcceptance,
        "RGBWSVT manifest with unknown production acceptance",
    )

    transferWithoutT = copy.deepcopy(transferManifest)
    transferWithoutT["tiff"]["channelOrder"] = ["R", "G", "B", "W", "S", "V"]
    ValidateNegative(
        transferValidator,
        transferWithoutT,
        "RGBWSVT manifest without T",
    )

    transferWithoutStats = copy.deepcopy(transferManifest)
    del transferWithoutStats["tiff"]["channelStats"]
    ValidateNegative(
        transferValidator,
        transferWithoutStats,
        "RGBWSVT manifest without persisted channel statistics",
    )


def ValidateSceneContracts(repoRoot: Path) -> None:
    validator = BuildValidator(
        repoRoot / "contracts" / "slicesoft.multimodel_scene.13b.1.schema.json"
    )
    legacyScene = LoadJson(
        repoRoot / "samples" / "configs" / "scene" / "fixture_two_model_scene.json"
    )
    ValidatePositive(validator, legacyScene, "legacy multi-model scene")

    sceneWithZLimit = copy.deepcopy(legacyScene)
    sceneWithZLimit["buildVolume"]["zLimitMm"] = 60.0
    ValidatePositive(
        validator,
        sceneWithZLimit,
        "multi-model scene with optional zLimitMm",
    )

    invalidZLimit = copy.deepcopy(legacyScene)
    invalidZLimit["buildVolume"]["zLimitMm"] = 0.0
    ValidateNegative(validator, invalidZLimit, "scene with invalid zLimitMm")


def ValidateProfileContracts(repoRoot: Path) -> None:
    validator = BuildValidator(
        repoRoot / "contracts" / "slicesoft.slice_profile.schema.json"
    )
    legacyProfile = LoadJson(
        repoRoot
        / "samples"
        / "configs"
        / "material_process"
        / "obj_mtl_texture_rgb_only.json"
    )
    whiteOnDemandProfile = LoadJson(
        repoRoot
        / "samples"
        / "configs"
        / "material_process"
        / "obj_mtl_texture_rgb_white_ondemand.json"
    )
    ValidatePositive(validator, legacyProfile, "legacy profile without Stage 15 fields")
    ValidatePositive(
        validator,
        whiteOnDemandProfile,
        "Stage 15 white-on-demand profile",
    )

    invalidProfile = copy.deepcopy(whiteOnDemandProfile)
    invalidProfile["texture"]["unprintableWhiteValue"] = 255
    ValidateNegative(
        validator,
        invalidProfile,
        "white-underbase profile with empty W value",
    )


def ValidateMaterialVolumeReportContracts(repoRoot: Path) -> None:
    validator = BuildValidator(
        repoRoot / "contracts" / "slicesoft.material_volume_report.1.schema.json"
    )
    report = LoadJson(
        repoRoot / "tests" / "matvol" / "fixtures" / "material_volume_report_minimal.json"
    )
    ValidatePositive(validator, report, "minimal material volume report")

    transferProtocolReport = copy.deepcopy(report)
    transferProtocolReport["packageProtocol"] = "p0.rgbwsvt.1"
    ValidatePositive(
        validator,
        transferProtocolReport,
        "RGBWSVT material volume report",
    )

    surfaceBand = copy.deepcopy(report)
    surfaceBand["openSurface"]["mode"] = "surface_band"
    surfaceBand["openSurface"]["requestedThicknessMm"] = 0.2
    ValidatePositive(validator, surfaceBand, "material volume report with surface band")

    unknownTopology = copy.deepcopy(report)
    unknownTopology["materials"][0]["topology"] = "mystery"
    ValidateNegative(validator, unknownTopology, "report with unknown topology kind")

    negativeThickness = copy.deepcopy(report)
    negativeThickness["openSurface"]["requestedThicknessMm"] = -1.0
    ValidateNegative(validator, negativeThickness, "report with negative requested thickness")

    unknownField = copy.deepcopy(report)
    unknownField["unexpectedField"] = 1
    ValidateNegative(validator, unknownField, "report with an unexpected top level field")

    badChannel = copy.deepcopy(report)
    badChannel["materials"][0]["rgb"] = [63, 190, 256]
    ValidateNegative(validator, badChannel, "report with an out-of-range RGB channel")


def ValidateTransferChannelReportContracts(repoRoot: Path) -> None:
    validator = BuildValidator(
        repoRoot / "contracts" / "slicesoft.transfer_channel_report.1.schema.json"
    )
    channelStats = {
        "printPixels": 0,
        "fullPrintPixels": 0,
        "partialPrintPixels": 0,
        "emptyPixels": 1,
        "minValue": 255,
        "maxValue": 255,
    }
    report = {
        "schema": "slicesoft.transfer_channel_report.1",
        "packageProtocol": "p0.rgbwsvt.1",
        "statisticsSource": "persisted_tiff_bytes",
        "enabled": True,
        "matchSource": "material_diffuse_rgb",
        "configuredMaterialDiffuseRgbValues": [[255, 220, 198]],
        "regionPresent": False,
        "materialName": "",
        "matchedDiffuseRgb": [0, 0, 0],
        "value": 0,
        "channelStats": {
            channel: copy.deepcopy(channelStats)
            for channel in ["R", "G", "B", "W", "S", "V", "T"]
        },
        "totals": {
            "layerCount": 1,
            "transferPrintPixels": 0,
            "unexpectedOverlapPixels": 0,
        },
        "layers": [{"layerIndex": 0, "transferPrintPixels": 0}],
        "warnings": [],
        "errors": [],
    }
    ValidatePositive(validator, report, "RGBWSVT transfer channel report")
    invalid = copy.deepcopy(report)
    invalid["totals"]["unexpectedOverlapPixels"] = 1
    ValidateNegative(validator, invalid, "overlapping transfer channel report")


def Main() -> int:
    parser = argparse.ArgumentParser(description="Validate SliceSoft JSON contracts")
    parser.add_argument("--repo-root", type=Path, required=True)
    arguments = parser.parse_args()
    repoRoot = arguments.repo_root.resolve()

    ValidateManifestContracts(repoRoot)
    ValidateSceneContracts(repoRoot)
    ValidateProfileContracts(repoRoot)
    ValidateMaterialVolumeReportContracts(repoRoot)
    ValidateTransferChannelReportContracts(repoRoot)
    print("SliceSoft JSON Schema contracts: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
