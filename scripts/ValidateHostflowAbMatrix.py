#!/usr/bin/env python3
"""Validate the HOSTFLOW H-C-03 A/B comparison matrix."""

from __future__ import annotations

import json
import math
import sys
from collections import Counter
from pathlib import Path


MATRIX_PATH = Path("docs/slice/REPORT/assets/hostflow_hc03_ab_matrix.json")
EXPECTED_AXES = {
    "import_preflight",
    "model_list_selection",
    "transform_layout",
    "profile_settings",
    "slice_job_cancel",
    "package_result",
    "workspace_persistence",
    "diagnostics_scope",
}
ALLOWED_DISPOSITIONS = {"equivalent", "known_trim", "slicer_only"}


def ReadJson(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def Main() -> int:
    if not MATRIX_PATH.is_file():
        print(f"HOSTFLOW_HC03_FAILED: missing {MATRIX_PATH}", file=sys.stderr)
        return 2

    matrix = ReadJson(MATRIX_PATH)
    errors: list[str] = []
    if matrix.get("schema") != "hostflow.ab_matrix.1":
        errors.append("matrix schema 不匹配")

    fixture = matrix.get("canonicalFixture", {})
    modelPath = Path(str(fixture.get("modelPath", "")))
    if not modelPath.is_file():
        errors.append(f"规范化模型不存在: {modelPath}")
    expectedFixture = {
        "mainProfileId": "textured_nail_rgb_only_lower_support",
        "hostProfileId": "host-reference-default",
        "profileSemantics": "legacy_rgb_solid_rgbwsv_strict",
        "dpiX": 635,
        "dpiY": 600,
        "materialStrategy": "rgb_solid",
    }
    for key, expectedValue in expectedFixture.items():
        if fixture.get(key) != expectedValue:
            errors.append(
                f"规范化输入 {key}={fixture.get(key)!r}, expected={expectedValue!r}"
            )
    if not math.isclose(
        float(fixture.get("layerThicknessMm", 0.0)), 0.038, abs_tol=1.0e-9
    ):
        errors.append("规范化层厚必须为 0.038 mm")

    buildVolume = fixture.get("buildVolume", {})
    expectedVolume = {
        "widthMm": 230.0,
        "heightMm": 100.0,
        "zLimitMm": 60.0,
        "origin": "lower_left",
        "xDirection": "positive",
        "yDirection": "positive",
    }
    for key, expectedValue in expectedVolume.items():
        if buildVolume.get(key) != expectedValue:
            errors.append(f"buildVolume.{key} 不匹配")

    declaredAxes = set(matrix.get("requiredAxes", []))
    if declaredAxes != EXPECTED_AXES:
        errors.append(
            "requiredAxes 不闭合: "
            f"missing={sorted(EXPECTED_AXES - declaredAxes)}, "
            f"extra={sorted(declaredAxes - EXPECTED_AXES)}"
        )

    entries = matrix.get("entries", [])
    identifiers = [str(entry.get("id", "")) for entry in entries]
    duplicateIds = sorted(
        identifier
        for identifier, count in Counter(identifiers).items()
        if count > 1
    )
    if duplicateIds:
        errors.append(f"重复对照项: {duplicateIds}")

    coveredAxes: set[str] = set()
    for entry in entries:
        identifier = str(entry.get("id", ""))
        axis = str(entry.get("axis", ""))
        disposition = str(entry.get("disposition", ""))
        if not identifier:
            errors.append("存在空 id")
        if axis not in EXPECTED_AXES:
            errors.append(f"{identifier} 使用未知 axis={axis}")
        else:
            coveredAxes.add(axis)
        if disposition not in ALLOWED_DISPOSITIONS:
            errors.append(f"{identifier} 使用未知 disposition={disposition}")
        for key in ("businessStep", "mainEvidence", "hostEvidence", "explanation"):
            if not str(entry.get(key, "")).strip():
                errors.append(f"{identifier} 缺少 {key}")

    if coveredAxes != EXPECTED_AXES:
        errors.append(f"矩阵未覆盖维度: {sorted(EXPECTED_AXES - coveredAxes)}")

    if errors:
        for error in errors:
            print(f"HOSTFLOW_HC03_FAILED: {error}", file=sys.stderr)
        return 3

    counts = Counter(entry["disposition"] for entry in entries)
    print(
        "HOSTFLOW_HC03_MATRIX_PASS "
        f"total={len(entries)} axes={len(coveredAxes)} "
        f"equivalent={counts['equivalent']} "
        f"known_trim={counts['known_trim']} "
        f"slicer_only={counts['slicer_only']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
