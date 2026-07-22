# DOC_EXEC_12E-08C-R4-06 Repaired Asset Intake 结果

> 文档状态：IMPLEMENTATION COMPLETE / REAL FAMILY MATRIX 0/3 BLOCKED
> 日期：2026-07-22
> 原子任务：12E-08C-R4-06
> 下一任务：R4-07 WAIT REQUIRED FAMILY MATRIX 3/3

## 1. 结论

R4-06 的修复/重建候选接收合同、命令行工具、完整审计证据、报告 schema、负向测试和真实模型族基线脚本
已经实现。服务只负责接收和审计，不修复模型、不启动切片 pipeline，也不写 TIFF 或 production package。

当前爱神、玫瑰、梯田三个 required family 仍没有 admitted candidate，真实矩阵为 `0/3 BLOCKED`。这表示
R4-06 的软件能力已经完成，但真实资产准入条件尚未满足；R4-07 不能启动。

## 2. 实现内容

```text
RepairedAssetIntakeTypes：
  固化 strict_pass_original / external_repaired / independently_rebuilt；
  记录 source/resource/transform/geometry/attribute/audit hash；
  记录尺寸、姿态、拓扑、完整自相交、材质、纹理和 UV 证据。

RepairedAssetIntakeService：
  校验 required family、候选身份、原/新 source hash 和 provenance；
  比较尺寸、姿态、材质、纹理、UV 和资源差异；
  执行两次候选完整审计并校验 repeatability；
  sampled/incomplete、缺失纹理、post-strict 失败和未批准属性差异全部 fail-closed。

repaired_asset_intake CLI：
  读取 slicesoft.repaired_asset_intake_manifest.12e_08c_r4.1；
  写出 slicesoft.repaired_asset_intake.12e_08c_r4.1；
  admitted 返回 0，blocked 返回 2，合同/执行错误返回 1。
```

`ModelPreflightService` 增加仅供进程内消费者使用的完整审计 evidence，并随 cache 一同保存。稳定
`slicesoft.model_preflight.12e_08c_r4.1` JSON schema 未改变；intake 可直接复用同一次完整审计，不再次执行
独立几何分析。

## 3. 稳定阻断码

```text
E_12E_INTAKE_MANIFEST_INVALID
E_12E_INTAKE_FAMILY_UNKNOWN
E_12E_INTAKE_FAMILY_PATH_MISMATCH
E_12E_INTAKE_CANDIDATE_KIND_INVALID
E_12E_INTAKE_PROVENANCE_MISSING
E_12E_INTAKE_ORIGINAL_HASH_MISMATCH
E_12E_INTAKE_CANDIDATE_HASH_MISMATCH
E_12E_INTAKE_CANDIDATE_IDENTITY_INVALID
E_12E_INTAKE_RESOURCE_MISSING
E_12E_INTAKE_POST_STRICT_FAILED
E_12E_INTAKE_ATTRIBUTE_MISMATCH
E_12E_INTAKE_COORDINATE_MISMATCH
E_12E_INTAKE_BOUNDS_DELTA_EXCEEDED
E_12E_INTAKE_REPEATABILITY_FAILED
E_12E_INTAKE_EXECUTION_FAILED
```

## 4. 测试覆盖

Generated unit/golden 覆盖：

| Case | 预期 | 结果 |
|---|---|---|
| required family strict PASS 原始闭合 OBJ | admitted / count=1 | PASS |
| 跨族 clean control | family mismatch / count=0 | PASS |
| 修复候选缺 provenance | BLOCKED | PASS |
| 原始 hash 不匹配 | BLOCKED | PASS |
| 属性差异未经批准 | BLOCKED | PASS |
| 纹理资源缺失 | BLOCKED | PASS |
| 完整自相交预算不足 | post-strict BLOCKED | PASS |
| 开口候选 | post-strict BLOCKED | PASS |

真实 family 基线使用每族一个已跟踪代表：

| Family | 当前代表 | Intake |
|---|---|---|
| aishen | `MF_aishen_damuzhi_L_tx02.obj` | BLOCKED / confirmed self-intersection |
| meigui | `04.obj` | BLOCKED / confirmed self-intersection |
| titian | `dmz.obj` | BLOCKED / confirmed self-intersection |

真实脚本输出 `required_family_matrix.json`，固定记录 `admittedFamilyCount=0`、
`productionOutputWritten=false`。生成证据位于忽略目录
`output/benchmarks/12e_08c_r4_06_repaired_asset_intake`，不纳入源代码提交。

## 5. 验证结果

```text
Debug repaired_asset_intake / unit target build                        PASS
repaired_asset_intake_unit_tests                                      PASS
model_preflight_service_unit_tests                                    PASS
mesh_repair_preflight_unit_tests                                      PASS
Release run_12e_08c_r4_06_repaired_asset_intake.ps1                   PASS
真实 required family matrix                                           0/3 BLOCKED（预期）
production TIFF/package                                               未生成（预期）
```

## 6. 后续 Gate

```text
R4-06 软件实现：COMPLETE；
R4-06 真实资产准入：0/3 BLOCKED；
R4-07：必须等待 aishen/meigui/titian 各一个 admitted candidate；
R4-08：等待 R4-07 四 case、Release 预算、legacy TIFF/RIP/Quick CI；
12E-08D：继续 BLOCKED，且仍需用户明确授权。
```
