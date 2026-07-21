# DOC_PREP_12E-08C-R3 Real Model 与 Release Gate 准备

> 文档状态：IN PROGRESS / R3-01 COMPLETE / R3-01A READY
> 日期：2026-07-20

## 1. 准备结论

R3 的真实模型模式分类、repair matrix、Release 核心预算和 12E-08D GO/NO-GO 证据已拆分。R3 不扩大
R2 修复范围，不把 `manual_repair_required` 解释为 production PASS。

R1-04 新增事实：三个 required OBJ 的当前自相交诊断均达到 triangle-pair 采样上限。因此 R3 在真实模型
Repair Matrix 前新增 R3-01A 完整自相交证据，准备入口为
`DOC_PREP_12E_08C_R3_01A_完整自相交证据准备.md`。

R3-01 已完成只读的 non-manifold pattern classifier，真实模型分类结果与重复性证据见
`DOC_EXEC_12E_08C_R3_01_NonManifoldPatternClassifier结果.md`。当前下一允许的原子任务为 R3-01A；
R3-02 及之后任务继续阻断。

## 2. R3-01 Non-Manifold Pattern Classifier

状态：COMPLETE / NON-PRODUCTION。

分类对象至少包括：

```text
duplicate shell/exporter duplicate；
separable local edge fan；
overlapping component；
mixed winding fan；
attribute-conflicting fan；
unclassified non-manifold pattern。
```

Pattern classifier 只描述结构并评估唯一 fan split 可行性。未证明唯一、属性可保持的 pattern 进入
`manual_repair_required`，不得批量猜测修复 `meigui_fudiao`。

## 3. R3-01A 完整自相交证据

使用确定性 broad-phase 完整枚举 required real model 候选 pair，并复用当前 triangle intersection
narrow-phase。sampled 不得计为 strict PASS；confirmed/coplanar blocker 继续 fail-fast 或 blocked。

## 4. R3-02 真实模型 Repair Matrix

每个 required case 分别运行 repair disabled/enabled，并记录：

```text
input/config/hash；
pre issue pattern；
eligibility 和 operations；
attribute preservation；
post strict；
partition/texture/raster/full closure；
最终专项状态与 production Gate 状态。
```

专项允许 no-op、repaired、manual、rejected；12E-08D Gate 只接受 required case 的 strict PASS。

## 5. R3-03 Release Core 与 Legacy Regression

Release 计时必须分离：

```text
import/transform；
pre diagnostics/eligibility；
repair/attribute/post strict；
partition/texture transfer；
raster/full closure；
TIFF/PNG/JSON 写盘。
```

核心预算只统计前五类相关计算，写盘单列。记录 peak working set、网格规模和重复运行离散度。预算阈值必须
根据真实结果冻结，不能在准备文档中虚构。

Legacy 回归至少包含旧 Profile、RIP strict、repair-disabled TIFF SHA-256 invariant、默认 OpenVDB OFF
build/CTest。R3 不执行 global production write。

## 6. R3-04 GO/NO-GO

只有以下条件全部满足才能建议 12E-08D GO：

```text
required real models post-repair strict PASS；
attribute preservation PASS；
12E partition/texture/raster/full closure PASS；
Release runtime/peak memory budget PASS；
legacy/RIP/TIFF/protocol regression PASS；
用户再次确认 production-path change。
```

任一失败输出 NO-GO 和稳定 blocker。GO 只表示 08D 可开始，不表示 global 已生产准入。

## 7. 证据与输出

```text
mesh_repair_report；
texture_fill_partition_report；
12E Release matrix JSON；
legacy regression summary；
R3 GO/NO-GO decision/report；
AI handoff 和文档索引更新。
```

## 8. 预计脚本边界

优先扩展现有 `run_12e_08c_release_evidence.ps1` 的显式 repair lane，或新增只生成诊断证据的 R3 脚本。
脚本不得写 12E production TIFF；repair-disabled legacy TIFF 仅用于不变量回归。

## 9. 停止条件

required-case matrix 需要变更、预算需豁免、属性 provenance 无法保持、修复需第三方库或 global writer 需要
提前接入时停止，分别进入正式决策，不得在 R3 中顺手放宽。
