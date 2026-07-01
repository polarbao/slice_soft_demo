# DEV_10_LayerChannelSummaryContract

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 10-2
> 生成日期：2026-07-01
> 任务：Task 10-2 Layer summary / channel summary

---

## 1. 目标

本文件定义 Stage 10 的每层统计和每通道统计契约，确保下游 RIP 工程、UI layer preview、golden 验证和 release candidate gate 使用同一套统计口径。

本文件只定义统计语义，不改变 `p0.rgbwsv.2`、不改变 RGBWSV 通道顺序、不实现 RIP 半色调。

---

## 2. 当前实现依据

当前统计口径来自以下 A 级代码：

```text
src/slicer_core/slicer.cpp
src/slicer_core/tiff_io.cpp
src/slicer_core/rip_reader.cpp
tests/packages/legacy/legacy_v1_tiled/reports/slice_report.json
```

核心实现函数：

```text
update_channel_stats
channel_stats_to_json
channel_stats_array_to_json
layer_diagnostics_to_json
material_process_report_to_json
read_rgbwsv_tiff
validate_slice_package
```

---

## 3. 固定协议前提

```text
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType only in metadata/report/debug
```

所有统计必须按生产 TIFF 值解释，不按 preview PNG 显示值解释。

---

## 4. Channel Summary 口径

每个通道必须输出以下字段：

| 字段 | 类型 | 统计口径 | Golden |
|---|---|---|---|
| `printPixels` | uint64 | `value < 255` 的像素数 | exact |
| `fullPrintPixels` | uint64 | `value == 0` 的像素数 | exact |
| `partialPrintPixels` | uint64 | `1 <= value <= 254` 的像素数 | exact |
| `emptyPixels` | uint64 | `value == 255` 的像素数 | exact |
| `minValue` | int | 该通道最小 uint8 值 | exact |
| `maxValue` | int | 该通道最大 uint8 值 | exact |

约束：

```text
printPixels = fullPrintPixels + partialPrintPixels
printPixels + emptyPixels = widthPx * heightPx
0 <= minValue <= maxValue <= 255
```

跨层 totals 约束：

```text
totals.channelStats[channel].printPixels = sum(layers[].channelStats[channel].printPixels)
totals.channelStats[channel].emptyPixels = sum(layers[].channelStats[channel].emptyPixels)
totals.channelStats[channel].fullPrintPixels = sum(layers[].channelStats[channel].fullPrintPixels)
totals.channelStats[channel].partialPrintPixels = sum(layers[].channelStats[channel].partialPrintPixels)
totals.channelStats[channel].minValue = min(layers[].channelStats[channel].minValue)
totals.channelStats[channel].maxValue = max(layers[].channelStats[channel].maxValue)
```

---

## 5. 通道语义

| 通道 | 索引 | 名称 | 打印含义 | 空白含义 |
|---|---:|---|---|---|
| R | 0 | RGB 红 | 红色墨水打印或参与 RGB 颜色 | `255` 不打印 |
| G | 1 | RGB 绿 | 绿色墨水打印或参与 RGB 颜色 | `255` 不打印 |
| B | 2 | RGB 蓝 | 蓝色墨水打印或参与 RGB 颜色 | `255` 不打印 |
| W | 3 | White | 白墨打印 | `255` 不打印 |
| S | 4 | Support | 支撑材料打印 | `255` 不打印 |
| V | 5 | Varnish | 光油打印 | `255` 不打印 |

RGB 聚合字段必须按三个 RGB 通道联合解释：

```text
rgbPrintPixels = count(R < 255 or G < 255 or B < 255)
rgbNonZeroPixels = rgbPrintPixels
```

W / S / V 聚合字段必须按单通道解释：

```text
whitePrintPixels = channelStats.W.printPixels
supportPrintPixels = channelStats.S.printPixels
varnishPrintPixels = channelStats.V.printPixels
```

当前报告中 `NonZeroPixels` 是历史命名，Stage 10 语义应解释为 `printPixels`，不是数学非零值。

---

## 6. Layer Summary 字段

### 6.1 Manifest Layer List

`manifest.layers[]` 是下游定位 TIFF 文件的稳定列表：

| 字段 | 类型 | 口径 | Golden |
|---|---|---|---|
| `index` | int | 0-based layer index | exact |
| `zMm` | number | 层中心 Z 坐标 | tolerance |
| `path` | string | 相对 package root 的 TIFF 路径 | exact |
| `widthPx` | int | 层宽，必须等于 grid width | exact |
| `heightPx` | int | 层高，必须等于 grid height | exact |
| `modelPixels` | uint64 | 模型 mask 像素数 | exact for fixture |
| `supportPixels` | uint64 | 支撑 mask 像素数 | exact for fixture |

### 6.2 Slice Report Layer

`reports/slice_report.json.layers[]` 是正式 layer summary 来源：

| 字段 | 类型 | 口径 | Golden |
|---|---|---|---|
| `layerIndex` | int | 0-based layer index | exact |
| `zMm` | number | 层中心 Z 坐标 | tolerance |
| `modelPrintPixels` | uint64 | 模型打印区域像素 | exact for fixture |
| `rgbPrintPixels` | uint64 | RGB 联合打印像素 | exact for fixture |
| `whitePrintPixels` | uint64 | W 通道打印像素 | exact for fixture |
| `supportPrintPixels` | uint64 | S 通道打印像素 | exact for fixture |
| `varnishPrintPixels` | uint64 | V 通道打印像素 | exact for fixture |
| `channelStats` | object | R/G/B/W/S/V 每通道统计 | exact |
| `supportTypeStats` | object | 支撑类型分类统计 | diagnostic exact for fixture |
| `supportConnectivity` | object | 支撑连通性诊断 | diagnostic |
| `fillWarnings` | array | 填充/采样警告 | diagnostic |

建议新增但当前未固化的候选字段：

| 字段 | 说明 |
|---|---|
| `nonEmptyPixelCount` | 任意 RGBWSV 通道 `value < 255` 的 union 像素数 |
| `modelSupportOverlapPixels` | 模型与支撑同时打印的冲突像素数 |
| `diagnosticOverlayCount` | UI overlay 或诊断覆盖像素数 |

这些字段必须在 10-6 schema / golden 中出现后，才能升级为 Stable。

---

## 7. Totals Summary 字段

`reports/slice_report.json.totals` 必须包含：

| 字段 | 类型 | 口径 | Golden |
|---|---|---|---|
| `modelPrintPixels` | uint64 | 所有层模型打印区域像素总和 | exact for fixture |
| `rgbPrintPixels` | uint64 | 所有层 RGB 联合打印像素总和 | exact for fixture |
| `whitePrintPixels` | uint64 | 所有层 W 通道打印像素总和 | exact |
| `supportPrintPixels` | uint64 | 所有层 S 通道打印像素总和 | exact |
| `varnishPrintPixels` | uint64 | 所有层 V 通道打印像素总和 | exact |
| `channelStats` | object | 所有层 R/G/B/W/S/V 每通道累计统计 | exact |
| `texture` | object | 纹理采样累计统计 | candidate until 10-3 |
| `materialPolicy` | object | 材料策略累计统计 | comparable |
| `supportTypeStats` | object | 支撑类型累计统计 | diagnostic |
| `supportConnectivity` | object | 支撑连通性累计诊断 | diagnostic |

---

## 8. Golden 比较规则

### 8.1 Exact Match

以下字段可用于固定 fixture 的 exact golden：

```text
manifest.schema
manifest.tiff.channelOrder
manifest.tiff.channelCount
manifest.tiff.bitDepth
manifest.tiff.sampleFormat
manifest.tiff.planarConfig
manifest.tiff.storageMode
manifest.tiff.polarity
manifest.tiff.printValue
manifest.tiff.emptyValue
manifest.grid.widthPx / heightPx / layerCount
manifest.layers[].index / path / widthPx / heightPx
slice_report.layers[].channelStats.*
slice_report.totals.channelStats.*
slice_report.layers[].rgbPrintPixels
slice_report.layers[].whitePrintPixels
slice_report.layers[].supportPrintPixels
slice_report.layers[].varnishPrintPixels
slice_report.totals.rgbPrintPixels
slice_report.totals.whitePrintPixels
slice_report.totals.supportPrintPixels
slice_report.totals.varnishPrintPixels
```

### 8.2 Tolerance Match

以下字段建议做容差比较：

```text
zMm
pixelSizeMm
pixelSizeXmm / pixelSizeYmm
coverageRatio
```

建议默认容差：

```text
absoluteTolerance = 1e-9 for mm values generated from DPI/layer height
absoluteTolerance = 1e-6 for coverageRatio
```

### 8.3 Trend / Diagnostic Only

以下字段不应作为 hard gate exact match：

```text
absolute source paths
preview PNG display statistics
supportConnectivity.components bbox ordering beyond deterministic fixtures
warnings text
timings
memory
OpenVDB experimental diagnostic-only fields
```

---

## 9. 下游解释规则

下游 RIP 工程应优先消费：

```text
manifest.tiff.* protocol fields
manifest.layers[] TIFF path and dimensions
layer TIFF pixel values
slice_report.channelStats for validation / auditing
texture_report and material reports for explanation, not direct bitstream generation
```

下游不应消费：

```text
preview PNG as production data
SupportType as TIFF value
UI pseudo color as material value
experimental OpenVDB diagnostic report as production package
```

---

## 10. 后续任务衔接

```text
10-3：在本统计口径上定义 textureResolvedRate、uvCoverageRate、fallbackPixelRate 等指标。
10-6：将 exact / tolerance / diagnostic 规则固化为 output contract golden / schema。
11：LayerPreview UI 应读取本契约，而不是反推 preview PNG 或 slicer.cpp 内部结构。
```
