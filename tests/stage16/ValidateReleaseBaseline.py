#!/usr/bin/env python3

import argparse
import json
from pathlib import Path
from typing import Any


EXPECTED_STRATEGIES = {"S0", "S3", "S4"}
EXPECTED_FULL_COUNTS = {1, 11, 12, 22}


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def Require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--allow-quick", action="store_true")
    args = parser.parse_args()

    report = LoadJson(args.report)
    Require(
        report.get("schema") == "slicesoft.stage16.release_baseline.1",
        "unexpected Stage 16 Release baseline schema",
    )
    Require(report.get("stage") == "16C-02", "unexpected Stage 16 task")
    Require(report.get("status") == "complete", "baseline is incomplete")
    Require(
        report.get("productionStatus") == "INPUT_OPEN",
        "external production budget must remain INPUT_OPEN",
    )

    build = report.get("buildIdentity", {})
    for field in (
        "gitCommit",
        "buildConfig",
        "executablePath",
        "executableSha256",
        "compiler",
        "tiffBackend",
        "os",
    ):
        Require(bool(build.get(field)), f"missing build identity field: {field}")
    Require(build["buildConfig"] == "Release", "baseline must use Release")

    strategy_ids = {item["id"] for item in report.get("strategies", [])}
    Require(strategy_ids == EXPECTED_STRATEGIES, "S0/S3/S4 matrix drifted")
    instance_counts = {item["instances"] for item in report.get("cases", [])}
    expected_counts = (
        {1} if args.allow_quick else EXPECTED_FULL_COUNTS
    )
    Require(instance_counts == expected_counts, "scene count matrix drifted")

    summaries = report.get("summaries", [])
    Require(
        len(summaries) == len(EXPECTED_STRATEGIES) * len(expected_counts),
        "summary matrix is incomplete",
    )
    for summary in summaries:
        Require(summary["strategyId"] in EXPECTED_STRATEGIES, "unknown strategy")
        Require(summary["instanceCount"] in expected_counts, "unknown scene count")
        Require(summary["ripStrictPass"], "RIP strict failed")
        Require(
            bool(summary["deterministicOutputHash"]),
            "deterministic output hash is missing",
        )
        Require(summary["packageBytes"] > 0, "package is empty")
        cold = summary["cold"]
        warm = summary["warm"]
        Require(cold["coreOnlyMs"] >= 0, "invalid cold core timing")
        Require(cold["endToEndMs"] >= cold["coreOnlyMs"], "invalid cold total")
        Require(cold["peakWorkingSetBytes"] > 0, "missing cold memory")
        Require(warm["sampleCount"] >= 3, "insufficient warm samples")
        Require(warm["coreOnlyP50Ms"] >= 0, "invalid warm core p50")
        Require(
            warm["coreOnlyP95Ms"] >= warm["coreOnlyP50Ms"],
            "warm core p95 is below p50",
        )
        Require(
            warm["endToEndP95Ms"] >= warm["endToEndP50Ms"],
            "warm total p95 is below p50",
        )
        Require(warm["peakWorkingSetBytes"] > 0, "missing warm memory")

    print("STAGE16_RELEASE_BASELINE_VALIDATION_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
