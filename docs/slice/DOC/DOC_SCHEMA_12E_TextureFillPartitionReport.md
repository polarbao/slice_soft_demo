# DOC_SCHEMA_12E Texture Fill Partition Report

> 文档状态：12D CLOSURE LINKAGE IMPLEMENTED / PRODUCTION NOT ADMITTED
> Schema：`slicesoft.texture_fill_partition.12e.1`
> 日期：2026-07-16

## 1. 目的

定义 12E 全局纹理表面层与模型填充互补分区的机器可读报告。该报告证明分区、宽度、后端、纹理传递和性能证据，不替代 `p0.rgbwsv.2` manifest、`slice_report` 或 12D `material_closure_report`。

计划路径：

```text
reports/texture_fill_partition_report.json
```

## 2. 根结构

```json
{
  "schema": "slicesoft.texture_fill_partition.12e.1",
  "packageProtocol": "p0.rgbwsv.2",
  "enabled": true,
  "strategy": "global_surface_shell",
  "availability": "unavailable",
  "status": "blocked",
  "productionAcceptance": "not_evaluated",
  "geometryMode": "global_3d_distance",
  "surfaceScope": "all_closed_surfaces",
  "backend": "none",
  "backendRole": "unavailable",
  "grid": {},
  "width": {},
  "partition": {},
  "textureTransfer": {},
  "diagnosticComposer": {},
  "closureLinkage": {},
  "performance": {},
  "queryStats": {},
  "layers": [],
  "issues": [],
  "configSnapshot": {}
}
```

## 3. 状态枚举

```text
availability = available | unavailable
status = unavailable | blocked | diagnostic | pass | fail
productionAcceptance = not_evaluated | passed | failed
backendRole = production_candidate | conformance_candidate | unavailable
```

约束：

```text
12E-01 report skeleton 只能是 unavailable/blocked/not_evaluated；
没有实际 partition result 时不得输出 pass；
OpenVDB conformance 结果不得仅因 partitionPass=true 自动成为 production passed；
productionAcceptance=passed 只能由后续显式 production admission 任务产生。
```

## 4. Width 对象

```json
{
  "requestedWidthMm": 0.10,
  "widthStepMm": 0.01,
  "baseMinimumWidthMm": 0.10,
  "classificationResolutionMm": null,
  "effectiveMinimumWidthMm": null,
  "effectiveWidthMm": null,
  "maxInteriorDistanceMm": null,
  "allTextureThresholdMm": null,
  "allTexture": false,
  "quantizationErrorMm": null,
  "clamped": false
}
```

动态值未计算时使用 `null`，不得用 0 冒充实际测量结果。

## 5. Partition 对象

```json
{
  "modelVoxels": 0,
  "textureSurfaceVoxels": 0,
  "modelFillVoxels": 0,
  "overlapTextureFillVoxels": 0,
  "unassignedModelVoxels": 0,
  "modelPixels": null,
  "textureSurfacePixels": null,
  "modelFillPixels": null,
  "overlapTextureFillPixels": null,
  "unassignedModelPixels": null,
  "textureCoverageRatio": 0.0,
  "modelFillCoverageRatio": 0.0,
  "thinRegionMergedVoxels": 0,
  "medialAxisTieCount": 0,
  "partitionPass": false
}
```

实际分区可用时必须满足：

```text
textureSurface ∩ modelFill = empty；
textureSurface ∪ modelFill = model；
overlapTextureFill = 0；
unassignedModel = 0；
textureSurface + modelFill = model。
```

## 6. Layer 对象

```json
{
  "layerIndex": 0,
  "zMm": 0.005,
  "modelVoxels": 0,
  "textureSurfaceVoxels": 0,
  "modelFillVoxels": 0,
  "overlapTextureFillVoxels": 0,
  "unassignedModelVoxels": 0,
  "partitionPass": false
}
```

`layers` 必须按真实 `layerIndex` 升序排列，不使用 preview 序号代替层号。12E-05
仍工作在三维分类 grid，因此字段使用 `Voxels`；尚未进入最终生产 raster 时，`Pixels`
字段必须为 `null`，不能用 0 冒充实际测量。

## 7. Texture Transfer 对象

```json
{
  "availability": "available",
  "status": "diagnostic",
  "productionAcceptance": "not_evaluated",
  "textureSurfaceVoxels": 0,
  "sampledTextureCount": 0,
  "materialDiffuseCount": 0,
  "fallbackCount": 0,
  "missingUvCount": 0,
  "missingTextureCount": 0,
  "textureSampleFailureCount": 0,
  "uvOutOfRangeCount": 0,
  "outsideColoredCount": 0,
  "reusedReferenceCount": 0,
  "nearestQueryCount": 0,
  "maxTransferDistanceMm": 0.0,
  "medialAxisTieCount": 0,
  "loadedTextureCount": 0,
  "textureCacheHits": 0,
  "textureCacheMisses": 0,
  "textureCacheBytes": 0,
  "issues": []
}
```

未执行纹理传递时 `availability=unavailable`、`status=not_evaluated`，计数字段为 0、
`maxTransferDistanceMm=null`。12E-06 实际传递必须满足 `nearestQueryCount=0`，证明只复用
partition 已保存的 closest reference；`outsideColoredCount` 必须为 0。

## 7.1 Diagnostic Composer 对象

```json
{
  "availability": "available",
  "status": "diagnostic",
  "productionAcceptance": "not_evaluated",
  "width": 2,
  "height": 1,
  "depth": 2,
  "layerCount": 2,
  "channelOrder": ["R", "G", "B", "W", "S", "V"],
  "textureSurfaceVoxels": 1,
  "modelFillVoxels": 2,
  "modelFillWhiteVoxels": 2,
  "modelFillVarnishVoxels": 0,
  "modelFillRgbVoxels": 0,
  "supportPrintVoxels": 0,
  "emptyVoxels": 1,
  "issues": []
}
```

该对象只描述内存诊断合成。`supportPrintVoxels` 必须为 0；它不代表生产 package 已生成。

## 7.2 Closure Linkage 对象

```json
{
  "availability": "available",
  "status": "diagnostic",
  "scope": "texture_model_fill_only",
  "source": "semantic_masks",
  "confidence": "exact",
  "productionAcceptance": "not_evaluated",
  "allTexture": false,
  "colorFillApplicability": "applicable",
  "allTextureReason": null,
  "colorFillGapVoxels": 0,
  "modelDomainGapVoxels": 0,
  "supportClosureStatus": "not_evaluated",
  "varnishClosureStatus": "not_evaluated",
  "repairAttempted": false,
  "productionOutputWritten": false,
  "layerCount": 1,
  "layers": [],
  "issues": []
}
```

普通模式 `colorFillApplicability=applicable` 且 gap 必须为 0。allTexture 模式使用：

```text
colorFillApplicability=not_applicable；
allTextureReason=all_texture_partition；
modelDomainGapVoxels=0。
```

`supportClosureStatus` 和 `varnishClosureStatus` 在 12E-07 必须为 `not_evaluated`，不得用
同尺寸零 mask 伪造完整 production closure PASS。

## 8. Performance 对象

```json
{
  "preflightMs": 1.0,
  "topologyMs": 1.0,
  "levelSetMs": 2.0,
  "gridSampleMs": 3.0,
  "occupancyMs": 5.0,
  "distanceMs": 7.0,
  "partitionMs": 11.0,
  "textureTransferMs": null,
  "totalCoreMs": 24.0,
  "gridVoxelCount": 4,
  "maskBytes": 12,
  "closestReferenceBytes": 96,
  "occupancyQueryBytes": 0,
  "nearestQueryBytes": 0,
  "openVdbGridBytes": 128,
  "workingSetBytes": null,
  "peakWorkingSetBytes": null
}
```

性能只统计 12E 核心步骤，不把 TIFF/PNG/JSON 写盘时间混入核心分类耗时。

## 8.1 Width Sweep 摘要

`BuildTextureFillPartitionWidthSweepSummary` 输出后端无关的诊断对象，至少包含：

```text
backend/backendRole/availability/status/productionAcceptance；
minimumWidthMm/maximumWidthMm/widthStepMm；
sampleCount/monotonic/endpoint；
totalCandidateCoreMs；
samples[] 中 requested/effective/allTexture/partition counts/core timing；
issues。
```

它是本 Schema 的诊断组成对象，不创建第二个生产协议，也不写入 manifest。

## 9. Issues

```json
{
  "code": "E_12E_PARTITION_BACKEND_UNAVAILABLE",
  "severity": "error",
  "message": "global 3D partition backend is unavailable",
  "layerIndex": null
}
```

首版稳定错误码见 `DOC_PREP_12E_R0_ConfigDTO契约准备.md`。`message` 可演进，`code` 是测试和兼容判断的稳定依据。

12E-05 新增：

```text
E_12E_WIDTH_SWEEP_EMPTY
E_12E_WIDTH_SWEEP_SAMPLE_FAILED
E_12E_WIDTH_SWEEP_MODEL_CHANGED
E_12E_WIDTH_SWEEP_TEXTURE_NON_MONOTONIC
E_12E_WIDTH_SWEEP_FILL_NON_MONOTONIC
E_12E_WIDTH_SWEEP_ENDPOINT_INVALID
```

12E-06 新增：

```text
E_12E_TEXTURE_TRANSFER_INPUT_INVALID
E_12E_TEXTURE_REFERENCE_MISSING
E_12E_TEXTURE_TRIANGLE_OUT_OF_RANGE
E_12E_TEXTURE_MISSING_UV
E_12E_TEXTURE_MISSING_RESOURCE
E_12E_TEXTURE_SAMPLE_FAILED
E_12E_DIAGNOSTIC_COMPOSER_INPUT_INVALID
E_12E_DIAGNOSTIC_COMPOSER_PARTITION_INVALID
```

12E-07 新增：

```text
E_12E_CLOSURE_ADAPTER_INPUT_INVALID
E_12E_CLOSURE_LAYER_ORDER_INVALID
E_12E_CLOSURE_MASK_INVALID
E_12E_CLOSURE_MODEL_DOMAIN_GAP
E_12E_CLOSURE_COLOR_FILL_GAP
E_12E_CLOSURE_CHANNEL_ORDER_INVALID
```

## 10. Config Snapshot

至少记录：

```text
texture.enabled；
texture.applyMode；
texture.surfaceShell 全部字段；
modelFill.enabled/material/scope/value；
OpenVDB build availability，只记录能力，不暴露实现类型；
output grid/pixel pitch/layer thickness 的有效值。
```

## 11. 与其他报告关系

```text
manifest：声明 package 和 report 路径；
slice_report：汇总最终生产输出统计；
texture_fill_partition_report：证明 12E 模型内部 texture/fill 分区；
material_closure_report：证明模型、支撑、光油和背景的横截面闭环；
RIP Reader：验证最终 RGBWSV TIFF 协议和 layer list。
```

12E-01 不要求把本报告写入 production manifest；只有后续 diagnostic/package 接入任务可以增加引用。

## 12. 兼容与安全

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
报告缺失不影响 legacy Profile；
OpenVDB 默认关闭；
没有 production admission 时不写 production TIFF。
```
