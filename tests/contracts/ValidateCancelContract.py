#!/usr/bin/env python3

import copy
import json
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator
from jsonschema.exceptions import ValidationError


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def ExpectInvalid(validator: Draft202012Validator, value: Any, label: str) -> None:
    try:
        validator.validate(value)
    except ValidationError:
        return
    raise AssertionError(f"{label} should be rejected")


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    contracts = repoRoot / "contracts"
    contract = LoadJson(contracts / "slicer_cancel_contract.json")

    if contract["maxCancelLatencyMs"] != 2000:
        raise AssertionError("cancel latency contract drifted")
    if contract["states"]["terminal"] != ["succeeded", "failed", "cancelled"]:
        raise AssertionError("terminal state set drifted")
    if contract["cancelCall"]["stateBeforeReturn"] != "cancelling":
        raise AssertionError("pm_cancel must expose Cancelling before return")
    if not contract["completion"]["workerExitRequired"]:
        raise AssertionError("Cancelled must wait for real Worker exit")
    if not contract["completion"]["stagingRemovedRequired"]:
        raise AssertionError("Cancelled must wait for staging cleanup")
    if contract["workerProtocol"]["escalation"] != "terminate_windows_job_object":
        raise AssertionError("Worker process-tree fallback drifted")
    forbidden = set(contract["cleanup"]["forbidden"])
    if not {
        "report_cancelled_before_worker_exit",
        "report_cancelled_with_staging_residue",
        "recursive_delete_outside_package_parent",
    } <= forbidden:
        raise AssertionError("cancellation safety boundaries are incomplete")

    resultSchema = LoadJson(contracts / "file_contract_v1.result.schema.json")
    Draft202012Validator.check_schema(resultSchema)
    validator = Draft202012Validator(resultSchema)
    cancelledResult = {
        "contract": "file_contract",
        "major": 1,
        "minor": 0,
        "jobId": "job-cancel-001",
        "correlationId": "corr-cancel-001",
        "capability": "slice.rgbwsv",
        "ok": False,
        "code": "PM-SLICER-CANCELLED-0070",
        "error": {"message": "cancelled"},
        "engineVersion": "0.9.3",
        "elapsedMs": 125.0,
        "cleanup": {"stagingRemoved": True, "published": False},
    }
    validator.validate(cancelledResult)

    residueResult = copy.deepcopy(cancelledResult)
    residueResult["cleanup"]["stagingRemoved"] = False
    ExpectInvalid(validator, residueResult, "cancelled result with staging residue")
    publishedResult = copy.deepcopy(cancelledResult)
    publishedResult["cleanup"]["published"] = True
    ExpectInvalid(validator, publishedResult, "cancelled result that published output")

    print("Cancelling/Cancelled and staging cleanup contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
