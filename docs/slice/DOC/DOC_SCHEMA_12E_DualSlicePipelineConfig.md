# DOC_SCHEMA_12E Dual Slice Pipeline Config

> 文档状态：08D-01/02 IMPLEMENTED / SHARED WRITER PENDING
> 日期：2026-07-20

## 1. 配置结构

新增顶层对象：

```json
{
  "slicePipeline": {
    "mode": "legacy"
  }
}
```

稳定枚举：

```text
legacy
global_surface_shell
```

旧配置未声明时默认 `legacy`。该默认只用于兼容旧配置，不允许根据其他字段自动推断 global。

## 2. 概念边界

```text
slicePipeline.mode：端到端切片执行链路；
slicingMode：closed mesh / relief geometry category；
texture.applyMode：纹理区域策略；
experimental.openvdbPipeline.engine：历史实验 backend；
meshRepair：global 前置修复策略。
```

这些字段不得互相替代。

## 3. Legacy 组合

`slicePipeline.mode=legacy`：

```text
继续接受当前正式 legacy texture/material/support/varnish 配置；
不得因存在 12E 字段静默进入 global；
省略 slicePipeline 的旧 Profile 输出保持不变；
生产成功必须生成当前 RGBWSV TIFF package。
```

## 4. Global 组合

`slicePipeline.mode=global_surface_shell` 至少要求：

```text
texture.enabled=true；
texture.applyMode=global_surface_shell；
texture.surfaceShell.geometryMode=global_3d_distance；
modelFill.enabled=true；
modelFill.scope=complement_of_global_texture_shell；
显式 production Profile；
strict topology 或 repaired post-strict PASS；
12E production admission=admitted。
```

未 admitted 时可以生成 diagnostic report/preview，但实际生产命令必须阻断，不生成可使用 TIFF package。

## 5. Output Invariant

`output` 不提供“关闭生产 TIFF”的模式开关。只要执行结果状态为 production success，就必须满足：

```text
package/layers/layer_<index>.tiff 完整存在；
manifest layer list 与 TIFF 一致；
p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
RIP Reader strict 可读取；
preview/report 按当前配置继续生成或保留。
```

## 6. Stable Errors

建议冻结：

```text
E_12E_PIPELINE_MODE_UNSUPPORTED
E_12E_PIPELINE_MODE_CONFIG_MISMATCH
E_12E_PIPELINE_GLOBAL_NOT_ADMITTED
E_12E_PIPELINE_GLOBAL_TOPOLOGY_BLOCKED
E_12E_PIPELINE_PRODUCTION_TIFF_REQUIRED
E_12E_PIPELINE_SILENT_FALLBACK_FORBIDDEN
E_12E_PIPELINE_GLOBAL_ADAPTER_INPUT_INVALID
E_12E_PIPELINE_GLOBAL_ADAPTER_CLOSURE_REQUIRED
E_12E_PIPELINE_GLOBAL_ADAPTER_LAYER_MISMATCH
E_12E_PIPELINE_GLOBAL_ADAPTER_PROTOCOL_MISMATCH
```

## 7. Effective Config

Qt session effective config 必须记录：

```text
slicePipeline.mode；
source Profile；
admission status；
repair status；
global width/modelFill；
output protocol snapshot；
实际执行 mode，不只记录用户 requested mode。
```

requested/effective 不一致时必须给出 blocker，不允许静默改 mode。

## 8. Report

在不修改 manifest schema 的前提下，由 `slice_report`、`texture_fill_partition_report` 和 UI timing/status 记录：

```text
requestedPipelineMode；
effectivePipelineMode；
productionAcceptance；
fallbackApplied=false；
productionOutputWritten；
productionTiffLayerCount；
```

## 9. Negative Cases

```text
unknown mode -> reject；
legacy + global-only fill scope -> mismatch；
global + legacy texture mode -> mismatch；
global not admitted -> no production TIFF；
global blocker -> no legacy fallback；
production success without TIFF -> hard failure；
old config omitted -> legacy unchanged。
```
