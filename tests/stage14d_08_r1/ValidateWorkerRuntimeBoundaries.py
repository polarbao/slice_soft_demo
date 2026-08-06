#!/usr/bin/env python3
"""Validate the Stage 14D-08-R1 worker-runtime dependency boundary."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo_root.resolve()
    runtime = repo / "apps" / "slicer_worker" / "runtime"
    required = {
        "WorkerJobIdentity.h",
        "WorkerJobIdentity.cpp",
        "WorkerRequestEnvelope.h",
        "WorkerRequestEnvelope.cpp",
        "WorkerRequestParser.h",
        "WorkerRequestParser.cpp",
        "WorkerResultEnvelope.h",
        "WorkerResultEnvelope.cpp",
        "WorkerResultWriter.h",
        "WorkerResultWriter.cpp",
    }
    actual = {path.name for path in runtime.glob("*") if path.is_file()}
    missing = sorted(required - actual)
    if missing:
        raise SystemExit(f"missing worker runtime files: {missing}")

    forbidden_tokens = (
        "ProductionSliceFacade",
        "PreflightFullFacade",
        "RepairFacade",
        "RgbwsvPackageWriter",
        "slicer_module",
        "Qt",
    )
    for path in runtime.glob("Worker*"):
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for token in forbidden_tokens:
            if token in text:
                raise SystemExit(f"forbidden dependency {token!r} in {path}")

    cmake = (repo / "CMakeLists.txt").read_text(encoding="utf-8")
    if "slicer_worker_runtime" not in cmake:
        raise SystemExit("slicer_worker_runtime target is not registered")
    if "stage14d08_r1_worker_runtime_tests" not in cmake:
        raise SystemExit("Stage 14D-08-R1 runtime test target is not registered")

    print("Stage 14D-08-R1 worker runtime boundary: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
