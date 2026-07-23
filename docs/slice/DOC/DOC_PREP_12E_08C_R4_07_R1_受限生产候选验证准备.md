# DOC_PREP_12E-08C-R4-07-R1 受限生产候选验证准备

> 文档状态：READY FOR DEVELOPMENT
> 准备完成时间：2026-07-23 11:32 +08:00
> 前置决策：`DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md`
> 任务性质：Release/closure/performance 证据自动化；不写 global production package

## 1. 目标

把已经通过 R4-06 intake 和 R4-07 development four-case 的两个独立真实模型族，提升为机器可读的
“受限生产候选证据”。本任务只验证候选范围是否足以进入预算和 CI 收口，不授予生产写包权限。

## 2. 输入身份

| 角色 | 模型族 | 输入 | 版本状态 |
|---|---|---|---|
| Primary | `xiao_ma_wu_yu_new` | `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | Git tracked |
| Independent | `yecan` | `model/obj/yecan/3.obj` | Git tracked |
| 3MF control | 不计真实模型族 | `samples/models/3mf/texture2d_checker_cube.3mf` | Git tracked |

`model/obj/yecan/4.obj` 可作本机扩展，但在来源冻结前不作为必跑基线。

## 3. 原子任务边界

### R4-07-R1

```text
为 development intake evidence 增加显式 modelFamilyId；
新增受限生产候选 expectations；
新增 validator/runner；
校验两个独立 admitted 模型族；
校验 minimum/intermediate/allTexture 与 Texture2D 控制组；
校验 Release 三次测量、full closure、legacy TIFF/RIP；
输出机器可读候选摘要；
保持 productionOutputWritten=false。
```

### R4-07-R2

```text
基于 R4-07-R1 当前机器、编译器、模型 hash 和重复测量，冻结受限候选时间/内存预算；
预算阈值必须有依据和版本；
不得在 R4-07-R1 中自动用测量最大值生成阈值。
```

### R4-08-R2

```text
解决或正式隔离 Quick CI baseline；
汇总候选预算、legacy/RIP、协议和用户授权；
重新输出 08D GO/NO-GO。
```

## 4. 机器可读输出

R4-07-R1 输出：

```text
schema=slicesoft.r4_restricted_production_candidate.12e_08c_r4.1
scope=restricted_production_candidate
diagnosticOnly=true
productionOutputWritten=false
productionAdmission=not_evaluated
```

必需字段：

```text
generatedAt；
sourceEvidence；
admission.minimumIndependentModelFamilies；
admission.admittedIndependentModelFamilies；
admission.candidates[]；
fourCase.requiredCount/passedCount/pass；
releaseMeasurement.sampleCountPerCase；
releaseMeasurement.observedGlobalCoreMaxMs；
releaseMeasurement.observedPeakWorkingSetMaxBytes；
legacyRegression；
remainingBlockers[]；
result。
```

`productionAdmission` 在本任务中不得为 `passed`。

## 5. 验证规则

```text
development intake schema 必须匹配；
four-case schema 必须匹配；
两个 required candidate 都必须 admitted；
两个 candidate 的 modelFamilyId 必须不同；
source/resource/geometry/attribute/audit hash 不得为空；
four-case 必须恰好包含 expectations 中的 4 个身份；
每个 case pass=true；
每个 case 至少 3 次 Release measurement；
globalCoreMs > 0；
peakWorkingSetBytes > 0；
legacyRegression=passed；
diagnosticOnly=true；
productionOutputWritten=false。
```

## 6. 停止条件

出现以下任一情况立即失败：

```text
只有一个模型族；
候选未 admitted 或 hash 缺失；
候选路径不在冻结清单；
四用例缺失或 closure 失败；
Release 样本不足；
legacy TIFF/RIP 未通过；
要求写 global TIFF/package；
要求把当前测量自动视为正式预算；
要求忽略 Quick CI blocker。
```

## 7. 验证命令

```powershell
cmake --build build --config Release --target repaired_asset_intake repaired_asset_intake_unit_tests texture_fill_partition_positive_matrix texture_fill_partition_release_benchmark
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1 -BuildDir build -Config Release -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_development_gate.ps1 -BuildDir build -Config Release -SkipBuild -ReuseIntakeEvidence
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_restricted_candidate_gate.ps1
git diff --check
```

## 8. 准备结论

输入身份、schema、验收规则、停止条件、输出边界和验证命令已经完整。R4-07-R1 可进入开发。
R4-07-R2、Quick CI baseline 和 R4-08-R2 仍是后续独立原子任务。
