# DOC_SCHEMA_12E Texture Fill Partition Report

> 文档状态：SKELETON + CPU/OPENVDB CONFORMANCE DTO IMPLEMENTED / SUCCESS SERIALIZATION PENDING
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
  "width": {},
  "partition": {},
  "textureTransfer": {},
  "performance": {},
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
  "modelPixels": 0,
  "textureSurfacePixels": 0,
  "modelFillPixels": 0,
  "overlapTextureFillPixels": 0,
  "unassignedModelPixels": 0,
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
  "modelPixels": 0,
  "textureSurfacePixels": 0,
  "modelFillPixels": 0,
  "overlapTextureFillPixels": 0,
  "unassignedModelPixels": 0,
  "partitionPass": false
}
```

`layers` 必须按真实 `layerIndex` 升序排列，不使用 preview 序号代替层号。

## 7. Texture Transfer 对象

```json
{
  "sampledTextureCount": 0,
  "fallbackCount": 0,
  "missingUvCount": 0,
  "missingTextureCount": 0,
  "uvOutOfRangeCount": 0,
  "outsideColoredCount": 0,
  "maxTransferDistanceMm": null,
  "medialAxisTieCount": 0
}
```

12E-01 尚未执行纹理传递时计数字段为 0、测量字段为 `null`，并通过 status/issue 说明不可用原因。

## 8. Performance 对象

```json
{
  "preflightMs": null,
  "occupancyMs": null,
  "distanceMs": null,
  "partitionMs": null,
  "textureTransferMs": null,
  "totalCoreMs": null,
  "peakMemoryBytes": null
}
```

性能只统计 12E 核心步骤，不把 TIFF/PNG/JSON 写盘时间混入核心分类耗时。

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
