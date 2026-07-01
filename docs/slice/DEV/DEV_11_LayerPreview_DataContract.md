# DEV_11_LayerPreview_DataContract

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 11-1
> 生成日期：2026-07-01
> 任务：Task 11-1 LayerPreview data contract

---

## 1. 目标

本文件定义 Stage 11 的 UI 层预览数据契约，确保 `slicer_debug_ui` 只通过 package / manifest / report / preview 文件读取切片结果，不访问 `slicer.cpp` 内部临时结构，不改变 `p0.rgbwsv.2` 生产协议。

本契约面向 UI，可由现有输出派生：

```text
manifest.json
reports/preview_report.json
reports/slice_report.json
reports/texture_report.json
reports/support_report.json
reports/material_policy_report.json
preview/*.png
layers/*.tiff
```

---

## 2. 当前实现依据

当前 A 级证据：

```text
src/slicer_core/slicer.cpp
apps/slicer_debug_ui/services/PreviewReportIndex.cpp
apps/slicer_debug_ui/services/ReportLoader.cpp
tests/golden/expected/10_output_contract_schema.json
output/UiSmokeOverlayRgbwv/manifest.json
output/UiSmokeOverlayRgbwv/reports/preview_report.json
output/UiSmokeOverlayRgbwv/reports/slice_report.json
```

当前 preview report 已有：

```text
schema = p0.preview_report.1
channels[]
files[].channel
files[].kind
files[].layerIndex
files[].path
files[].type
files[].format
files[].printPixels
files[].displayNonZeroPixels
files[].nonZeroPixels
files[].maxValue
pseudoColors
```

Stage 11 不把 preview PNG 当生产数据；preview PNG 只作为 UI 显示帧。生产统计仍以 manifest / slice_report / TIFF 协议为准。

---

## 3. Schema

LayerPreview 派生契约使用：

```text
schema = p0.layer_preview_manifest.1
```

该 schema 当前可以作为 UI 内部 DTO / golden schema 使用；是否写成 package 内独立 `reports/layer_preview_manifest.json`，留到 Task 11-6 或 REPORT_11 决策。

---

## 4. LayerPreviewManifest

| 字段 | 类型 | 来源 | 说明 |
|---|---|---|---|
| `schema` | string | UI 合成 | 固定 `p0.layer_preview_manifest.1` |
| `packagePath` | string | UI 输入 | 当前 package 路径，不做跨机器 exact match |
| `manifestPath` | string | UI 合成 | `manifest.json` |
| `previewReportPath` | string | UI 合成 | `reports/preview_report.json` |
| `sliceReportPath` | string | UI 合成 | `reports/slice_report.json` |
| `layerCount` | int | `manifest.grid.layerCount` | 必须大于 0 |
| `widthPx` / `heightPx` | int | `manifest.grid` | UI 画布基础尺寸 |
| `layerHeightMm` | number | `manifest.grid.layerThicknessMm` | 层高 |
| `zMinMm` / `zMaxMm` | number | `manifest.layers[]` | 层 Z 范围 |
| `pixelSizeXmm` / `pixelSizeYmm` | number | `manifest.grid` | 像素物理尺寸 |
| `availableChannels` | array | preview / slice report | UI 可切换通道 |
| `frames` | array | preview + slice report | 每层每通道显示帧 |
| `pseudoColorMaps` | object | preview config / UI default | 伪彩映射 |
| `diagnostics` | array | report | fallback / missing / warning 诊断摘要 |

约束：

```text
layerCount = manifest.grid.layerCount
widthPx = manifest.grid.widthPx
heightPx = manifest.grid.heightPx
availableChannels 不代表生产 TIFF 通道顺序
生产 TIFF 通道顺序仍固定 R G B W S V
```

---

## 5. LayerPreviewChannel

首批通道：

| id | 类型 | 来源 | 显示说明 |
|---|---|---|---|
| `rgb` | composite | preview `model_rgb` / true-color | RGB 合成预览 |
| `texture_rgb` | composite | preview `texture_rgb` / true-color | 纹理颜色预览 |
| `white` | mask | preview `white_w` / W stats | 白墨伪彩 |
| `support` | mask | preview `support_s` / S stats | 支撑伪彩 |
| `varnish` | mask | preview `varnish_v` / V stats | 光油伪彩 |
| `occupancy` | derived | slice layer stats | 模型占用 / 非空区域 |
| `diagnostic` | overlay | warnings / texture fallback | 诊断覆盖层 |
| `texture_fidelity` | overlay | texture_report | 纹理 fallback / missing UV / missing texture |

通道约束：

```text
channel id 是 UI 视图，不是 TIFF channel value；
white/support/varnish 的 display color 由 UI 伪彩决定；
RGB true-color preview 不改变生产 RGBWSV 数据；
diagnostic / texture_fidelity 可以没有 PNG，允许使用 report 生成 overlay。
```

---

## 6. LayerPreviewFrame

| 字段 | 类型 | 来源 | 说明 |
|---|---|---|---|
| `layerIndex` | int | preview / slice report | 0-based |
| `zMm` | number | `slice_report.layers[].zMm` | 当前层高度 |
| `channel` | string | preview file | 对应 LayerPreviewChannel id |
| `kind` | string | preview file | `single` / `composite` / `overlay` |
| `source` | string | UI 合成 | `preview_png` / `slice_report` / `texture_report` |
| `path` | string | preview file | 相对 package path |
| `widthPx` / `heightPx` | int | manifest grid | 预览图尺寸应匹配或可缩放到 grid |
| `stats` | object | slice report / preview report | 该层该通道统计 |
| `available` | bool | UI 合成 | 当前帧是否可显示 |
| `unavailableReason` | string | UI 合成 | 没有 preview 时原因 |

缺图规则：

```text
如果某层没有某通道 preview PNG，但 slice_report 有该通道统计，则 frame.available=false，source=slice_report；
UI 应显示空白/占位和统计，不应报生产数据错误；
只有 manifest/layer TIFF 缺失才应走 package error。
```

---

## 7. LayerPreviewStats

每个 frame 可包含：

| 字段 | 来源 | 说明 |
|---|---|---|
| `printPixels` | preview / slice report | 打印像素数，按 Stage 10 口径解释 |
| `displayNonZeroPixels` | preview report | 显示图中的非空像素，仅 UI 诊断 |
| `nonZeroPixels` | preview report | 历史显示统计，不能当生产统计 |
| `maxValue` | preview report | 显示图最大值 |
| `rgbPrintPixels` | slice report | RGB 联合打印像素 |
| `whitePrintPixels` | slice report | W 通道打印像素 |
| `supportPrintPixels` | slice report | S 通道打印像素 |
| `varnishPrintPixels` | slice report | V 通道打印像素 |
| `channelStats` | slice report | R/G/B/W/S/V 生产统计 |
| `supportTypeStats` | support report / slice report | 支撑类型诊断 |
| `fillWarnings` | slice report | 几何填充警告 |

解释规则：

```text
生产统计优先使用 slice_report；
preview report 的 displayNonZeroPixels 只用于显示图自检；
UI 不应把 black_is_print 的生产值直接当显示值。
```

---

## 8. PseudoColorMap

伪彩映射属于 UI 显示策略，不属于生产数据：

| id | 默认颜色 | 说明 |
|---|---|---|
| `empty` | `[255, 255, 255]` | 未打印区域 |
| `support` | `[0, 255, 0]` | 支撑打印区域 |
| `white` | `[0, 170, 255]` | 白墨打印区域 |
| `varnish` | `[127, 127, 127]` | 光油打印区域 |
| `diagnostic_warning` | `[255, 180, 0]` | 警告覆盖层 |
| `diagnostic_error` | `[255, 0, 0]` | 阻断覆盖层 |
| `texture_fallback` | `[255, 0, 255]` | 纹理 fallback |

约束：

```text
伪彩可以由配置覆盖；
伪彩不能写回 production TIFF；
伪彩不能改变 manifest / slice_report 的生产统计。
```

---

## 9. Diagnostic Overlay

诊断覆盖层来自：

```text
texture_report.warnings
slice_report.layers[].fillWarnings
slice_report.layers[].supportConnectivity
support_report
material_policy_report.warnings
experimental OpenVDB report 的 productionAdmission / blockerCodes
```

首批 overlay 类型：

| id | 来源 | UI 行为 |
|---|---|---|
| `texture_fallback` | texture report | 标记 fallback 发生；没有逐像素数据时显示全局提示 |
| `missing_texture` | texture report | 显示资源缺失提示 |
| `missing_uv` | model / texture report | 显示 UV 缺失提示 |
| `support_small_component` | support connectivity | 显示小连通域统计 |
| `fill_warning` | slice report | 显示层级填充警告 |
| `admission_blocker` | experimental report | 显示 production admission blocker |

当前没有逐像素 diagnostic mask 时，overlay 可以退化为 layer badge / report badge，不得伪造像素位置。

---

## 10. 验证和 Golden

机器可读契约文件：

```text
tests/golden/expected/11_layer_preview_manifest_schema.json
```

Task 11-1 只固化 schema，不要求 UI 已完全实现。后续任务使用该 schema：

```text
11-2：LayerPreviewDataProvider / UI viewer 读取该契约；
11-6：UI smoke 和 golden preview 验证该契约。
```

---

## 11. 非目标范围

```text
不修改 p0.rgbwsv.2；
不改变 RGBWSV channel order；
不默认启用 OpenVDB；
不实现 RIP 半色调、设备通信或喷头 bitstream；
不让 UI 直接访问 slicer.cpp 内部临时结构；
不让 UI 直接依赖 OpenVDB 类型；
不把 preview PNG 作为生产输入；
不默认启用多模型 production 输出。
```
