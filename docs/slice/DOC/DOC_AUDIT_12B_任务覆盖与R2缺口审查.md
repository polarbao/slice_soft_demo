# DOC_AUDIT_12B 任务覆盖与 R2 缺口审查

> 文档状态：Audit / Stage 12B
> 日期：2026-07-08
> 审查范围：`TASKS_12B_切片引擎性能与OpenVDB替代任务清单.md`、R0/R1/R2 文档包

## 1. 审查结论

当前 12B 主任务已经被 R0/R1/R2 三段全部覆盖，但 12B 整体尚未完成。

状态分解：

```text
12B-01 到 12B-04：由 R0 覆盖并完成；
12B-05 到 12B-06：由 R1 覆盖并完成；
12B-07：由 R2 覆盖，当前进行中，R2-00 到 R2-04 已完成。
```

因此：

```text
覆盖度：已覆盖 7/7；
完成度：已完成 6/7，12B-07 仍在执行；
当前阻塞点：R2 尚未完成 capability matrix、最小 utility report 和最终状态报告。
```

## 2. 12B 主任务覆盖矩阵

| 主任务 | 目标 | 覆盖阶段 | 当前状态 | 证据 |
|---|---|---|---|---|
| 12B-01 Benchmark 契约确认 | 固定 coreComputeMs / ioWriteMs / previewWriteMs / endToEndMs 边界 | R0 | DONE | `DOC_SCHEMA_12B_CoreBenchmarkReport.md` |
| 12B-02 same-pose benchmark 配置 | legacy/openvdb 同模型、同姿态、同 grid、同 layerThickness | R0 | DONE | `REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md` |
| 12B-03 Release core-only 脚本 | 建立 Release core-only benchmark 脚本 | R0 | DONE | `scripts/run_12b_core_benchmark.ps1`、R0 report |
| 12B-04 OpenVDB 语义可比性报告 | 输出 outputSemanticsComparable=false 原因 | R0 | DONE | R0 replacement gate 结论 |
| 12B-05 Legacy 优化候选实验 | 至少评估一个低风险 legacy 优化 | R1 | DONE | `REPORT_12B_R1_LegacyHeightfield优化当前状态.md` |
| 12B-06 Heightfield Fast Path 预研 | 判断 2.5D heightfield fast path 是否继续 | R1 | DONE | `DOC_ANALYSIS_12B_R1_2_5DHeightfieldFastPath可行性评估.md` |
| 12B-07 OpenVDB Hybrid 定位 | 判断 OpenVDB 是否改为 SDF utility | R2 | IN_PROGRESS | R2 PRD/DEV/DEMO/TASKS/schema/audit |

## 3. R0 完成覆盖

R0 覆盖原始 12B 的 benchmark 和 replacement gate 问题。

已完成：

```text
1. 固化 slicesoft.benchmark.12b.1；
2. 固定 same-pose / same-resolution / same-semantics 规则；
3. 建立真实模型 Release core-only benchmark；
4. 输出 legacy 与 OpenVDB candidate 对比；
5. 输出 OpenVDB replacementPass=false；
6. 明确 OpenVDB 不能替代 legacy production slicer。
```

R0 不再继续的内容：

```text
OpenVDB production replacement。
```

原因：

```text
outputSemanticsComparable=false；
OpenVDB candidate 不覆盖 12A/12D production RGBWSV 材料语义；
OpenVDB Release lane 当时不可作为可复现 production path。
```

## 4. R1 完成覆盖

R1 覆盖 legacy 优化和 heightfield fast path 可行性。

已完成：

```text
1. 引入 coarse SliceRunProfile；
2. 对真实模型做 Release profile baseline；
3. 完成 support.shape disabled fast path before/after；
4. 判断当前 relief_heightfield 已经是 column z_min/z_max 路径；
5. 明确不在 R1 新增独立 2.5D fast path。
```

R1 结论：

```text
继续优化 legacy 时，应优先关注支撑生成、材料组合和报告/写盘边界；
heightfield fast path 不是当前最高收益方向。
```

## 5. R2 当前覆盖

R2 覆盖 12B-07 OpenVDB Hybrid 定位。

已完成：

```text
1. R2 文档准入；
2. R2 阶段启动报告；
3. OpenVDB utility 当前代码盘点；
4. OpenVDB SDF utility report schema；
5. USE_OPENVDB=OFF 默认构建、UI self-test、legacy benchmark 和现有 unavailable diagnostic guard；
6. USE_OPENVDB=ON smoke 与可用性报告。
```

当前定位：

```text
OpenVDB 不作为 production slicer replacement；
OpenVDB 只作为 optional / disabled-by-default 的 SDF utility 候选；
R2 评估 outer varnish shell offset、clearance distance、topology diagnostic、material closure assist。
```

## 6. R2 剩余缺口

R2 尚未完成：

| R2 任务 | 当前状态 | 缺口 |
|---|---|---|
| R2-03 OpenVDB OFF 默认轨道保护 | DONE | 已验证默认 OFF build、UI self-test、legacy benchmark 和现有 unavailable diagnostic |
| R2-04 OpenVDB ON Smoke 与可用性报告 | DONE | 已验证 `build-openvdb-09p` OpenVDB ON smoke |
| R2-05 Utility Capability Matrix | PENDING | 需要四类 utility 的 promote / keep_experimental / reject 结论 |
| R2-06 最小 Utility Report 原型 | PENDING | 需要 report 原型或 unavailable report，不写 production TIFF |
| R2-07 R2 当前状态报告 | PENDING | 需要收口 Current/Target/Historical/Pending Confirmation |

## 7. 是否存在未覆盖主任务

判断：

```text
不存在未覆盖的 12B 主任务。
```

但存在未完成的 R2 子任务：

```text
12B-07 尚未完成；
R2 需要继续执行 R2-03 到 R2-07。
```

## 8. 下一步处理建议

推荐顺序：

```text
1. R2-05：在真实可用证据基础上完成 utility capability matrix；
2. R2-06：实现或补齐最小 utility report；
3. R2-07：输出 R2 当前状态报告并决定后续阶段。
```

理由：

```text
先保 production 默认轨道，再评估可选 OpenVDB；
先有 schema，再有 report；
先有 OFF/ON 证据，再写 promoteDecision。
```
