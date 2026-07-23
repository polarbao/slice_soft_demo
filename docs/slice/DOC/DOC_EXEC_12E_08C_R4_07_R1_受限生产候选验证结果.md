# DOC_EXEC_12E-08C-R4-07-R1 受限生产候选验证结果

> 文档状态：COMPLETE / CANDIDATE EVIDENCE PASS / NON-PRODUCTION
> 执行时间：2026-07-23 11:47 +08:00
> 决策依据：`DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md`
> 生产结论：`productionAdmission=not_evaluated`

## 1. 结论

R4-07-R1 已完成。xiao_ma 与 yecan 两个独立真实模型族均通过 R4-06 intake，受限生产候选四用例
`4/4 PASS`，Release 三次测量、纹理传递、raster/full closure、legacy repair-disabled TIFF invariant 和
RIP strict 均通过。

本任务只证明“受限生产候选证据可继续收口”。预算尚未冻结，Quick CI baseline 尚未解决，12E-08D
尚未获得独立授权，因此不得写 global production package。

## 2. 实现内容

```text
R4-06 development evidence：
  为每个 admitted candidate 增加 modelFamilyId；
  输出 admittedIndependentModelFamilyCount；
  输出 restrictedCandidateIdentityGatePass。

受限候选 expectations：
  冻结 xiao_ma/yecan 两个候选身份；
  冻结 minimum/intermediate/allTexture/Texture2D 四用例；
  要求每 case 至少 3 次 Release 测量；
  要求 legacyRegression=passed。

受限候选 validator：
  校验两个独立模型族，而不是两个同族文件；
  校验 candidate path 和五类 hash；
  校验四用例身份、样本数、计时和内存；
  输出机器可读摘要和剩余 blocker；
  保持 productionOutputWritten=false。
```

## 3. 模型族准入

| Candidate | Model family | Model | Intake |
|---|---|---|---|
| `development_xiao_ma_damuzhi` | `xiao_ma_wu_yu_new` | `MF_Xiao_ma_Damuzhi_ty02.obj` | ADMITTED |
| `development_yecan_3` | `yecan` | `3.obj` | ADMITTED |

```text
minimumIndependentModelFamilies=2；
admittedIndependentModelFamilies=2；
identityGatePass=true。
```

爱神/玫瑰/梯田现有资产继续为 `0/3` complex-relief coverage，不影响本次候选证据 PASS，也没有被
改判为可生产。

## 4. Four-case Release 结果

统一条件：

```text
buildType=Release；
backend=legacy_cpu_global_distance；
USE_OPENVDB=OFF；
voxelMm=0.20；
warm-up=1；
measurementCount=3；
JSON/TIFF/PNG 写盘不计入 globalCoreMs。
```

| Case | Width | Median | Max | Peak working set | 结果 |
|---|---:|---:|---:|---:|---|
| xiao_ma minimum | 0.40 mm | 372.2191 ms | 378.2123 ms | 26,374,144 B | PASS |
| xiao_ma allTexture | 0.46 mm | 435.9968 ms | 496.5500 ms | 26,865,664 B | PASS |
| yecan intermediate | 0.41 mm | 525.3167 ms | 534.8622 ms | 31,137,792 B | PASS |
| Texture2D 3MF control | 0.40 mm | 40.8597 ms | 47.5680 ms | 7,303,168 B | PASS |

本轮观测最大值：

```text
observedGlobalCoreMaxMs=534.8622；
observedPeakWorkingSetMaxBytes=31137792；
budgetStatus=measured_not_frozen。
```

## 5. Legacy 与协议

```text
Repair Disabled TIFF SHA-256 invariant：PASS；
RIP Reader strict：PASS；
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
global production package：未写入。
```

## 6. 正向与负向验证

正向 validator：

```text
R4-07-R1 restricted production candidate evidence: PASS
Independent model families: 2
Production admission: NOT EVALUATED
```

负向 validator 使用只保留 xiao_ma 的临时 intake evidence，稳定返回：

```text
独立 admitted 模型族数量不足
exit=1
```

这证明同一模型族的多个文件不能冒充两个独立候选族。

## 7. 证据与哈希

| 证据 | SHA-256 |
|---|---|
| `development_gate_matrix.json` | `b28f44677fa128e720bb4604749ac1888056e34891d87f2e2ea5575af2e21808` |
| `four_case_development_summary.json` | `3478e7da302631c49f402d820a777109a20baaeb1f3be948238cdbb2d2ae6f76` |
| `restricted_candidate_summary.json` | `a685c432fd7045b3bf08ae3a57aab4bd69c68c0d6ada54e75af6c0fc8677ad3f` |
| `12e_r4_07_restricted_candidate_expectations.json` | `b4d46a12c1a4cc83af2ba90a6ea49e91411d3194305cf41859764fa5d308d86c` |

本地 benchmark 位于 `output/benchmarks`，按仓库规则不提交。

## 8. 实际验证命令

```powershell
cmake --build build --config Release --target repaired_asset_intake repaired_asset_intake_unit_tests texture_fill_partition_positive_matrix texture_fill_partition_positive_matrix_unit_tests texture_fill_partition_release_benchmark texture_fill_partition_release_benchmark_unit_tests
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1 -BuildDir build -Config Release -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_development_gate.ps1 -BuildDir build -Config Release -SkipBuild -ReuseIntakeEvidence
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_restricted_candidate_gate.ps1
```

执行结果：

```text
Release targets：PASS；
R4-06 CTest：1/1 PASS；
R4-07 CTest：3/3 PASS；
R4-06 development intake：2 families / 2 candidates PASS；
R4-07 four-case：4/4 PASS；
legacy TIFF/RIP：PASS；
restricted candidate validator：正向 PASS，单模型族负向 PASS。
```

首次 R4-06 脚本运行暴露 PowerShell `OrderedDictionary` 不能直接使用
`Select-Object -ExpandProperty modelFamilyId`。已改为按字典键读取并重新运行，最终结果通过。

## 9. 剩余 blocker 与下一任务

```text
release_budget_not_frozen；
quick_ci_baseline_unresolved；
explicit_08d_authorization_missing。
```

下一原子任务为 `12E-08C-R4-07-R2`：冻结受限生产候选时间/内存预算。之后还需 Quick-CI-R1 和
R4-08-R2；这些任务完成前 12E-08D 保持 BLOCKED。
