#!/usr/bin/env python3
"""Create truthful Stage 14D-07 evidence for a parameterized Worker binary."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import pathlib
import subprocess
import sys
from typing import Any


def LoadJson(path: pathlib.Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def WriteJson(path: pathlib.Path, document: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporaryPath = path.with_suffix(path.suffix + ".tmp")
    with temporaryPath.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, indent=2, sort_keys=True)
        stream.write("\n")
    temporaryPath.replace(path)


def QueryWorker(workerPath: pathlib.Path) -> dict[str, Any]:
    completed = subprocess.run(
        [str(workerPath), "--contract-info"],
        check=False,
        capture_output=True,
        encoding="utf-8",
        timeout=5,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"worker contract-info failed with exit {completed.returncode}")
    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError("worker contract-info must emit exactly one JSON line")
    return json.loads(lines[0])


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path, required=True)
    parser.add_argument("--worker", type=pathlib.Path, required=True)
    parser.add_argument("--gate", type=pathlib.Path)
    parser.add_argument("--evidence-root", type=pathlib.Path, required=True)
    arguments = parser.parse_args()

    repoRoot = arguments.repo_root.resolve()
    workerPath = arguments.worker.resolve()
    if not workerPath.is_file():
        raise ValueError("worker path must identify an existing file")
    gatePath = arguments.gate
    if gatePath is None:
        gatePath = workerPath.parent / "stage14d07_engine_conformance_gate.exe"
    gatePath = gatePath.resolve()
    if not gatePath.is_file():
        raise ValueError("conformance gate path must identify an existing file")

    contract = LoadJson(repoRoot / "contracts" / "slicer_engine_conformance_v1.json")
    workerInfo = QueryWorker(workerPath)
    supported = set(workerInfo.get("capabilities", []))
    required = {"slice.rgbwsv", "geometry.preflight.full", "geometry.repair"}
    discoveryPassed = (
        workerInfo.get("contract") == "file_contract"
        and workerInfo.get("major") == 1
        and "p0.rgbwsv.2" in set(workerInfo.get("produces", []))
        and required.issubset(supported)
    )

    evidenceRoot = arguments.evidence_root.resolve()
    if not discoveryPassed:
        raise RuntimeError("worker contract-info does not satisfy the frozen baseline")
    completed = subprocess.run(
        [str(gatePath), str(workerPath), str(repoRoot), str(evidenceRoot)],
        check=False,
        timeout=120,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"engine conformance gate failed with exit {completed.returncode}"
        )

    evidenceFiles = {
        "E-01": "e01_contract_identity.json",
        "E-02": "e02_package_protocol.json",
        "E-03": "e03_golden.json",
        "E-04": "e04_reports.json",
        "E-05": "e05_progress_timing.json",
        "E-06": "e06_negative.json",
        "E-07": "e07_cancel_recovery.json",
        "E-08": "e08_replaceability.json",
    }
    themeResults = []
    for theme in contract["themes"]:
        evidencePath = evidenceRoot / evidenceFiles[theme["id"]]
        evidence = LoadJson(evidencePath)
        if evidence.get("theme") != theme["id"] or evidence.get("status") != "pass":
            raise RuntimeError(f"{theme['id']} evidence is not PASS")
        themeResults.append({
            "id": theme["id"],
            "status": "pass",
            "evidence": evidencePath.name,
        })
    runIdentity = {
        "schema": "slicesoft.engine_conformance.run_identity.14d_07.1",
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "workerPath": str(workerPath),
        "gatePath": str(gatePath),
        "workerInfo": workerInfo,
        "discoveryStatus": "pass" if discoveryPassed else "fail",
        "contractVersion": contract["version"],
    }
    summary = {
        "schema": "slicesoft.engine_conformance.summary.14d_07.1",
        "overall": "pass",
        "themes": themeResults,
        "rule": contract["overallRule"],
    }
    WriteJson(evidenceRoot / "run_identity.json", runIdentity)
    WriteJson(evidenceRoot / "summary.json", summary)
    print(f"Stage 14D-07 runner: PASS evidence written to {evidenceRoot}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError, subprocess.TimeoutExpired) as error:
        print(f"Stage 14D-07 runner: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
