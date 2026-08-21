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


def Main() -> int:
    parser = argparse.ArgumentParser(description="Validate SliceSoft JSON contracts")
    parser.add_argument("--repo-root", type=Path, required=True)
    arguments = parser.parse_args()
    repoRoot = arguments.repo_root.resolve()

    ValidateManifestContracts(repoRoot)
    ValidateSceneContracts(repoRoot)
    ValidateProfileContracts(repoRoot)
    ValidateMaterialVolumeReportContracts(repoRoot)
    print("SliceSoft JSON Schema contracts: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
