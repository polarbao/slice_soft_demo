#!/usr/bin/env python3

import copy
import json
import re
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator
from jsonschema.exceptions import ValidationError


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def BuildValidator(path: Path) -> Draft202012Validator:
    schema = LoadJson(path)
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema)


def ExpectValid(validator: Draft202012Validator, value: Any, label: str) -> None:
    errors = list(validator.iter_errors(value))
    if errors:
        raise AssertionError(f"{label}: {errors[0].json_path}: {errors[0].message}")


def ExpectInvalid(validator: Draft202012Validator, value: Any, label: str) -> None:
    try:
        validator.validate(value)
    except ValidationError:
        return
    raise AssertionError(f"{label} should be rejected")


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    contractRoot = repoRoot / "contracts"
    requestValidator = BuildValidator(
        contractRoot / "file_contract_v1.request.schema.json"
    )
    resultValidator = BuildValidator(
        contractRoot / "file_contract_v1.result.schema.json"
    )
    infoValidator = BuildValidator(
        contractRoot / "file_contract_v1.contract_info.schema.json"
    )

    request = {
        "contract": "file_contract",
        "major": 1,
        "minor": 0,
        "jobId": "job-20260805-0001",
        "correlationId": "corr-0001",
        "capability": "slice.rgbwsv",
        "timeoutMs": 1800000,
        "sceneHash": "sha256:9ac3abcd",
        "scene": {},
        "profile": {},
        "output": {
            "contract": "p0.rgbwsv.2",
            "packageDir": "E:/jobs/package"
        }
    }
    ExpectValid(requestValidator, request, "legacy slice request")
    invalidRequest = copy.deepcopy(request)
    invalidRequest["timeoutMs"] = 0
    ExpectInvalid(requestValidator, invalidRequest, "unbounded request")

    transferRequest = copy.deepcopy(request)
    transferRequest["minor"] = 1
    transferRequest["capability"] = "slice.rgbwsvt"
    transferRequest["output"]["contract"] = "p0.rgbwsvt.1"
    ExpectValid(requestValidator, transferRequest, "transfer slice request")

    transferWithLegacyMinor = copy.deepcopy(transferRequest)
    transferWithLegacyMinor["minor"] = 0
    ExpectInvalid(
        requestValidator,
        transferWithLegacyMinor,
        "transfer slice request with legacy minor",
    )

    transferWithLegacyOutput = copy.deepcopy(transferRequest)
    transferWithLegacyOutput["output"]["contract"] = "p0.rgbwsv.2"
    ExpectInvalid(
        requestValidator,
        transferWithLegacyOutput,
        "transfer slice request with legacy output contract",
    )

    legacyWithTransferMinor = copy.deepcopy(request)
    legacyWithTransferMinor["minor"] = 1
    ExpectInvalid(
        requestValidator,
        legacyWithTransferMinor,
        "legacy slice request with transfer minor",
    )

    result = {
        "contract": "file_contract",
        "major": 1,
        "minor": 0,
        "jobId": request["jobId"],
        "correlationId": request["correlationId"],
        "capability": request["capability"],
        "ok": True,
        "code": "PM-SLICER-OK-0000",
        "output": {"packageDir": "E:/jobs/package"},
        "engineVersion": "0.9.3",
        "elapsedMs": 1250.0
    }
    ExpectValid(resultValidator, result, "legacy successful result")
    invalidResult = copy.deepcopy(result)
    invalidResult["jobId"] = "../escape"
    ExpectInvalid(resultValidator, invalidResult, "path-like job id")

    transferResult = copy.deepcopy(result)
    transferResult["minor"] = 1
    transferResult["capability"] = "slice.rgbwsvt"
    ExpectValid(resultValidator, transferResult, "transfer successful result")

    transferResultWithLegacyMinor = copy.deepcopy(transferResult)
    transferResultWithLegacyMinor["minor"] = 0
    ExpectInvalid(
        resultValidator,
        transferResultWithLegacyMinor,
        "transfer result with legacy minor",
    )

    legacyResultWithTransferMinor = copy.deepcopy(result)
    legacyResultWithTransferMinor["minor"] = 1
    ExpectInvalid(
        resultValidator,
        legacyResultWithTransferMinor,
        "legacy result with transfer minor",
    )

    info = {
        "contract": "file_contract",
        "major": 1,
        "minor": 1,
        "engineVersion": "0.9.3",
        "produces": ["p0.rgbwsv.2", "p0.rgbwsvt.1"],
        "capabilities": [
            "slice.rgbwsv",
            "slice.rgbwsvt",
            "geometry.preflight.full",
            "geometry.repair"
        ]
    }
    ExpectValid(infoValidator, info, "contract info")
    missingLegacyProduce = copy.deepcopy(info)
    missingLegacyProduce["produces"] = ["p0.rgbwsvt.1"]
    ExpectInvalid(infoValidator, missingLegacyProduce, "missing legacy package contract")

    missingTransferProduce = copy.deepcopy(info)
    missingTransferProduce["produces"] = ["p0.rgbwsv.2"]
    ExpectInvalid(
        infoValidator,
        missingTransferProduce,
        "missing transfer package contract",
    )

    missingTransferCapability = copy.deepcopy(info)
    missingTransferCapability["capabilities"].remove("slice.rgbwsvt")
    ExpectInvalid(
        infoValidator,
        missingTransferCapability,
        "missing transfer slice capability",
    )

    staleInfo = copy.deepcopy(info)
    staleInfo["minor"] = 0
    ExpectInvalid(infoValidator, staleInfo, "stale discovery minor")

    progressPattern = re.compile(
        r"^SLICE_PROGRESS phase=[A-Za-z0-9_.-]+ current=\d+ total=\d+ "
        r"percent=(?:100|[0-9]{1,2}) elapsedMs=\d+\.\d{3}$"
    )
    canonicalProgress = (
        "SLICE_PROGRESS phase=scene_instance_slice current=2 total=5 "
        "percent=40 elapsedMs=1234.567"
    )
    if progressPattern.fullmatch(canonicalProgress) is None:
        raise AssertionError("canonical progress line should match")
    if progressPattern.fullmatch(canonicalProgress + " garbage") is not None:
        raise AssertionError("malformed reserved progress line should be rejected")

    exitTable = LoadJson(contractRoot / "file_contract_v1.exit_codes.json")
    exitCodes = [entry["exitCode"] for entry in exitTable["exitCodes"]]
    if exitCodes != [0, 1, 2, 3, 4, 5, 6, 7, 8]:
        raise AssertionError("file contract exit-code table drifted")

    print("file_contract_v1 schemas and protocol examples: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
