#!/usr/bin/env python3

"""能力集的四处权威声明必须彼此一致（R-10）。

**这条门禁要解决的是什么。** 能力集被钉在多处：契约 DTO、`module_info` schema 的
三个 `const`、生成 `module.json` 的模板。P0-02 加 `slice.rgbwsvt` 时，这些地方是
**一次一个红灯**被发现的——改完一处跑一次，红了才知道还有下一处，范围估计从 6 个文件
涨到 10 个。没有任何一处告诉你「一共要改几处」。

本脚本一次读完全部权威声明，**把所有不一致一起报出来**，而不是停在第一条。
下一次增删能力时，它给出的是完整的待改清单。

**不做代码生成。** 曾考虑由单一真源生成其余产物，但那要改构建流程、动 `module.json.in`
的生成链，风险远大于收益——而实际痛点是「不知道要改几处」，交叉校验就能解决。

**已知的合法重叠**：`geometry.preflight` 同时属于 `syncCapabilities` 与
`workerCapabilities`（它两条通道都有），故断言的是并集相等而非两者不相交。
"""

import io
import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

DTOS = "contracts/slicer_capability_dtos.json"
INFO_SCHEMA = "contracts/slicer_module_info.schema.json"
TEMPLATE = "src/slicer_module/module.json.in"


def LoadJson(relative: str):
    path = REPO / relative
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def TemplateArray(text: str, key: str) -> str:
    """取模板里某个数组的原文（模板含 @VAR@ 占位符，不能直接当 JSON 解析）。"""
    match = re.search(r'"' + key + r'"\s*:\s*\[(.*?)\]', text, re.DOTALL)
    if match is None:
        raise AssertionError(f"{TEMPLATE} 里找不到 {key} 数组")
    return match.group(1)


def Main() -> int:
    dtos = LoadJson(DTOS)
    info = LoadJson(INFO_SCHEMA)
    template = (REPO / TEMPLATE).read_text(encoding="utf-8", errors="replace")

    dtoIds = [entry["id"] for entry in dtos["capabilities"]]
    provides = info["properties"]["provides"]["const"]
    syncCapabilities = (
        info["properties"]["capabilities"]["properties"]["syncCapabilities"]["const"])
    workerCapabilities = (
        info["properties"]["capabilities"]["properties"]["workerCapabilities"]["const"])
    producesSchema = info["properties"]["produces"]["const"]
    templateProvides = re.findall(r'"([^"]+)"', TemplateArray(template, "provides"))

    problems: list[str] = []

    # 1) DTO 契约与 schema 的 provides 必须是同一个集合，且都不含重复。
    if len(set(dtoIds)) != len(dtoIds):
        duplicates = sorted({x for x in dtoIds if dtoIds.count(x) > 1})
        problems.append(f"{DTOS} 的 capabilities[].id 有重复：{duplicates}")
    if len(set(provides)) != len(provides):
        duplicates = sorted({x for x in provides if provides.count(x) > 1})
        problems.append(f"{INFO_SCHEMA} 的 provides const 有重复：{duplicates}")
    if set(dtoIds) != set(provides):
        problems.append(
            f"DTO 契约与 module_info schema 的能力集不一致："
            f"仅在 DTO={sorted(set(dtoIds) - set(provides))}，"
            f"仅在 schema={sorted(set(provides) - set(dtoIds))}")

    # 2) 生成 module.json 的模板必须与 schema 一致——否则运行时自述与契约对不上。
    if set(templateProvides) != set(provides):
        problems.append(
            f"{TEMPLATE} 的 provides 与 schema 不一致："
            f"仅在模板={sorted(set(templateProvides) - set(provides))}，"
            f"仅在 schema={sorted(set(provides) - set(templateProvides))}")

    # 3) 同步通道 ∪ Worker 通道必须恰好覆盖 provides。
    #    允许交集（geometry.preflight 两条通道都有），但并集必须严格相等——
    #    少了说明某个能力没有分配通道，多了说明分配了不存在的能力。
    union = set(syncCapabilities) | set(workerCapabilities)
    if union != set(provides):
        problems.append(
            f"syncCapabilities ∪ workerCapabilities 未覆盖 provides："
            f"多出={sorted(union - set(provides))}，"
            f"缺失={sorted(set(provides) - union)}")

    # 4) produces（包协议）在 schema 与模板之间必须一致。
    templateProduces = TemplateArray(template, "produces")
    for entry in producesSchema:
        contract = entry["contract"]
        if f'"{contract}"' not in templateProduces:
            problems.append(f"{TEMPLATE} 的 produces 缺少 schema 声明的包协议 {contract}")
    for contract in re.findall(r'"contract"\s*:\s*"([^"]+)"', templateProduces):
        if contract not in [e["contract"] for e in producesSchema]:
            problems.append(
                f"{TEMPLATE} 的 produces 多出 schema 未声明的包协议 {contract}")

    if problems:
        raise AssertionError(
            "能力集的权威声明彼此不一致（全部列出，不止第一条）：\n  "
            + "\n  ".join(problems))

    print(
        f"capability registry contract: PASS "
        f"({len(provides)} capabilities, "
        f"{len(syncCapabilities)} sync / {len(workerCapabilities)} worker, "
        f"{len(producesSchema)} package protocols)")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
