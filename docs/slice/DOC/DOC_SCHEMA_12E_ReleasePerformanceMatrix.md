# DOC_SCHEMA 12E Release Performance Matrix

> Schema：`slicesoft.stage12e.release_performance.1`
> 状态：FROZEN FOR 12E-10C
> 日期：2026-08-03

## 1. 顶层字段

```text
schema
generatedAt
buildConfiguration
referenceMachine
measurementContract
samples
summaries
comparisons
summary
decision
residualRisks
```

## 2. Sample 必填字段

```text
caseId / pairId / iteration / measured
modelFamily / modelPath / modelSha256
pipelineMode / widthPoint / requestedWidthMm / effectiveWidthMm
dpiX / dpiY / layerThicknessMm
storageMode / compression / previewEnabled
packagePath / layerCount / packageBytes / ripStrictPass
fallbackApplied / result
timingsMs.core / compose / tiffSave / previewReportSave / total / wallClock
memory.peakWorkingSetBytes
```

warm-up 样本可保留在 `samples`，但 `measured=false`，不得进入中位数。

## 3. Summary 必填字段

每个 `pairId + pipelineMode` 汇总：

```text
sampleCount
medianCoreMs
medianComposeMs
medianTiffSaveMs
medianPreviewReportSaveMs
medianTotalMs
medianWallClockMs
medianPeakWorkingSetBytes
maxPeakWorkingSetBytes
```

每个 `pairId` 的 `comparisons` 至少包含：

```text
globalVsLegacyCoreRatio
globalVsLegacyTotalRatio
globalVsLegacyPeakMemoryRatio
```

## 4. 判定

```text
PASS_LEGACY_DEFAULT_GLOBAL_EXPLICIT_CANDIDATE
FAIL_MEASUREMENT_CONTRACT
```

10C 不能仅因一次参考机性能结果自动切换默认引擎。任何默认切换必须另行获得产品与架构授权。
