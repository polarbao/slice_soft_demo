# ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划

> 文档版本：v0.1
> 文档状态：Formal Roadmap / Project Operating Plan
> 生成日期：2026-07-01
> 当前阶段：Stage 10 已完成，当前执行 11 UI 切片层预览、交互配置与多模型能力评估

---

## 1. 使用方式

本计划用于项目运行排布，不替代阶段 PRD / DEV / TASKS。

时间范围是建议节奏，实际进度以人员、真实模型、OpenVDB 环境和验证结果为准。

---

## 2. 短期计划

建议范围：2026-07 到 2026-08。

目标：

```text
完成文档真源收束；
完成 09P-R2 hardening；
明确是否需要 mesh repair / admission gate 专项；
执行 10 输出契约、layer summary、texture fidelity 和下游 handoff；
执行 11 UI layer preview、interactive settings 和 multi-model capability decision；
保持 legacy slicer_cli 和 RGBWSV 协议稳定。
```

重点任务：

| 序号 | 任务 | 交付物 | 退出标准 |
|---:|---|---|---|
| S1 | 文档真源收束 | 00-08 / 09P-R2 / 10 / 11 文档入口 | README、DOC_INDEX、codex_task 入口可直接定位 |
| S2 | 09P-R2 report schema | `p0.experimental_openvdb_shell_cli_report.1` 文档和验证 | schema 字段稳定，缺字段有补充任务 |
| S3 | topology admission gate | blocker/warning matrix | strict_closed / warn_and_attempt / diagnostic_only 行为清楚 |
| S4 | mesh repair 前置判断 | DOC_DECISION 或 matrix | 明确哪些 issue 可 repair、哪些必须 reject |
| S5 | service data contract | OpenVDB / texture / composer DTO 说明 | ValidationIssue、stats、timing、memory 传播规则清楚 |
| S6 | OpenVDB OFF / ON CI matrix | CI matrix 脚本或方案 | OFF 默认可跑，ON 环境条件清楚 |
| S7 | 阶段报告 | `REPORT_09P_R2` | 是否进入 09P-R3 或 mesh repair 专项有结论 |
| S8 | 10 输出契约 | output contract、layer summary、texture fidelity、handoff checklist | 已完成，见 `REPORT_10` |
| S9 | 11 UI layer preview | layer preview contract、slider、channel switch、config panel、multi-model decision | UI 可按层查看关键通道，多模型范围有结论 |

短期不做：

```text
不实现 RIP；
不实现设备通信；
不默认启用 OpenVDB；
不展开完整多模型 production 切片；
不大规模重写 slicer.cpp / model.cpp。
```

---

## 3. 中期计划

建议范围：2026-09 到 2026-12。

目标：

```text
完成 production candidate 所需的输出契约、真实模型验收、UI 可视化和配置交互。
```

重点任务：

| 序号 | 任务 | 交付物 | 退出标准 |
|---:|---|---|---|
| M1 | mesh repair / admission gate 专项决策 | repair_then_strict 设计或延期决策 | 不再把 blocker 模型误标 production-safe |
| M2 | 09P-R3 UI/report/profile 工程化 | UI 读取 report/profile/admission | UI 不依赖 OpenVDB 内部类型 |
| M3 | 09P-R4 production candidate gate | 真实模型集合、性能/内存门槛 | productionAllowed release gate 清楚 |
| M4 | 10 输出契约 | output contract、layer summary、texture fidelity | 下游 RIP 工程师可消费 package/report |
| M5 | 11 UI layer preview | layer slider、pseudo color、channel switch | UI 可按层查看关键通道 |
| M6 | 11 interactive settings | 配置面板、validator、normalized config | 常用配置不再只能手改文件 |
| M7 | 11 multi-model decision | DOC_DECISION / scene model | 多模型是否进入后续 production 有结论 |

中期决策点：

```text
是否允许 experimental OpenVDB 路径进入 production candidate；
是否需要独立 mesh repair 阶段；
多模型是否只做 scene/UI 能力，还是进入后续 production 切片；
10 输出契约是否满足下游 RIP 工程团队需求。
```

---

## 4. 长期计划

建议范围：2027 及以后。

目标：

```text
从正式候选切片软件演进为稳定工程产品。
```

长期方向：

| 序号 | 方向 | 交付物 | 退出标准 |
|---:|---|---|---|
| L1 | 批处理作业与作业队列 | job model / queue / retry policy | 多作业可追踪、可取消、可复现 |
| L2 | 多模型 build volume | placement / collision / nesting | 多模型 production 范围明确 |
| L3 | 材料 profile 管理 | profile versioning / migration | 材料参数可版本化、可回滚 |
| L4 | 真实模型 release gate | curated real-model suite | 每次 release 有固定模型集合验收 |
| L5 | 性能和大模型 | memory/cache/benchmark | 大模型耗时和内存有门槛 |
| L6 | 下游 handoff 协议 | package contract feedback loop | RIP 团队反馈能进入 issue/report |
| L7 | 安装和运维 | installer / logs / diagnostics | 工程交付可安装、可诊断 |
| L8 | 用户和维护文档 | user guide / maintainer guide | 非开发者可运行常见流程 |

长期仍不默认包含：

```text
RIP 半色调实现；
设备通信；
喷头 bitstream；
生产硬件控制。
```

这些由外部专门团队负责，本项目只保持清晰输出契约和必要的下游反馈接口。

---

## 5. 阶段门槛

| 阶段 | 进入条件 | 退出条件 |
|---|---|---|
| 09P-R2 | 09P-R1 完成，文档入口清楚 | REPORT_09P_R2，schema/gate/contract/CI 清楚 |
| Mesh Repair 专项 | 真实模型被 topology blocker 阻断 | repair_then_strict 策略和复验规则清楚 |
| 09P-R3 | report schema 稳定 | UI 能展示 report/profile/admission |
| 09P-R4 | 真实模型集合准备好 | production candidate gate 清楚 |
| 10 | 输出协议和纹理链路稳定 | output contract / texture fidelity / handoff checklist 完成 |
| 11 | UI 基础和输出契约稳定 | layer preview / config panel / multi-model decision 完成 |

---

## 6. 运行节奏

建议项目按固定节奏运行：

| 节奏 | 内容 | 产出 |
|---|---|---|
| 每个任务开始 | 检查 `git status --short`、确认任务边界 | 当前任务范围 |
| 每个任务结束 | 运行指定验证、记录未验证项 | task summary |
| 每个阶段开始 | PRD / DEV / DEMO / TASKS / CODEX_PROMPT 齐备 | 阶段入口 |
| 每个阶段结束 | REPORT + 是否进入下一阶段判断 | 阶段状态 |
| 每月 | 更新短中长期计划 | ROADMAP 修订 |
