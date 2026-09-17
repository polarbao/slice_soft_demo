#!/usr/bin/env python3

"""采集/比对切片产物的字节级基线（F-09 第 0 步）。

为什么需要它：`tests/golden/expected/` 下 28 份 golden 全是**摘要级或 schema 级**
（输出契约、报告骨架、矩阵期望），没有一份是切片产物的字节哈希。250 项回归能告诉你
「没有新的失败」，但**说不了「产出的 TIFF 与重构前逐字节相同」**。

`slicer.cpp` 拆解的全部正当性建立在「只搬运、不改行为」上——没有字节级对照物，
这句话无法证明。故本脚本是 F-09 第 1 步之前的硬前置。

用法：
    python scripts/CaptureSliceOutputBaseline.py --capture   # 采集基线
    python scripts/CaptureSliceOutputBaseline.py --verify    # 与基线比对

产物落 E 盘（C 盘常年接近写满），仓内只留哈希清单。
"""

import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
MANIFEST = REPO / "scripts" / "SliceOutputBaseline.json"
DEFAULT_WORK = Path("E:/slicesoft-baseline")

# 11 种 (slicingMode, packageProtocol, 支撑, 光油, 贴图) 组合各取一个代表。
# 组合由 samples/configs 下 88 份可跑配置实测归并得到，不是臆选。
CASES = [
    ("cms_tex",           "samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json"),
    ("cms_var",           "samples/configs/support/support_outer_varnish_shell_1px.json"),
    ("cms_sup",           "samples/configs/support/support_base_projection_30_layers.json"),
    ("cms_sup_var",       "samples/configs/support/support_outer_varnish_shell_2px_with_support.json"),
    ("cms_sup_var_tex",   "samples/configs/texture_fill_partition/global_production_xiao_ma_material_parity.json"),
    ("cms_rgbwsvt",       "samples/configs/matvol_t/transfer_rgbwsvt_prototype.json"),
    ("relief_plain",      "samples/configs/material_mapping/obj_mtl_material_mapping_ignore.json"),
    ("relief_tex",        "samples/configs/material_process/stage15_f03_four_value.json"),
    ("relief_sup",        "samples/configs/material_mapping/obj_mtl_material_mapping_rgbwv.json"),
    ("relief_sup_tex",    "samples/configs/golden/material_process_top2_fixture.json"),
    ("relief_sup_var_tex", "samples/configs/support/cross_section_material_stack_real_obj.json"),
]


def Sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def LoadConfig(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def RunCase(cli: Path, name: str, configRel: str, work: Path) -> dict:
    """跑一个配置并返回其产物的哈希清单。"""
    source = REPO / configRel
    document = LoadConfig(source)

    # packageDir 必须改成绝对路径：相对路径会写进工作树里被 gitignore 的 output/，
    # 既污染工作树又让两次运行互相覆盖。
    caseDir = work / name
    if caseDir.exists():
        shutil.rmtree(caseDir)
    caseDir.mkdir(parents=True, exist_ok=True)
    document["output"]["packageDir"] = str((caseDir / "package").as_posix())

    # 预览若开启会额外产出，一并纳入——它们同样是「不得改变」的产物。
    runConfig = caseDir / "config.json"
    # 模型路径在原配置里是相对其所在目录的，改写后目录变了，故转成绝对路径。
    modelPath = (source.parent / document["input"]["modelPath"]).resolve()
    document["input"]["modelPath"] = str(modelPath.as_posix())
    runConfig.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")

    started = time.time()
    completed = subprocess.run(
        [str(cli), "--config", str(runConfig)],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=str(REPO), timeout=3600)
    elapsed = time.time() - started

    artifacts = {}
    if caseDir.exists():
        for path in sorted(caseDir.rglob("*")):
            if not path.is_file() or path == runConfig:
                continue
            rel = path.relative_to(caseDir).as_posix()
            artifacts[rel] = {"sha256": Sha256(path), "bytes": path.stat().st_size}

    return {
        "config": configRel,
        "exitCode": completed.returncode,
        "elapsedSeconds": round(elapsed, 2),
        "artifactCount": len(artifacts),
        "artifacts": artifacts,
        "stderrTail": (completed.stderr or "").strip().splitlines()[-3:],
    }


def Main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--capture", action="store_true", help="采集基线并写入清单")
    group.add_argument("--verify", action="store_true", help="与既有清单比对")
    parser.add_argument("--work", type=Path, default=DEFAULT_WORK,
                        help="产物工作目录（默认 E:/slicesoft-baseline）")
    parser.add_argument("--config-name", default="Debug", choices=("Debug", "Release"))
    parser.add_argument("--only", default="", help="只跑名字包含该子串的用例")
    arguments = parser.parse_args()

    cli = REPO / "build-slicesoft" / "main" / arguments.config_name / "slicer_cli.exe"
    if not cli.exists():
        raise SystemExit(f"找不到 slicer_cli：{cli}")

    cases = [c for c in CASES if arguments.only in c[0]]
    arguments.work.mkdir(parents=True, exist_ok=True)

    results = {}
    failures = []
    for index, (name, configRel) in enumerate(cases, start=1):
        print(f"[{index}/{len(cases)}] {name:<20} {configRel}", flush=True)
        outcome = RunCase(cli, name, configRel, arguments.work)
        results[name] = outcome
        status = "OK" if outcome["exitCode"] == 0 else f"退出码 {outcome['exitCode']}"
        print(f"      {status}  产物 {outcome['artifactCount']} 个  "
              f"{outcome['elapsedSeconds']} 秒", flush=True)
        if outcome["exitCode"] != 0:
            failures.append(name)
            for line in outcome["stderrTail"]:
                print(f"        {line[:140]}", flush=True)

    print()
    if arguments.capture:
        payload = {
            "schemaVersion": 1,
            "note": "F-09 第 0 步字节级基线。每步拆解后用 --verify 比对；任何差异都必须能解释。",
            "cases": results,
        }
        MANIFEST.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
        total = sum(r["artifactCount"] for r in results.values())
        print(f"已写入 {MANIFEST.relative_to(REPO)}：{len(results)} 个用例 / {total} 个产物")
        if failures:
            print(f"⚠ 有 {len(failures)} 个用例退出码非零：{', '.join(failures)}")
            print("  基线仍已记录（含其退出码），但这些用例在比对时意义有限。")
        return 0

    # --verify
    if not MANIFEST.exists():
        raise SystemExit(f"找不到基线清单：{MANIFEST}，请先 --capture")
    baseline = json.loads(MANIFEST.read_text(encoding="utf-8"))["cases"]

    drift = []
    for name, outcome in results.items():
        base = baseline.get(name)
        if base is None:
            drift.append(f"{name}: 基线中不存在该用例")
            continue
        if base["exitCode"] != outcome["exitCode"]:
            drift.append(f"{name}: 退出码 {base['exitCode']} → {outcome['exitCode']}")
        baseArtifacts = base["artifacts"]
        for rel, info in sorted(outcome["artifacts"].items()):
            if rel not in baseArtifacts:
                drift.append(f"{name}/{rel}: 新增产物")
            elif baseArtifacts[rel]["sha256"] != info["sha256"]:
                drift.append(
                    f"{name}/{rel}: 字节改变（{baseArtifacts[rel]['bytes']} → {info['bytes']} 字节）")
        for rel in sorted(baseArtifacts):
            if rel not in outcome["artifacts"]:
                drift.append(f"{name}/{rel}: 产物消失")

    if drift:
        print(f"字节级比对：**{len(drift)} 处差异**")
        for line in drift[:60]:
            print(f"  ✗ {line}")
        if len(drift) > 60:
            print(f"  …另有 {len(drift) - 60} 处")
        return 1
    total = sum(r["artifactCount"] for r in results.values())
    print(f"字节级比对：PASS（{len(results)} 个用例 / {total} 个产物逐字节一致）")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
