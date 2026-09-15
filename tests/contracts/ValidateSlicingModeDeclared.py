#!/usr/bin/env python3

"""samples/configs 下的切片配置必须显式声明 slicingMode。

P0FIX/P0-07 把 slicingMode 从「省略即 closed_mesh_scanline」改成了必填。
改的是 load_slice_config 的解析层，而仓内配置是靠一次性补齐跟上的——
没有门禁的话，下一个新增配置照样会漏，补齐会慢慢烂掉，而漏掉的代价是
静默走错切片路径（slicer.cpp 中 texture / support / materialRoleMapping /
columnRanges 四处按 slicing_mode 分叉，且其中多数不在交叉校验的覆盖范围内）。

判据与 load_slice_config 对齐：顶层含 input.modelPath 即视为切片配置。
合法取值以 validate_slice_config 为准。

**声明位置按 schema 分档**：`slicer.config.1` 的 slicingMode 住在 `pipeline` 下，
不在顶层——NormalizeSlicerConfig1（`src/slicer_core/config/ConfigMigration.cpp`）
只搬运白名单键，写在顶层会被静默丢弃，引擎照样判「未声明」。所以只查顶层的门禁
会对 v1 配置报绿而引擎拒绝，比不查更坏。反向也要查：v1 配置**不该**有顶层
slicingMode，那是个看着生效、实则被忽略的误导键。
"""

import json
from pathlib import Path
from typing import Any

LEGAL_MODES = ("closed_mesh_scanline", "relief_heightfield")
SLICER_CONFIG_1 = "slicer.config.1"


def LoadJson(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def IsSliceConfig(document: Any) -> bool:
    if not isinstance(document, dict):
        return False
    modelInput = document.get("input")
    return isinstance(modelInput, dict) and "modelPath" in modelInput


def DeclaredSlicingMode(document: dict) -> tuple[Any, str]:
    """返回 (声明值或 None, 该 schema 下的正确位置)。"""
    if document.get("schema") == SLICER_CONFIG_1:
        pipeline = document.get("pipeline")
        if isinstance(pipeline, dict) and "slicingMode" in pipeline:
            return pipeline["slicingMode"], "pipeline.slicingMode"
        return None, "pipeline.slicingMode"
    if "slicingMode" in document:
        return document["slicingMode"], "slicingMode"
    return None, "slicingMode"


def Main() -> int:
    repoRoot = Path(__file__).resolve().parents[2]
    configRoot = repoRoot / "samples" / "configs"
    if not configRoot.is_dir():
        raise AssertionError(f"config root is missing: {configRoot}")

    missing: list[str] = []
    illegal: list[str] = []
    ignored: list[str] = []
    checked = 0

    for path in sorted(configRoot.rglob("*.json")):
        try:
            document = LoadJson(path)
        except json.JSONDecodeError as error:
            raise AssertionError(f"{path.relative_to(repoRoot)} is not valid JSON: {error}")
        if not IsSliceConfig(document):
            continue
        checked += 1
        relative = path.relative_to(repoRoot).as_posix()
        declared, location = DeclaredSlicingMode(document)
        if declared is None:
            missing.append(f"{relative} (declare it at {location})")
            continue
        if declared not in LEGAL_MODES:
            illegal.append(f"{relative} -> {location} = {declared!r}")
        if location != "slicingMode" and "slicingMode" in document:
            ignored.append(relative)

    if missing:
        raise AssertionError(
            "these slice configs omit slicingMode, which load_slice_config now rejects:\n  "
            + "\n  ".join(missing))
    if illegal:
        raise AssertionError(
            "these slice configs declare an unsupported slicingMode:\n  "
            + "\n  ".join(illegal))
    if ignored:
        raise AssertionError(
            "these slicer.config.1 configs carry a top-level slicingMode that "
            "NormalizeSlicerConfig1 silently drops; it reads as declared but is "
            "ignored, so remove it and keep pipeline.slicingMode:\n  "
            + "\n  ".join(ignored))
    if checked == 0:
        raise AssertionError(
            "no slice config was discovered under samples/configs; "
            "the discovery predicate has drifted and this gate is vacuous")

    print(f"slicingMode declaration contract: PASS ({checked} slice configs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
