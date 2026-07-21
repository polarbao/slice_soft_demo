# DOC_SCHEMA_12E Mesh Repair Release Evidence

> 文档状态：FROZEN / NON-PRODUCTION
> Schema：`slicesoft.mesh_repair_release_evidence.12e_08c_r3_03.1`
> 日期：2026-07-21

## 1. 用途

本 Schema 汇总 R3-03 的 Release repair、global diagnostic core、legacy TIFF invariant 与 RIP strict
证据。它不是生产 manifest，不得用于准许 global production package。

## 2. 顶层字段

```text
schema；
generatedAt；
stage=12E-08C-R3-03；
build；
contract；
cases[]；
legacyRegression；
result。
```

`contract` 固定记录 `diagnosticOnly=true`、`globalProductionPackageWritten=false`、写盘不计入核心预算、
RGBWSV、uint8 与 `black_is_print`。

## 3. Case 契约

每个 case 必须包含：

```text
caseId；
sourceHash/optionsHash；
repair.status/repairAttempted/blockerCodes；
globalCore.status；
timingsMs；
productionOutputWritten=false。
```

`globalCore.status` 仅允许：

```text
completed；
skipped_due_topology。
```

当状态为 `skipped_due_topology` 时，partition、texture transfer、raster mapping 和 full closure 均为
`skipped`，对应耗时必须为 `null`，不得以 0 冒充执行结果。

## 4. 分段计时

```text
importTransformMs；
preDiagnosticsEligibilityMs；
repairCoreMs + repairCoreStatus；
attributeValidationPostStrictMs + attributeValidationPostStrictStatus；
partitionMs；
textureTransferMs；
rasterMappingMs；
fullClosureMs；
globalCoreMs；
writeJsonMs + writeJsonStatus；
writeTiffPreviewMs + writeTiffPreviewStatus；
peakWorkingSetBytes。
```

JSON 写入当前为 `excluded_not_instrumented`；TIFF/PNG 在 global diagnostic lane 中为 `not_executed`。

## 5. Legacy 与结果

`legacyRegression` 必须记录 repair 默认关闭、逐层 TIFF SHA-256 invariant、RIP strict、协议和 Quick CI
实际状态。已知 baseline 差异必须写为 `failed_known_baseline`，不能写为 PASS。

`result.releaseBudget=blocked` 与 `productionAdmission=blocked` 表示证据任务可以 COMPLETE，但尚不满足
12E-08D 准入。
