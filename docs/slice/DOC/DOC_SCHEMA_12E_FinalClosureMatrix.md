# DOC_SCHEMA 12E Final Closure Matrix

> 状态：FROZEN FOR 12E-10
> 日期：2026-07-23

## 1. 目的

冻结 12E-10 双模式真实模型、preview、协议、性能和残余风险的汇总字段。该矩阵是验收报告，不是新的
生产 package schema。

## 2. 顶层字段

```text
schema = slicesoft.stage12e.final_closure_matrix.1
generatedAt
buildConfiguration
compiler
openVdbEnabled
referenceMachine
cases[]
summary
residualRisks[]
decision
```

## 3. Case 字段

```text
caseId
modelFamily
modelPath
modelSha256
inputFormat
pipelineMode
productionProfileId
dpiX
dpiY
pixelSizeXmm
pixelSizeYmm
widthPoint = minimum | intermediate | all_texture
requestedWidthMm
effectiveWidthMm
allTextureThresholdMm
preflightState
admissionState
blockingCodes[]
productionOutputWritten
packagePath
manifestSchema
layerCount
tiffHashProjection
ripStrictPass
textureSurfacePixels
modelFillPixels
supportPixels
whitePixels
varnishPixels
overlapPixels
unassignedPixels
previewLayerIndexAlignmentPass
previewPhysicalAspectPass
coreMs
composeMs
tiffSaveMs
previewReportSaveMs
totalMs
peakWorkingSetBytes
fallbackApplied
result
```

## 4. 固定矩阵

生产正向：

```text
xiao_ma baseline family；
yecan baseline family。
```

格式控制：

```text
samples/models/3mf/texture2d_checker_cube.3mf。
```

阻断披露：

```text
aishen_fudiao；
meigui_fudiao；
titian_fudiao。
```

阻断披露行必须记录真实错误，不要求生成 package。

## 5. 判定

```text
PASS：该 case 的所有适用断言通过；
BLOCKED_EXPECTED：strict blocker 正确阻断且无写包/fallback；
FAIL：协议、材料、同层、身份或性能证据缺失；
NOT_EVALUATED：任务未执行，不能计入完成。
```

Stage 12E 只有在 required production 行 PASS、阻断行按预期 fail-closed、12E-10A/B/C/D 完成后才能收口。
