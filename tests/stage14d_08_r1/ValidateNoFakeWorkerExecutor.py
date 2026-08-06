#!/usr/bin/env python3
"""Prove the production Worker has no test executor or malformed fallback."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", type=Path, required=True)
    args = parser.parse_args()
    worker = args.worker.resolve()
    if not worker.is_file():
        raise SystemExit(f"worker executable is missing: {worker}")

    binary = worker.read_bytes()
    for token in (b"RecordingExecutor", b"AlwaysSuccessExecutor", b"FakeWorkerCapabilityExecutor"):
        if token in binary:
            raise SystemExit(f"test executor token leaked into production Worker: {token!r}")

    with tempfile.TemporaryDirectory(prefix="slicesoft_14d08_r1_") as directory:
        job = Path(directory)
        request_path = job / "request.json"
        request = {
            "contract": "file_contract",
            "major": 1,
            "minor": 0,
            "jobId": "production-no-executor",
            "correlationId": "production-no-executor-correlation",
            "capability": "geometry.repair",
            "timeoutMs": 5000,
            "input": {"mesh": "fixture.obj"},
        }
        request_path.write_text(json.dumps(request), encoding="utf-8")
        process = subprocess.run(
            [str(worker), "--spi-request", str(request_path.resolve())],
            capture_output=True,
            check=False,
            encoding="utf-8",
            errors="replace",
        )
        if process.returncode != 2:
            raise SystemExit(f"malformed repair request must exit 2, got {process.returncode}")
        if process.stdout:
            raise SystemExit("failed Worker request must not write ordinary stdout")
        if "PM-SLICER-INPUT-0001" not in process.stderr:
            raise SystemExit("malformed repair diagnostic lacks stable input code")
        result_path = job / "result.json"
        if not result_path.is_file() or (job / "result.json.tmp").exists():
            raise SystemExit("identity-closed failure result was not atomically published")
        result = json.loads(result_path.read_text(encoding="utf-8"))
        if result.get("ok") is not False or result.get("code") != "PM-SLICER-INPUT-0001":
            raise SystemExit("production Worker returned a fake success")
        for key in ("jobId", "correlationId", "capability"):
            if result.get(key) != request[key]:
                raise SystemExit(f"result identity mismatch: {key}")
        if any(path.name.startswith("package") for path in job.iterdir()):
            raise SystemExit("R1 Worker created a forbidden production package")

    print("Stage 14D-08 production Worker no-fake/fail-closed boundary: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
