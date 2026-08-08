#!/usr/bin/env python3
"""Validate the HOSTFLOW H-C-01 migration inventory against the live tree."""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path


INVENTORY_PATH = Path(
    "docs/slice/REPORT/assets/hostflow_hc01_migration_inventory.json"
)
SOURCE_ROOT = Path("apps/slicer_debug_ui")
LOCAL_INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)


def ReadText(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def ResolveLocalHeader(sourcePath: Path, includeName: str) -> Path | None:
    candidate = (sourcePath.parent / includeName).resolve()
    repositoryRoot = Path.cwd().resolve()
    try:
        relative = candidate.relative_to(repositoryRoot)
    except ValueError:
        return None
    return relative if candidate.is_file() and candidate.suffix == ".h" else None


def ValidateDirectUnit(
    headerPath: Path,
    bucketByPath: dict[str, str],
    errors: list[str],
) -> None:
    unitPaths = [headerPath]
    implementationPath = headerPath.with_suffix(".cpp")
    if implementationPath.is_file():
        unitPaths.append(implementationPath)
    for sourcePath in unitPaths:
        text = ReadText(sourcePath)
        if "slicer_core/" in text or "src/slicer_core" in text:
            errors.append(f"A 桶直接依赖 core: {sourcePath.as_posix()}")
        for includeName in LOCAL_INCLUDE_PATTERN.findall(text):
            localHeader = ResolveLocalHeader(sourcePath, includeName)
            if localHeader is None or localHeader == headerPath:
                continue
            localKey = localHeader.as_posix()
            if localKey in bucketByPath and bucketByPath[localKey] != "A":
                errors.append(
                    "A 桶依赖非 A 本地头文件: "
                    f"{sourcePath.as_posix()} -> {localKey}"
                )


def Main() -> int:
    repositoryRoot = Path.cwd()
    inventoryFile = repositoryRoot / INVENTORY_PATH
    if not inventoryFile.is_file():
        print(f"HOSTFLOW_HC01_FAILED: missing {INVENTORY_PATH}", file=sys.stderr)
        return 2
    inventory = json.loads(ReadText(inventoryFile))
    errors: list[str] = []
    if inventory.get("schema") != "hostflow.migration_inventory.1":
        errors.append("inventory schema 不匹配")

    entries = inventory.get("entries", [])
    inventoryPaths = [entry.get("path", "") for entry in entries]
    duplicates = sorted(
        path for path, count in Counter(inventoryPaths).items() if count > 1
    )
    if duplicates:
        errors.append(f"重复路径: {duplicates}")

    actualPaths = sorted(
        path.relative_to(repositoryRoot).as_posix()
        for path in (repositoryRoot / SOURCE_ROOT).rglob("*.h")
    )
    if sorted(inventoryPaths) != actualPaths:
        missing = sorted(set(actualPaths) - set(inventoryPaths))
        stale = sorted(set(inventoryPaths) - set(actualPaths))
        errors.append(f"清单未覆盖当前树: missing={missing}, stale={stale}")
    if inventory.get("headerCount") != len(actualPaths):
        errors.append(
            f"headerCount={inventory.get('headerCount')} actual={len(actualPaths)}"
        )

    reasonCodes = inventory.get("reasonCodes", {})
    bucketByPath: dict[str, str] = {}
    for entry in entries:
        path = entry.get("path", "")
        bucket = entry.get("bucket", "")
        reasonCode = entry.get("reasonCode", "")
        if bucket not in {"A", "B", "C"}:
            errors.append(f"非法 bucket: {path}={bucket}")
        if reasonCode not in reasonCodes:
            errors.append(f"未知 reasonCode: {path}={reasonCode}")
        bucketByPath[path] = bucket

    for path, bucket in bucketByPath.items():
        if bucket == "A":
            ValidateDirectUnit(repositoryRoot / path, bucketByPath, errors)

    if errors:
        for error in errors:
            print(f"HOSTFLOW_HC01_FAILED: {error}", file=sys.stderr)
        return 3

    counts = Counter(bucketByPath.values())
    print(
        "HOSTFLOW_HC01_PASS "
        f"total={len(actualPaths)} A={counts['A']} B={counts['B']} C={counts['C']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
