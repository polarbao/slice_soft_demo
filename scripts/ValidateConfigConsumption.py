#!/usr/bin/env python3
"""SliceConfig 消费面门禁（R-13 方案 a / F-10）。

【为什么是这个而不是按车道拆分】F-10 的诉求是「哪些配置项影响哪个车道无法从类型判断，
只能靠 DiagnosticEffectiveConfig（855 行）事后解释」——那是**可理解性**问题，不是耦合问题。
路线图原方案是把 SliceConfig 拆成 5 条车道类型（R-13，L 成本、92 个文件），
但 2026-09-17 实测推翻了它的前提：

    25 个顶层成员 / 42 个消费文件，其中读 >=10 个成员的只有 5 个
    （config.cpp 25、slicer.cpp 20、GlobalSurfaceShellProductionPipeline 13、
      SliceMaterialTexture 11、RetainedMaterialLayerComposer 11）

也就是说绝大多数文件本来就只碰一两个成员、拆了没帮助；而那 5 个重度文件拆完仍需大部分车道。
收益不在拆分上，在于**把这张消费图变成可查、且能被机器守住的东西**——本脚本就是它。

【本脚本守两件事】
  G-C1  任何 SliceConfig 顶层成员都必须有至少一个消费者（config.cpp 之外）。
        零消费者意味着「用户配得上、但什么都不影响」——是已经收下却无人读取的死旋钮。
  G-C2  读 >=10 个成员的文件数不得超过基线（棘轮）。它盯的是消费面扩散，
        不冻结清单、不替任何专项承接既有的 5 个重度文件。

【匹配器的边界，必须知道】本脚本按 `config.<成员>` 等少数别名做文本匹配，不做真正的 C++ 解析。
方向是【偏窄】的：匹配到就一定是真消费；匹配不到则可能是经别名或引用访问。
所以 G-C1 的零消费者判定配了一份允许清单（baseline 的 deadMemberAllowlist），
用于登记「确实经别名访问」的成员——但登记必须写明是哪个别名，不能拿它当消音器。

反面教材留痕：2026-09-17 首次测这张图时用的是宽松匹配器（裸 `.成员`），
得到 99 个消费文件——而 `layer.support`、`result.support`、`instance.*` 根本不是 SliceConfig，
是同名成员的别的结构体。虚高 2.4 倍。宽松匹配器在这里不是「更保险」，是错的。

用法：
    python scripts/ValidateConfigConsumption.py            # 跑门禁
    python scripts/ValidateConfigConsumption.py --list      # 打印消费图
    python scripts/ValidateConfigConsumption.py --self-test # 自检判定逻辑
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG_HEADER = "src/slicer_core/config.h"
CONFIG_SOURCE = "src/slicer_core/config.cpp"
BASELINE_PATH = "scripts/ConfigConsumptionBaseline.json"
SCAN_ROOTS = ("src", "apps")

# 引用 SliceConfig 的变量名。2026-09-17 实测：`config.` 占 898 次、是绝对主流；
# 其余别名按实际出现补入。新增别名时请一并更新，否则会把真消费者判成零消费。
CONFIG_ALIASES = (
    "config",
    "sliceConfig",
    "slice_config",
    "sliceSettings",
    "effectiveConfig",
)


def ReadText(relativePath: str) -> str:
    absolutePath = os.path.join(REPO_ROOT, relativePath)
    with open(absolutePath, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def CollectTopLevelMembers(headerText: str) -> list[str]:
    """取 SliceConfig 的顶层成员名，保持声明顺序。"""
    body = re.search(r"struct SliceConfig \{(.*?)\n\};", headerText, re.S)
    if body is None:
        raise AssertionError(
            "config.h 里找不到 struct SliceConfig 的定义 —— "
            "本门禁靠它取成员清单，结构变化请同步本脚本"
        )
    return re.findall(r"\n\s+[A-Za-z_][\w:<>, ]*?\s+([a-z_][a-z0-9_]*)\s*;", body.group(1))


def CollectSourceFiles() -> list[str]:
    files: list[str] = []
    for root in SCAN_ROOTS:
        for directoryPath, _, fileNames in os.walk(os.path.join(REPO_ROOT, root)):
            for fileName in fileNames:
                if not fileName.endswith((".cpp", ".h")):
                    continue
                absolutePath = os.path.join(directoryPath, fileName)
                files.append(
                    os.path.relpath(absolutePath, REPO_ROOT).replace("\\", "/"))
    return sorted(files)


def BuildConsumptionMap(
    members: list[str], files: list[str]
) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """返回（成员 -> 消费文件）与（文件 -> 所读成员）两张表。"""
    aliasGroup = "|".join(CONFIG_ALIASES)
    patterns = {
        member: re.compile(r"\b(?:%s)\.%s\b" % (aliasGroup, re.escape(member)))
        for member in members
    }
    byMember: dict[str, list[str]] = {member: [] for member in members}
    byFile: dict[str, list[str]] = {}
    for path in files:
        text = ReadText(path)
        seen = [member for member in members if patterns[member].search(text)]
        if seen:
            byFile[path] = seen
        for member in seen:
            byMember[member].append(path)
    return byMember, byFile


def EvaluateGates(
    byMember: dict[str, list[str]],
    byFile: dict[str, list[str]],
    baseline: dict,
) -> list[str]:
    """返回错误信息列表；空列表表示通过。"""
    errors: list[str] = []
    allowlist = set(baseline.get("deadMemberAllowlist", []))
    heavyThreshold = int(baseline["heavyThreshold"])
    heavyBaseline = int(baseline["heavyFileBaseline"])

    # G-C1：零消费者的成员
    dead = [
        member
        for member, consumers in byMember.items()
        if not [path for path in consumers if path != CONFIG_SOURCE]
    ]
    unexpectedDead = sorted(set(dead) - allowlist)
    if unexpectedDead:
        errors.append(
            "G-C1 以下 SliceConfig 成员没有任何消费者（config.cpp 之外）: "
            + ", ".join(unexpectedDead)
            + "\n    这意味着它被收下却无人读取 —— 用户配得上、但什么都不影响。"
            + "\n    若它确实经别名或引用访问，请把别名补进本脚本的 CONFIG_ALIASES；"
            + "\n    只有在确实无法用别名表达时，才登记进 "
            + BASELINE_PATH
            + " 的 deadMemberAllowlist，并写明经哪条路径访问。"
        )

    # 已登记却其实有消费者的条目：允许清单过期了，应当收回
    staleAllowed = sorted(allowlist - set(dead))
    if staleAllowed:
        errors.append(
            "G-C1 以下成员已登记在 deadMemberAllowlist 里，但实测有消费者: "
            + ", ".join(staleAllowed)
            + "\n    允许清单过期即失去意义，请从 "
            + BASELINE_PATH
            + " 里删掉这些条目。"
        )

    # G-C2：重度消费文件的棘轮
    heavy = sorted(
        (path for path, seen in byFile.items() if len(seen) >= heavyThreshold),
        key=lambda path: -len(byFile[path]),
    )
    if len(heavy) > heavyBaseline:
        errors.append(
            "G-C2 读 >=%d 个配置成员的文件增至 %d 个（基线 %d）:"
            % (heavyThreshold, len(heavy), heavyBaseline)
            + "".join("\n    %3d  %s" % (len(byFile[path]), path) for path in heavy)
            + "\n    配置消费面正在扩散。要么把新增的消费收敛，"
            + "\n    要么在有理由时把基线调高并写明为什么。"
        )
    return errors


def SelfTest() -> int:
    """判定逻辑的自检：不读仓库，只喂构造数据。"""
    baseline = {"deadMemberAllowlist": [], "heavyThreshold": 3, "heavyFileBaseline": 1}
    cases = [
        ("零消费者应报错", {"a": [], "b": ["x.cpp"]}, {"x.cpp": ["b"]}, baseline, True),
        ("只出现在 config.cpp 也算零消费者",
         {"a": [CONFIG_SOURCE], "b": ["x.cpp"]}, {"x.cpp": ["b"]}, baseline, True),
        ("都有消费者则通过", {"a": ["y.cpp"], "b": ["x.cpp"]},
         {"x.cpp": ["b"], "y.cpp": ["a"]}, baseline, False),
        ("登记进允许清单后放行", {"a": [], "b": ["x.cpp"]}, {"x.cpp": ["b"]},
         {**baseline, "deadMemberAllowlist": ["a"]}, False),
        ("过期的允许清单条目应报错", {"a": ["y.cpp"], "b": ["x.cpp"]},
         {"x.cpp": ["b"], "y.cpp": ["a"]},
         {**baseline, "deadMemberAllowlist": ["a"]}, True),
        ("重度文件超基线应报错", {"a": ["x.cpp"], "b": ["x.cpp"], "c": ["x.cpp"]},
         {"x.cpp": ["a", "b", "c"], "y.cpp": ["a", "b", "c"]}, baseline, True),
    ]
    failed = 0
    for name, byMember, byFile, base, expectError in cases:
        errors = EvaluateGates(byMember, byFile, base)
        ok = bool(errors) == expectError
        print("  %s  %s" % ("PASS" if ok else "FAIL", name))
        if not ok:
            failed += 1
            print("      得到: %s" % (errors or "无错误"))
    print("self-test: %s" % ("PASS" if failed == 0 else "FAIL (%d)" % failed))
    return 1 if failed else 0


def Main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the SliceConfig consumption surface (R-13 / F-10).")
    parser.add_argument("--list", action="store_true", help="打印消费图后退出")
    parser.add_argument("--self-test", action="store_true", help="只自检判定逻辑")
    arguments = parser.parse_args()

    if arguments.self_test:
        return SelfTest()

    members = CollectTopLevelMembers(ReadText(CONFIG_HEADER))
    files = CollectSourceFiles()
    byMember, byFile = BuildConsumptionMap(members, files)
    baseline = json.loads(ReadText(BASELINE_PATH))

    if arguments.list:
        print("SliceConfig 消费图（%d 个顶层成员 / %d 个消费文件）" % (
            len(members), len(byFile)))
        print("匹配器按 %s 等别名做文本匹配，偏窄：匹配到必为真消费。" % ", ".join(
            "`%s.`" % alias for alias in CONFIG_ALIASES[:2]))
        print()
        for member in members:
            consumers = [path for path in byMember[member] if path != CONFIG_SOURCE]
            print("%-26s %3d 个消费者" % (member, len(consumers)))
            for path in consumers:
                print("      %s" % path)
        print()
        print("按文件（读得最多的在前）:")
        for path in sorted(byFile, key=lambda p: -len(byFile[p])):
            print("  %3d  %s" % (len(byFile[path]), path))
        return 0

    errors = EvaluateGates(byMember, byFile, baseline)
    heavy = [
        path for path, seen in byFile.items()
        if len(seen) >= int(baseline["heavyThreshold"])
    ]
    if errors:
        for message in errors:
            print("ERROR: %s" % message)
        print("SliceConfig consumption guard: FAIL")
        return 1
    print(
        "SliceConfig consumption guard: PASS "
        "(members=%d, consumers=%d, heavy=%d/%d)"
        % (len(members), len(byFile), len(heavy),
           int(baseline["heavyFileBaseline"])))
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
