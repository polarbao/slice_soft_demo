# DOC_PREP_12E-08C-R4-07-R2 受限生产候选预算冻结准备

> 文档状态：READY FOR DEVELOPMENT
> 准备完成时间：2026-07-23
> 前置任务：R4-07-R1 COMPLETE / 2 families / 4 cases PASS
> 前置决策：`DOC_DECISION_12E_08C_R4_07_R2_受限生产候选预算冻结规则.md`

## 1. 目标与范围

本任务把 R4-07-R1 的“已测量、未冻结”状态提升为可重复执行的候选工程预算 Gate。只处理 Release
core-only 时间和进程峰值内存，不优化算法，不写 global production package，也不修改生产 TIFF。

## 2. 输入

```text
R1 candidate evidence：
  output/benchmarks/12e_08c_r4_07_restricted_candidate/restricted_candidate_summary.json

Frozen policy：
  tests/golden/expected/12e_r4_07_r2_candidate_budget_policy.json

Measurement producer：
  scripts/run_12e_08c_r4_07_development_gate.ps1

Models：
  model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj
  model/obj/yecan/3.obj
  samples/models/3mf/texture2d_checker_cube.3mf
```

模型身份以 R4-06/R4-07 证据中的 `sourceHash/resourceHash` 为准。工作树中未跟踪模型不得进入本 Gate。

## 3. 实现拆分

```text
1. 新增版本化预算 policy，冻结环境、模型身份、测量口径和四 case 阈值；
2. 新增 R4-07-R2 runner/validator；
3. runner 默认执行 1 次预热 + 5 次正式 Release 测量；
4. runner 校验环境指纹、模型 hash、case 身份和阈值；
5. 输出机器可读 budget summary；
6. 用人为收紧阈值的临时 policy 验证超限时返回非零退出码。
```

## 4. 输出 schema

```text
schema=slicesoft.r4_restricted_candidate_budget.12e_08c_r4.1
stage=12E-08C-R4-07-R2
scope=reference_machine_engineering_candidate
diagnosticOnly=true
productionOutputWritten=false
environment.identityPass
sourceEvidence.*
budget.policyVersion
budget.minimumSamplesPerCase
budget.cases[]
budget.status=frozen_pass
result.budgetGatePass=true
result.productionAdmission=not_evaluated
result.nextTask=Quick-CI-R1
remainingBlockers=[
  quick_ci_baseline_unresolved,
  explicit_08d_authorization_missing
]
```

## 5. 停止条件

```text
当前机器或编译器与 policy 不匹配；
模型 source/resource hash 变化；
不是 Release 或 USE_OPENVDB 不为 OFF；
四 case 缺失、样本少于 5 或输出写盘计入 core；
任何 median/max/memory 超预算；
要求把候选预算表述为正式产品 SLA；
要求据此直接写 global production package。
```

## 6. 验证命令

```powershell
cmake --build build --config Release --target repaired_asset_intake texture_fill_partition_positive_matrix texture_fill_partition_release_benchmark
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_r2_candidate_budget.ps1 -BuildDir build -Config Release -SkipBuild

# 负向验证：复制 policy 后把任一 singleRunCoreMsMax 改为 1，
# 使用 -MeasurementSummaryPath 复用正向测量，脚本必须返回非零退出码。

git diff --check
git status --short
```

## 7. 后续任务准备要求

R4-07-R2 完成后只允许进入：

```text
Quick-CI-R1：归因并处理 material_process_top2 golden baseline；
R4-08-R2：汇总候选预算、CI、legacy/RIP、协议和独立授权状态。
```

在 Quick-CI-R1 和 R4-08-R2 未完成前，12E-08D 保持 `NOT READY / NO-GO`。
