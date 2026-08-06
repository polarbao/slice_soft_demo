#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path


def ParseArguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", required=True)
    return parser.parse_args()


def Run(worker: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(worker), *arguments],
        capture_output=True,
        check=False,
        encoding="utf-8",
        errors="replace",
    )


def RequireStableResult(
    worker: Path,
    expectedCode: int,
    expectedText: str,
    *arguments: str,
) -> None:
    first = Run(worker, *arguments)
    second = Run(worker, *arguments)
    assert first.returncode == expectedCode
    assert second.returncode == expectedCode
    assert expectedText in first.stdout + first.stderr
    assert first.stdout == second.stdout
    assert first.stderr == second.stderr


def Validate() -> None:
    arguments = ParseArguments()
    worker = Path(arguments.worker).resolve()
    assert worker.is_file(), f"worker executable is missing: {worker}"

    RequireStableResult(worker, 0, "file-contract shell", "--help")
    RequireStableResult(worker, 2, "invalid_arguments")
    RequireStableResult(worker, 2, "unknown argument", "--unknown")
    RequireStableResult(worker, 0, '"contract":"file_contract"', "--contract-info")
    RequireStableResult(worker, 2, "requires one absolute", "--spi-request")
    RequireStableResult(
        worker,
        2,
        "requires an absolute",
        "--spi-request",
        "relative-request.json",
    )
    RequireStableResult(
        worker,
        2,
        "PM-SLICER-INPUT-0002",
        "--spi-request",
        str((worker.parent / "request.json").resolve()),
    )


if __name__ == "__main__":
    Validate()
    print("Stage 14D-01 worker shell contract: PASS")
