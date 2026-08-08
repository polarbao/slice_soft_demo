#!/usr/bin/env python3
"""Validate the H-C-02 plan against the frozen H-C-01 B bucket."""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path


INVENTORY_PATH = Path(
    "docs/slice/REPORT/assets/hostflow_hc01_migration_inventory.json"
)
PLAN_PATH = Path("docs/slice/REPORT/assets/hostflow_hc02_migration_plan.json")


def ReadJson(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def Main() -> int:
    inventory = ReadJson(INVENTORY_PATH)
    plan = ReadJson(PLAN_PATH)
    errors: list[str] = []
    if plan.get("schema") != "hostflow.migration_plan.1":
        errors.append("plan schema 不匹配")
    if plan.get("inventorySchema") != inventory.get("schema"):
        errors.append("inventory schema 引用不匹配")

    expectedPaths = {
        entry["path"]
        for entry in inventory.get("entries", [])
        if entry.get("bucket") == "B"
    }
    entries = plan.get("entries", [])
    actualPaths = [entry.get("path", "") for entry in entries]
    duplicatePaths = sorted(
        path for path, count in Counter(actualPaths).items() if count > 1
    )
    if duplicatePaths:
        errors.append(f"重复计划项: {duplicatePaths}")
    if set(actualPaths) != expectedPaths:
        errors.append(
            "B 桶计划不闭合: "
            f"missing={sorted(expectedPaths - set(actualPaths))}, "
            f"extra={sorted(set(actualPaths) - expectedPaths)}"
        )

    workPackages = plan.get("workPackages", {})
    effortScale = plan.get("effortScale", {})
    allowedActions = {
        "replace_with_reference_host",
        "adapt_to_public_dto",
        "adapt_to_host_profile",
    }
    for entry in entries:
        path = entry.get("path", "")
        if entry.get("workstream") not in workPackages:
            errors.append(f"未知 workstream: {path}")
        if entry.get("effort") not in effortScale:
            errors.append(f"未知 effort: {path}")
        if entry.get("action") not in allowedActions:
            errors.append(f"未知 action: {path}")
        if not str(entry.get("replacement", "")).strip():
            errors.append(f"缺少 replacement: {path}")

    if errors:
        for error in errors:
            print(f"HOSTFLOW_HC02_FAILED: {error}", file=sys.stderr)
        return 2
    workstreamCounts = Counter(entry["workstream"] for entry in entries)
    print(
        "HOSTFLOW_HC02_PASS "
        f"total={len(entries)} "
        + " ".join(
            f"{name}={workstreamCounts[name]}" for name in sorted(workPackages)
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
