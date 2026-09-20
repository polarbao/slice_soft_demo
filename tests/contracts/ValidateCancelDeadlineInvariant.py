#!/usr/bin/env python3
"""取消期限不变式门禁：门禁期限必须大于模块给 worker 的优雅退出宽限期。

为什么需要这条门禁
------------------
worker 不自行退出时，模块**先等满 `cancelGracePeriod` 才强杀**
（`src/slicer_module/WorkerClient.cpp` 的运行循环 → `TerminateJobObject`），
而作业状态要等 `Run()` 返回才转终态（`src/slicer_module/WorkerJobService.cpp`）。
所以「取消到终态」在这条路径上的**时间下界就是宽限期本身**。

2026-09-18 的 F-53：两处门禁期限与宽限期同为 2000ms，于是
「worker 未自行退出」这条路径**必然判失败**，余量为负。
读数是双峰的——自行退出 12ms，走强杀 2053~2190ms，中间一次都没有——
却被误读成「余量约 1%」的抖动，连续多日污染全量回归的失败集合比对。

三个常量写在三个互不相干的文件里，没有任何机制保证它们的关系。
宿主经 LoadLibrary + C ABI 加载模块，**不能 include 模块内部头**
（会破坏三进程拓扑），所以无法共享编译期常量。本脚本替代那个编译期检查。

授权：docs/slice/DOC/DOC_DECISION_F53_2026_09_20_取消期限与宽限期不变式授权.md
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

GRACE_FILE = Path("src/slicer_module/WorkerClient.h")
GRACE_PATTERN = re.compile(
    r"inline\s+constexpr\s+std::chrono::milliseconds\s+"
    r"kDefaultCancelGracePeriod\s*\{\s*(\d+)\s*\}")

# 每条门禁：(相对路径, 提取期限的正则, 人类可读的名字)
GATES = [
    (
        Path("apps/slicer_ui_host_sim/CapabilityCoverageRunner.cpp"),
        re.compile(r"constexpr\s+int\s+kCancelLatencyLimitMs\s*\{\s*(\d+)\s*\}"),
        "UI-M5（slicer_stage14e04b_capability_coverage_test / hostflow_ha03_qt_end_to_end）",
    ),
]

# stage14d_07 的期限是**从宽限期推导**的，所以这里校验的是推导式仍然成立，
# 而不是一个独立的字面量。
DERIVED_FILE = Path("tests/stage14d_07/EngineConformanceGate.cpp")
DERIVED_GRACE_PATTERN = re.compile(
    r"constexpr\s+double\s+kCancelGraceMs\s*=\s*"
    r"static_cast<double>\(\s*module::kDefaultCancelGracePeriod\.count\(\)\s*\)")
DERIVED_BUDGET_PATTERN = re.compile(
    r"constexpr\s+double\s+kForcedTerminationBudgetMs\s*=\s*([0-9.]+)")
DERIVED_SUM_PATTERN = re.compile(
    r"constexpr\s+double\s+kCancelDeadlineMs\s*=\s*"
    r"kCancelGraceMs\s*\+\s*kForcedTerminationBudgetMs")
# 这两处一旦退回字面量，推导就断了，必须挡住。
DERIVED_LITERAL_PATTERN = re.compile(r"cancelElapsedMs\s*<=\s*[0-9]+(?:\.[0-9]+)?")


def _read(root: Path, rel: Path) -> str:
    path = root / rel
    if not path.is_file():
        raise SystemExit(f"[FAIL] 找不到文件：{rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def _extract(text: str, pattern: re.Pattern[str], rel: Path, what: str) -> int:
    hits = pattern.findall(text)
    if len(hits) != 1:
        raise SystemExit(
            f"[FAIL] {rel} 里 {what} 命中 {len(hits)} 次，应为 1 次。\n"
            f"       正则：{pattern.pattern}\n"
            f"       常量被改名或改写形态时本门禁会失效，故此处硬性要求唯一命中。")
    return int(float(hits[0]))


def check(root: Path) -> list[str]:
    """返回违规说明列表；空列表表示通过。"""
    problems: list[str] = []

    grace = _extract(
        _read(root, GRACE_FILE), GRACE_PATTERN, GRACE_FILE,
        "kDefaultCancelGracePeriod")

    for rel, pattern, label in GATES:
        limit = _extract(_read(root, rel), pattern, rel, "门禁期限")
        if limit <= grace:
            problems.append(
                f"{label}\n"
                f"    门禁期限 {limit}ms <= 宽限期 {grace}ms —— {rel}\n"
                f"    「worker 未自行退出」这条路径的耗时下界就是宽限期，\n"
                f"    期限不大于它则该路径必然判失败（这正是 F-53）。")

    derived = _read(root, DERIVED_FILE)
    if not DERIVED_GRACE_PATTERN.search(derived):
        problems.append(
            f"stage14d_07 E-07\n"
            f"    {DERIVED_FILE} 的 kCancelGraceMs 不再从 "
            f"module::kDefaultCancelGracePeriod 推导。\n"
            f"    改回字面量会让它与模块的宽限期各自漂移，F-53 会复发。")
    if not DERIVED_SUM_PATTERN.search(derived):
        problems.append(
            f"stage14d_07 E-07\n"
            f"    {DERIVED_FILE} 的 kCancelDeadlineMs 不再等于 "
            f"kCancelGraceMs + kForcedTerminationBudgetMs。")
    else:
        budget = _extract(
            derived, DERIVED_BUDGET_PATTERN, DERIVED_FILE,
            "kForcedTerminationBudgetMs")
        if budget <= 0:
            problems.append(
                f"stage14d_07 E-07\n"
                f"    kForcedTerminationBudgetMs = {budget}，必须为正数，\n"
                f"    否则期限不会大于宽限期。")
    stray = DERIVED_LITERAL_PATTERN.findall(derived)
    if stray:
        problems.append(
            f"stage14d_07 E-07\n"
            f"    {DERIVED_FILE} 里出现了字面量期限比较：{stray}\n"
            f"    应写成 `cancelElapsedMs <= kCancelDeadlineMs`。")

    return problems


def self_test() -> int:
    """证明本门禁确实会红：构造三种违规形态，逐一确认被挡住。

    本仓纪律：对拍测试可能整条空转，全绿不等于验过。
    所以这里不只是「跑一遍真文件」，而是**人为破坏不变式并断言它变红**。
    """
    import shutil
    import tempfile

    cases: list[tuple[str, dict[Path, tuple[str, str]]]] = [
        (
            "门禁期限等于宽限期（F-53 原始形态）",
            {GATES[0][0]: ("kCancelLatencyLimitMs{3000}",
                           "kCancelLatencyLimitMs{2000}")},
        ),
        (
            "门禁期限小于宽限期",
            {GATES[0][0]: ("kCancelLatencyLimitMs{3000}",
                           "kCancelLatencyLimitMs{1500}")},
        ),
        (
            "stage14d_07 退回字面量比较",
            {DERIVED_FILE: ("cancelElapsedMs <= kCancelDeadlineMs",
                            "cancelElapsedMs <= 2000.0")},
        ),
        (
            "强杀预算被改为 0",
            {DERIVED_FILE: ("kForcedTerminationBudgetMs = 1000.0",
                            "kForcedTerminationBudgetMs = 0.0")},
        ),
    ]

    failures = 0
    for label, mutations in cases:
        with tempfile.TemporaryDirectory() as tmp:
            sandbox = Path(tmp) / "repo"
            for rel in {GRACE_FILE, DERIVED_FILE} | {g[0] for g in GATES}:
                dst = sandbox / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(REPO_ROOT / rel, dst)
            for rel, (old, new) in mutations.items():
                target = sandbox / rel
                text = target.read_text(encoding="utf-8", errors="replace")
                if text.count(old) < 1:
                    print(f"  [自测跳过] {label}：锚点 {old!r} 未命中，"
                          f"常量形态已变，请更新本自测")
                    failures += 1
                    continue
                target.write_text(
                    text.replace(old, new, 1), encoding="utf-8", newline="")
            try:
                problems = check(sandbox)
            except SystemExit as exc:
                problems = [str(exc)]
            if problems:
                print(f"  [自测通过] {label} —— 已被挡住")
            else:
                print(f"  [自测失败] {label} —— **本门禁没有挡住它**")
                failures += 1

    # 未被破坏的真实仓库必须是绿的，否则自测本身没有区分力。
    if check(REPO_ROOT):
        print("  [自测失败] 未破坏的仓库也被判红，本门禁无区分力")
        failures += 1
    else:
        print("  [自测通过] 未破坏的仓库判绿")

    if failures:
        print(f"\n自测未通过：{failures} 项")
        return 1
    print("\n自测全部通过：本门禁对四种违规形态都会变红，对干净仓库判绿")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-test", action="store_true",
        help="人为破坏不变式，确认本门禁确实会变红")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    problems = check(REPO_ROOT)
    if not problems:
        grace = _extract(
            _read(REPO_ROOT, GRACE_FILE), GRACE_PATTERN, GRACE_FILE,
            "kDefaultCancelGracePeriod")
        print(f"取消期限不变式 PASS（宽限期 {grace}ms，所有门禁期限均大于它）")
        return 0

    print("取消期限不变式 FAIL —— 门禁期限必须大于 worker 的优雅退出宽限期\n")
    for item in problems:
        print(f"  * {item}\n")
    print("背景：worker 不自行退出时，模块先等满宽限期才强杀，")
    print("      作业状态要等 Run() 返回才转终态，")
    print("      所以那条路径的耗时下界就是宽限期本身。")
    print("      期限不大于宽限期 ⇒ 该路径必然判失败，且症状会伪装成偶发抖动。")
    print("往事：2026-09-18 F-53 —— 两处门禁与宽限期同为 2000ms，")
    print("      读数双峰（自行退出 12ms / 走强杀 2053~2190ms）被误读为「余量 1%」，")
    print("      连续多日污染全量回归的失败集合比对。")
    print("授权：改动这些常量须更新")
    print("      docs/slice/DOC/DOC_DECISION_F53_2026_09_20_取消期限与宽限期不变式授权.md")
    return 1


if __name__ == "__main__":
    sys.exit(main())
