# DOC_EXEC_12E-08C-R4-07-R2 受限生产候选预算冻结结果

> 文档状态：COMPLETE / CANDIDATE BUDGET FROZEN PASS
> 完成时间：2026-07-23
> 任务性质：参考机器工程候选预算；非正式产品 SLA；非生产写包授权

## 1. 结论

R4-07-R2 已完成。参考机器、MSVC Release、OpenVDB OFF、`legacy_cpu_global_distance`、`voxelMm=0.20`
条件下，xiao_ma/yecan/Texture2D 控制组四个 case 各完成 1 次预热和 5 次正式测量，时间与峰值内存均低于
版本化预算 `2026-07-23.r1`。

```text
budget.status=frozen_pass
result.budgetGatePass=true
result.productionAdmission=not_evaluated
diagnosticOnly=true
productionOutputWritten=false
```

本结果移除 `release_budget_not_frozen` blocker，但 Quick CI baseline 和 08D 独立授权仍未完成，因此
12E-08D 继续保持 `NOT READY / NO-GO`。

## 2. 实现内容

新增：

```text
tests/golden/expected/12e_r4_07_r2_candidate_budget_policy.json
scripts/run_12e_08c_r4_07_r2_candidate_budget.ps1
docs/slice/DOC/DOC_DECISION_12E_08C_R4_07_R2_受限生产候选预算冻结规则.md
docs/slice/DOC/DOC_PREP_12E_08C_R4_07_R2_受限生产候选预算冻结准备.md
```

runner 执行以下校验：

```text
R4-07-R1 候选身份和 source/resource hash；
CPU/机器/内存下限/CMake generator/MSVC 版本/架构；
Release/OpenVDB OFF/backend/voxel；
四 case 身份与每 case 至少 5 次正式测量；
median、single-run max、peak working set 三类预算；
diagnostic-only、no production output 和 no production admission。
```

输出：

```text
output/benchmarks/12e_08c_r4_07_r2_candidate_budget/candidate_budget_summary.json
schema=slicesoft.r4_restricted_candidate_budget.12e_08c_r4.1
```

## 3. 实测结果

| Case | Samples | Median ms | Max ms | Peak bytes | Budget result |
|---|---:|---:|---:|---:|---|
| `development_xiao_ma_minimum` | 5 | 328.1570 | 331.5887 | 26,632,192 | PASS |
| `development_xiao_ma_all_texture` | 5 | 402.9719 | 433.5741 | 27,348,992 | PASS |
| `development_yecan_intermediate` | 5 | 473.2710 | 479.5123 | 31,457,280 | PASS |
| `texture2d_3mf_control` | 5 | 16.7596 | 17.3148 | 7,323,648 | PASS |

本表的 `globalCoreMs` 不包含 TIFF、PNG、JSON 写盘。测量值不应与 UI 完整切片墙钟时间混用。

## 4. 环境指纹

```text
Machine：LENOVO 21LD
CPU：Intel(R) Core(TM) Ultra 5 125H / 14 cores / 18 logical processors
Memory：33,945,935,872 bytes
OS：Windows 11 10.0.26200
CMake generator：Visual Studio 18 2026
Compiler：MSVC 19.51.36248.0 / x64
Build：Release
USE_OPENVDB：OFF
Backend：legacy_cpu_global_distance
voxelMm：0.20
```

编译器、CPU、模型 hash 或测量口径变化时，必须新建 policy 版本，不能静默复用本预算。

## 5. 实际验证

已运行：

```powershell
cmake --build build --config Release --target repaired_asset_intake texture_fill_partition_positive_matrix texture_fill_partition_release_benchmark
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r4_07_r2_candidate_budget.ps1 -BuildDir build -Config Release -SkipBuild
```

结果：

```text
Release targets：PASS；
R4-07 定向 CTest：3/3 PASS；
positive matrix：3/3 PASS；
candidate budget：4/4 PASS；
productionAdmission：not_evaluated。
```

负向验证复用了正向 measurement summary，并把
`development_xiao_ma_minimum.singleRunCoreMsMax` 临时收紧为 `1 ms`：

```text
脚本退出码=1；
超预算 case 被明确拒绝；
负向 Gate=PASS。
```

## 6. 边界与下一任务

未修改：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
legacy 默认生产路径；
OpenVDB 默认关闭；
global_surface_shell diagnostic-only；
生产 writer。
```

下一原子任务：

```text
Quick-CI-R1：归因并处理 material_process_top2 golden baseline；
R4-08-R2：在预算与 CI 证据齐全后刷新 GO/NO-GO。
```

复杂浮雕 `aishen/meigui/titian` 的 `0/3` 继续记录为覆盖缺口，不因本预算 PASS 被改判。
