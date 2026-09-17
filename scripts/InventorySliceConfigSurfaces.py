#!/usr/bin/env python3

"""清点「切片配置来源面」：谁造配置、谁读配置（F-43）。

**为什么是工具而不是门禁。** F-43 原本的处置写的是「给其余四个来源面各加门禁」。
实测后改了：

1. **汇聚点唯一。** `SliceConfig` 只有 `load_slice_config` 一个命名入口，
   定义只在 `src/slicer_core/config.cpp` 一处。五种来源面无论怎么造配置，
   最终都要过这一关。在汇聚点强制（P0-07 的 slicingMode 必填、F-45 的未知键拒绝）
   **严格优于**给五个面分别加门禁——后者既冗余又会漏。

2. **冻结清单会变成噪音。** 实测需登记的文件近百个（运行本脚本可得当前数）。
   冻结这种规模的清单，结果是它常年变红、没人维护——比没有门禁更糟。

所以 F-43 剩下的是**可发现性**问题：契约要变时，怎么知道共有几面、都在哪。
本脚本就是回答这个问题的工具，**按需运行，不注册为 ctest**。

用法：
    python scripts/InventorySliceConfigSurfaces.py          # 概览
    python scripts/InventorySliceConfigSurfaces.py --list   # 列出全部文件
"""

import argparse
import io
import os
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
ROOTS = ("src", "apps", "tests", "scripts")
EXTS = (".cpp", ".h", ".c", ".py", ".ps1", ".in")

# 五种来源面。判据取自实测，不是臆分。
SURFACES = [
    ("A. 盘上的 JSON 配置",
     "samples/configs/**.json，顶层 input.modelPath",
     "已有门禁：slicing_mode_declared_contract_test（按 schema 分档，含空转自检）"),
    ("B. 测试源码里的字符串字面量",
     r'C++ 里的 "\"modelPath\"" 形态',
     "无独立门禁——由汇聚点强制兜住：缺 slicingMode 或未知键会在 load 时失败"),
    ("C. 程序化构造 Json::Object",
     '普通引号键名 {"modelPath", ...}，按转义写法 grep 会漏掉这一类',
     "同 B"),
    ("D. 纯 C 宿主 builder",
     "apps/slicer_host_sim/HostRequestBuilder.c，规范形式与紧凑形式【双份】手工维护",
     "同 B；另有 ComputeProfileDocumentHash 在引擎侧重算哈希，两份形式偏离会被抓到"),
    ("E. 引擎侧回读调用方传入的路径",
     "ProductionRepairFacadeFactory / ModelPreflightService 等 5 处",
     "契约上由调用方负责声明；同样过汇聚点"),
]


def Scan():
    authors, consumers = [], []
    for base in ROOTS:
        root0 = REPO / base
        if not root0.is_dir():
            continue
        for root, dirs, files in os.walk(root0):
            dirs[:] = [d for d in dirs if d != "__pycache__"]
            for name in sorted(files):
                if not name.endswith(EXTS):
                    continue
                path = Path(root) / name
                try:
                    text = io.open(path, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                rel = path.relative_to(REPO).as_posix()
                makes = "modelPath" in text and "packageDir" in text
                reads = "load_slice_config" in text
                if makes:
                    authors.append(rel)
                elif reads:
                    consumers.append(rel)
    return authors, consumers


def CheckConvergence() -> list[str]:
    """核验「汇聚点唯一」这个前提是否仍成立——它是本文全部论证的基础。

    判据必须是**结构化**的，不能用裸子串。首版用 `"RejectUnknownKeys" not in text`
    判断未知键拒绝是否还在，证伪时把它改名成 `RejectUnknownKeysDISABLED` ——
    子串仍在，检查照样通过，整条核验是空转的。故改为「要求定义 + 足量调用点」。
    """
    problems = []

    configCpp = (REPO / "src" / "slicer_core" / "config.cpp").read_text(
        encoding="utf-8", errors="replace")
    if not re.search(r"^SliceConfig\s+load_slice_config\s*\(", configCpp, re.MULTILINE):
        problems.append("load_slice_config 不在 src/slicer_core/config.cpp 定义")

    # P0-07：必须真的在「缺 slicingMode 时抛异常」，而不只是字面量还在某处。
    if not re.search(
            r"if\s*\(\s*!\s*root\.contains\(\s*\"slicingMode\"\s*\)\s*\)\s*\{[^}]*throw",
            configCpp, re.DOTALL):
        problems.append("P0-07 的 slicingMode 必填强制不见了（缺失时不再抛异常）")

    if not re.search(r"NormalizeConfigJson\s*\(", configCpp):
        problems.append("load_slice_config 不再调用归一化")

    migration = (REPO / "src" / "slicer_core" / "config" / "ConfigMigration.cpp").read_text(
        encoding="utf-8", errors="replace")
    # F-45：要求函数定义存在，且顶层与三个容器各有一处调用（共 4 处）。
    defined = re.search(r"\bvoid\s+RejectUnknownKeys\s*\(", migration) is not None
    callSites = len(re.findall(r"(?<!void\s)\bRejectUnknownKeys\s*\(\s*\w", migration))
    if not defined:
        problems.append("F-45 的 RejectUnknownKeys 定义不见了")
    elif callSites < 4:
        problems.append(
            f"F-45 的未知键拒绝调用点只剩 {callSites} 处（应为顶层 + pipeline/geometry/materials 共 4 处）")

    return problems


def Main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true", help="列出全部文件")
    arguments = parser.parse_args()

    print("=== 切片配置的五种来源面 ===")
    for name, predicate, gate in SURFACES:
        print(f"  {name}")
        print(f"      判据：{predicate}")
        print(f"      强制：{gate}")
    print()

    problems = CheckConvergence()
    print("=== 汇聚点核验 ===")
    if problems:
        print("  ⚠ 前提已不成立，上面「由汇聚点兜住」的论证随之失效：")
        for p in problems:
            print(f"      ✗ {p}")
    else:
        print("  ✓ load_slice_config 仍是唯一入口，且两道强制（必填 / 未知键拒绝）都在位")
    print()

    authors, consumers = Scan()
    print("=== 规模 ===")
    print(f"  造配置的文件：{len(authors)} 个")
    print(f"  只读配置的文件：{len(consumers)} 个")
    print(f"  合计：{len(authors) + len(consumers)} 个")
    print()
    print("  这个规模就是【不把它做成冻结清单门禁】的理由——")
    print("  近百个文件的清单会常年变红、没人维护，比没有门禁更糟。")

    if arguments.list:
        print()
        print("=== 造配置 ===")
        for a in authors:
            print("  " + a)
        print()
        print("=== 只读配置 ===")
        for c in consumers:
            print("  " + c)

    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(Main())
