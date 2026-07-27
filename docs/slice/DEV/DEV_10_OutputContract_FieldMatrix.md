# DEV_10_OutputContract_FieldMatrix

> 文档版本：v0.2
> 文档状态：Formal DEV / Stage 10-1
> 生成日期：2026-07-01
> 更新日期：2026-07-24
> 任务：Task 10-1 Output contract 字段

---

## 1. 目标

本文件定义 Stage 10 下游输出契约字段矩阵，回答哪些 package / manifest / report / layer summary 字段可以被下游 RIP 工程师、golden 校验和 UI 预览稳定依赖。

本阶段只定义契约，不实现 RIP 半色调、设备通信、喷头 bitstream，不修改 `p0.rgbwsv.2`。

---

## 2. 当前证据

当前字段矩阵基于以下 A 级实现和样例：

```text
src/slicer_core/slicer.cpp
src/slicer_core/rip_reader.cpp
src/slicer_core/tiff_io.cpp
src/slicer_core/reports/ReportSchemaValidator.cpp
apps/slicer_cli/main.cpp
tests/packages/legacy/legacy_v1_tiled/manifest.json
tests/packages/legacy/legacy_v1_tiled/reports/slice_report.json
tests/packages/legacy/legacy_v1_tiled/reports/texture_report.json
tests/golden/expected/09p_experimental_output_contract.json
```

当前 `rip_reader` 已严格校验：

```text
schema = p0.rgbwsv.1 | p0.rgbwsv.2
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
planarConfig = contiguous
storageMode = stripped | tiled
polarity = black_is_print
printValue = 0
emptyValue = 255
grid width / height / layerCount
layers array count / index / path / dimensions
TIFF storage and dimensions match manifest
```

---

## 3. 契约等级

| 等级 | 含义 | 下游使用方式 |
|---|---|---|
| Stable | Stage 10 稳定契约 | 可用于 RIP 输入、schema 校验、golden exact match |
| Comparable | 可比较契约 | 可用于 golden 趋势或容差比较，不应绑定机器路径 |
| Diagnostic | 诊断字段 | 可展示和排查，但不作为 RIP 消费必需字段 |
| Candidate | 候选字段 | 后续 10-2 / 10-3 / 10-6 固化前不能作为硬依赖 |
| NonContract | 非契约 | 不应被下游依赖 |

---

## 4. Package Layout

| 字段 / 路径 | 等级 | 当前来源 | 说明 |
|---|---|---|---|
| `manifest.json` | Stable | `slicer.cpp` | package 根入口，RIP reader 必读 |
| `layers/layer_XXXXXX.tiff` | Stable | `slicer.cpp` / `tiff_io.cpp` | 生产 RGBWSV 数据层 |
| `reports/package_report.json` | Stable | `slicer.cpp` | package 级 report base |
| `reports/slice_report.json` | Stable | `slicer.cpp` | layer summary / channel summary 当前主要来源 |
| `reports/texture_report.json` | Comparable | `slicer.cpp` | 纹理采样和 fallback 当前来源，10-3 会继续固化 |
| `reports/material_process_report.json` | Comparable | `slicer.cpp` | 材料工艺 profile 汇总 |
| `reports/material_policy_report.json` | Comparable | `slicer.cpp` | RGB/W/V 策略和打印像素统计 |
| `reports/support_report.json` | Diagnostic | `slicer.cpp` | 支撑形态、连通性和 SupportType 统计 |
| `reports/model_report.json` | Diagnostic | `slicer.cpp` | 输入模型统计、材质、UV、bbox |
| `reports/preview_report.json` | Diagnostic | `slicer.cpp` | UI/人工检查用 preview，不是 RIP 输入 |
| `preview/*.png` | NonContract | `slicer.cpp` | 显示用伪彩图或 true-color preview，不作为生产数据 |

---

## 5. Manifest Field Matrix

### 5.1 Root

| 字段 | 等级 | 类型 | 说明 |
|---|---|---|---|
| `schema` | Stable | string | 当前生产包必须为 `p0.rgbwsv.2`；`p0.rgbwsv.1` 仅 legacy 兼容读取 |
| `schemaVersion` | Stable | string | 应与 `schema` 保持一致 |
| `source.configPath` | Comparable | string | 可追踪来源；不做跨机器 exact match |
| `source.modelPath` | Comparable | string | 可追踪来源；不做跨机器 exact match |
| `source.format` | Stable | string | 输入格式，如 `obj` / `stl` / `3mf` |

### 5.2 Grid

| 字段 | 等级 | 类型 | 说明 |
|---|---|---|---|
| `grid.widthPx` | Stable | int | TIFF 层宽度，必须大于 0 |
| `grid.heightPx` | Stable | int | TIFF 层高度，必须大于 0 |
| `grid.layerCount` | Stable | int | 层数，必须等于 layer list 数量 |
| `grid.dpiX` / `grid.dpiY` | Stable | int | 独立必填，Reader 严格校验 72..2400；允许非等方 DPI |
| `grid.dpi` | Comparable | array<int,2> | 可选冗余字段；存在时必须与 `dpiX/dpiY` 一致 |
| `grid.pixelSizeXmm` / `grid.pixelSizeYmm` | Stable | number | 独立必填，必须分别与 `25.4 / dpiX`、`25.4 / dpiY` 一致 |
| `grid.pixelSizeMm` | Comparable | array<number,2> | 可选冗余字段；存在时必须与独立物理像素字段一致 |
| `grid.layerThicknessMm` | Stable | number | 层厚，必须大于 0 |
| `grid.originMm` | Comparable | array<number,3> | package 坐标原点，用于诊断和 UI 对齐 |

12E-09C 软件生产认证补充：

```text
首批通过组合：600/600、635/600；
两种组合均要求独立 dpiX/dpiY 与 pixelSizeXmm/pixelSizeYmm；
635/600 已通过 Legacy、Global restricted、Global material parity 真实模型 package 和 RIP strict；
认证结论只覆盖软件 package/RIP 合同，不代表打印机硬件标定；
TIFF 单文件当前不携带 XResolution/YResolution，物理尺寸必须从 manifest.grid 读取。
```

### 5.3 TIFF

| 字段 | 等级 | 类型 | 说明 |
|---|---|---|---|
| `tiff.channelOrder` | Stable | array<string,6> | 固定 `R G B W S V` |
| `tiff.channelCount` | Stable | int | 固定 6 |
| `tiff.bitDepth` | Stable | int | 固定 8 |
| `tiff.sampleFormat` | Stable | string | 固定 `uint` |
| `tiff.planarConfig` | Stable | string | 固定 `contiguous` |
| `tiff.storageMode` | Stable | string | `stripped` 或 `tiled` |
| `tiff.storage` | Comparable | string | 兼容字段，建议与 `storageMode` 一致 |
| `tiff.tiled` | Comparable | bool | 兼容字段，必须与 `storageMode` 逻辑一致 |
| `tiff.tileSize` | Stable | array<int,2> | `tiled` 时必需，两个值必须大于 0 |
| `tiff.rowsPerStrip` | Stable | int | `stripped` 时必需，必须大于 0 |
| `tiff.polarity` | Stable | string | 固定 `black_is_print` |
| `tiff.printValue` | Stable | int | 固定 0 |
| `tiff.emptyValue` | Stable | int | 固定 255 |
| `tiff.writeTiffLayers` | Diagnostic | bool | 当前任务运行选项，不代表 package schema 改变 |
| `tiff.layers` | Stable | array | 与 root `layers` 同义，兼容 reader 路径 |

### 5.4 Layers

| 字段 | 等级 | 类型 | 说明 |
|---|---|---|---|
| `layers[].index` | Stable | int | 0-based layer index |
| `layers[].zMm` | Stable | number | 层中心 Z 坐标 |
| `layers[].path` | Stable | string | 相对 package 根的 TIFF 路径 |
| `layers[].widthPx` / `heightPx` | Stable | int | 必须匹配 `grid.widthPx` / `grid.heightPx` |
| `layers[].modelPixels` | Comparable | int | 当前 model mask 像素统计 |
| `layers[].supportPixels` | Comparable | int | 当前 support mask 像素统计 |

---

## 6. TIFF Pixel Contract

| 内容 | 等级 | 说明 |
|---|---|---|
| channel 0 `R` | Stable | RGB 红通道 |
| channel 1 `G` | Stable | RGB 绿通道 |
| channel 2 `B` | Stable | RGB 蓝通道 |
| channel 3 `W` | Stable | 白墨通道 |
| channel 4 `S` | Stable | 支撑通道 |
| channel 5 `V` | Stable | 光油通道 |
| value `0` | Stable | 打印 |
| value `255` | Stable | 不打印 / 空白 |
| value `1..254` | Stable | 半强度打印值；当前统计为 partial print，不代表 RIP 半色调 |
| padding value | Stable | tiled padding 必须为 255 |

注意：SupportType 只允许出现在 metadata / report / debug 统计中，不能编码为 TIFF channel value。

---

## 7. Report Field Matrix

### 7.1 Package Report

| 字段 | 等级 | 说明 |
|---|---|---|
| `schema = p0.report.package.1` | Stable | package report schema |
| `source.component` | Stable | 当前为 `slicer_core` |
| `source.packageDir` | Comparable | package 输出路径，不做跨机器 exact match |
| `configSnapshot.schema` | Stable | package schema snapshot |
| `configSnapshot.configPath` / `modelPath` | Comparable | 来源追踪字段 |
| `stats` / `warnings` / `errors` / `timings` | Stable container | 容器必须存在，具体字段可扩展 |

### 7.2 Slice Report

| 字段 | 等级 | 说明 |
|---|---|---|
| `slicingMode` | Stable | 切片模式 |
| `grid.*` | Stable | 与 manifest grid 对齐 |
| `totals.modelPrintPixels` | Stable | 模型打印像素总量 |
| `totals.supportPrintPixels` | Stable | 支撑打印像素总量 |
| `totals.rgbPrintPixels` | Stable | RGB 打印像素总量 |
| `totals.whitePrintPixels` | Stable | 白墨打印像素总量 |
| `totals.varnishPrintPixels` | Stable | 光油打印像素总量 |
| `totals.channelStats.<R/G/B/W/S/V>` | Stable | 各通道 `printPixels/fullPrintPixels/partialPrintPixels/emptyPixels/minValue/maxValue` |
| `totals.texture.*` | Candidate | 10-3 继续固化 texture fidelity 指标 |
| `totals.materialPolicy.*` | Comparable | 材料策略汇总 |
| `layers[].layerIndex` / `zMm` | Stable | 每层索引和 Z 坐标 |
| `layers[].channelStats.<R/G/B/W/S/V>` | Stable | 每层各通道统计 |
| `layers[].supportTypeStats` | Diagnostic | 支撑类型统计，不进入 TIFF value |
| `layers[].supportConnectivity` | Diagnostic | 支撑连通性诊断 |
| `layers[].fillWarnings` | Diagnostic | 几何采样/填充诊断 |

### 7.3 Texture Report

| 字段 | 等级 | 说明 |
|---|---|---|
| `enabled` | Stable | 是否启用纹理链路 |
| `applyMode` | Stable | 纹理应用模式 |
| `source` | Stable | 纹理来源描述 |
| `materials` / `textureFiles` | Comparable | 资源追踪，不做跨机器 exact path match |
| `loadedTextures` / `missingTextures` | Stable | 资源加载统计 |
| `stats.facesWithUv` / `facesWithoutUv` | Stable | UV 覆盖基础统计 |
| `stats.sampledPixels` | Stable | 采样像素数 |
| `stats.fallbackPixels` | Stable | fallback 像素数 |
| `stats.uvOutOfRangePixels` | Stable | UV 越界像素数 |
| `warnings` | Diagnostic | 纹理诊断 |

### 7.4 Material Reports

| 文件 / 字段 | 等级 | 说明 |
|---|---|---|
| `material_policy_report.enabled` | Stable | 是否启用 MaterialPolicy |
| `material_policy_report.rgb/white/varnish.printPixels` | Comparable | 材料策略通道统计 |
| `material_process_report.profileName` | Comparable | 工艺 profile 名称 |
| `material_process_report.rgb/white/varnish/support.printPixels` | Comparable | 工艺维度汇总统计 |
| `material_process_report.validation.pass` | Diagnostic | 当前 profile 自检结果 |

---

## 8. Experimental Report Boundary

`apps/slicer_cli --experimental-openvdb-shell` 生成的 `outputContract` 是 diagnostic-only 字段，不是 production package。

| 字段 | 等级 | 说明 |
|---|---|---|
| `outputContract.packageSchema` | Stable | 必须保持 `p0.rgbwsv.2` |
| `outputContract.channelOrder` | Stable | 必须保持 `R G B W S V` |
| `outputContract.bitDepth` | Stable | 必须保持 8 |
| `outputContract.polarity` | Stable | 必须保持 `black_is_print` |
| `outputContract.perLayerStats.available=false` | Stable | experimental CLI 当前不写 production package |
| `outputContract.textureFidelity.available=false` | Stable | experimental CLI 当前不执行 texture transfer |

---

## 9. 非契约字段

以下内容不得作为 Stage 10 下游稳定契约：

```text
preview PNG 的具体伪彩颜色；
preview.displayNonZeroPixels / nonZeroPixels 的 UI 显示细节；
绝对路径字符串的 exact match；
contour_report 的内部填充细节；
repair_report 当前的 not_required_p0_lite 文案；
OpenVDB experimental diagnostic path 的 productionPackageWritten=false 之外的生产输出假设；
真实打印颜色、RIP 半色调结果或设备 bitstream。
```

---

## 10. 后续任务衔接

```text
10-2：把 layer summary / channel summary 的字段、统计口径和 golden 可比性继续细化。
10-3：把 textureResolvedRate、uvCoverageRate、fallbackPixelRate 等 texture fidelity 指标正式化。
10-6：把本字段矩阵转成 schema / golden 验证入口。
```
