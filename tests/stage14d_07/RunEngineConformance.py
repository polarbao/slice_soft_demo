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
    parser.add_argument("--evidence-root", type=pathlib.Path, required=True)
    arguments = parser.parse_args()

    repoRoot = arguments.repo_root.resolve()
    workerPath = arguments.worker.resolve()
    if not workerPath.is_file():
        raise ValueError("worker path must identify an existing file")

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

    themeResults = []
    for theme in contract["themes"]:
        blockers = list(theme["blockedBy"])
        if not discoveryPassed:
            blockers.insert(0, "worker contract-info mismatch")
        themeResults.append({
            "id": theme["id"],
            "status": "blocked",
            "blockers": blockers,
            "note": "14D-07-R1 builds the harness; full execution belongs to R2",
        })

    evidenceRoot = arguments.evidence_root.resolve()
    runIdentity = {
        "schema": "slicesoft.engine_conformance.run_identity.14d_07.1",
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "workerPath": str(workerPath),
        "workerInfo": workerInfo,
        "discoveryStatus": "pass" if discoveryPassed else "fail",
        "contractVersion": contract["version"],
    }
    summary = {
        "schema": "slicesoft.engine_conformance.summary.14d_07.1",
        "overall": "blocked",
        "themes": themeResults,
        "rule": contract["overallRule"],
    }
    WriteJson(evidenceRoot / "run_identity.json", runIdentity)
    WriteJson(evidenceRoot / "summary.json", summary)
    print(f"Stage 14D-07 runner: BLOCKED evidence written to {evidenceRoot}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(Main())
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError, subprocess.TimeoutExpired) as error:
        print(f"Stage 14D-07 runner: FAIL: {error}", file=sys.stderr)
        sys.exit(1)
