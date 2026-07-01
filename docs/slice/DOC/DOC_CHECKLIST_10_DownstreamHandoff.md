# DOC_CHECKLIST_10_DownstreamHandoff

> 文档版本：v0.1
> 文档状态：DOC / Stage 10-5
> 生成日期：2026-07-01
> 任务：Task 10-5 Downstream handoff checklist

---

## 1. 目标

本清单定义 SliceSoft 交付给下游 RIP 工程师的最小 package、manifest、report、限制说明和反馈入口。

Stage 10 的交付目标是让下游可以稳定读取 RGBWSV 切片数据并反馈需求；本清单不要求 SliceSoft 实现 RIP 半色调、设备通信、喷头 bitstream 或真实打印校准。

---

## 2. 交付包必须包含

| 类别 | 路径 / 内容 | 必需性 | 说明 |
|---|---|---|---|
| Package root | `<packageDir>/` | 必需 | 一次切片输出根目录 |
| Manifest | `manifest.json` | 必需 | RIP reader 的入口文件 |
| TIFF layers | `layers/layer_XXXXXX.tiff` | 必需 | 生产 RGBWSV 层数据 |
| Package report | `reports/package_report.json` | 必需 | package 级元数据和 config snapshot |
| Slice report | `reports/slice_report.json` | 必需 | layer summary / channel summary |
| Texture report | `reports/texture_report.json` | 纹理模型必需 | texture fidelity 基础统计 |
| Model report | `reports/model_report.json` | 必需 | 输入模型、bbox、UV、材质诊断 |
| 3MF report | `reports/three_mf_report.json` | 3MF 必需 | 3MF material / texture resource 诊断 |
| Material policy report | `reports/material_policy_report.json` | 材料策略必需 | RGB/W/V/S 策略和输出统计 |
| Support report | `reports/support_report.json` | 支撑模型必需 | 支撑形态、连通性、SupportType 诊断 |
| Preview | `preview/*.png` | 可选 | 人工检查，不是生产输入 |
| Known limitations | 本清单第 8 节 | 必需 | 解释不可过度依赖的内容 |

---

## 3. 下游优先校验顺序

下游 RIP 工程师建议按以下顺序读取：

```text
1. 读取 manifest.json；
2. 校验 schema / grid / TIFF protocol；
3. 校验 layers[] 数量、索引、路径、宽高；
4. 逐层读取 TIFF，确认 samples per pixel、bit depth、storage mode；
5. 对照 reports/slice_report.json 校验 layer/channel summary；
6. 对纹理模型读取 reports/texture_report.json 和 reports/model_report.json；
7. 对 3MF 模型读取 reports/three_mf_report.json；
8. 对白墨、光油、支撑模型读取 material / support report；
9. 记录不满足项，按第 10 节反馈。
```

---

## 4. 固定生产协议

以下字段必须稳定：

```text
manifest.schema = p0.rgbwsv.2
manifest.tiff.channelOrder = ["R", "G", "B", "W", "S", "V"]
manifest.tiff.channelCount = 6
manifest.tiff.bitDepth = 8
manifest.tiff.sampleFormat = uint
manifest.tiff.planarConfig = contiguous
manifest.tiff.storageMode = stripped | tiled
manifest.tiff.polarity = black_is_print
manifest.tiff.printValue = 0
manifest.tiff.emptyValue = 255
```

像素解释：

```text
0 = 打印；
255 = 不打印 / 空白；
1..254 = 半强度打印值，当前不代表 RIP 半色调；
channel 0..5 = R G B W S V；
SupportType 只能出现在 report / metadata，不能编码为 TIFF value。
```

---

## 5. 下游可依赖字段

### 5.1 Manifest

```text
schema
schemaVersion
source.format
grid.widthPx
grid.heightPx
grid.layerCount
grid.dpiX / dpiY
grid.pixelSizeXmm / pixelSizeYmm
grid.layerThicknessMm
tiff.*
layers[].index
layers[].zMm
layers[].path
layers[].widthPx / heightPx
```

### 5.2 Slice Report

```text
layers[].layerIndex
layers[].zMm
layers[].rgbPrintPixels
layers[].whitePrintPixels
layers[].supportPrintPixels
layers[].varnishPrintPixels
layers[].channelStats.R/G/B/W/S/V
totals.rgbPrintPixels
totals.whitePrintPixels
totals.supportPrintPixels
totals.varnishPrintPixels
totals.channelStats.R/G/B/W/S/V
```

### 5.3 Texture / Model / 3MF Reports

```text
texture_report.enabled
texture_report.applyMode
texture_report.loadedTextures
texture_report.missingTextures
texture_report.stats.facesWithUv
texture_report.stats.facesWithoutUv
texture_report.stats.sampledPixels
texture_report.stats.fallbackPixels
texture_report.stats.uvOutOfRangePixels
model_report.facesWithUv
model_report.facesWithoutUv
three_mf_report.validation.invalidReferenceCount
three_mf_report.colorGroups.*
three_mf_report.textures.*
```

---

## 6. 推荐交付模型

下游联调最小模型集来自 `DEMO_10_RealModelAcceptanceSet.md`：

| ID | 用途 | 下游关注 |
|---|---|---|
| `g1_obj_textured_relief` | OBJ/MTL/PNG 纹理和支撑 | RGB 与 S 通道共存，texture fidelity 可解释 |
| `g2_3mf_colorgroup` | 3MF ColorGroup | color group 字段和 RGB 输出 |
| `g2_3mf_texture2d_checker` | 3MF Texture2DGroup | 纹理资源加载和 RGB 输出 |
| `g3_rgb_white_varnish_support` | RGB/W/V/S 材料策略 | 四类材料通道统计和优先级 |
| `g4_missing_texture` | negative fixture | missing texture fallback 是否被报告 |
| `g4_no_uv` | negative fixture | UV 缺失是否被报告 |
| `g5_real_3mf_01` | 真实 3MF | 真实模型 RGB/S 输出和 layer summary |
| `g5_real_3mf_02` | 真实 3MF | 支撑随拱形收敛的统计 |
| `g5_real_3mf_03_texture` | 真实 3MF texture | top surface texture 和 fallback 风险 |

experimental OpenVDB 模型只用于诊断，不作为下游生产包接收标准。

---

## 7. 下游反馈模板

下游反馈应至少包含：

```text
packageDir
manifest.schema
manifest.grid.widthPx / heightPx / layerCount
tiff.storageMode
failedLayerIndex
failedChannel
expectedValue
actualValue
relatedReportPath
readerErrorCode
isBlockingForRip
requestedContractChange
samplePackageAvailable
```

建议分类：

| 类型 | 示例 | SliceSoft 处理方式 |
|---|---|---|
| `protocol_mismatch` | channel order、bit depth、polarity 不匹配 | 必须阻断 |
| `layer_mismatch` | layer list 和 TIFF 文件不一致 | 必须阻断 |
| `stats_mismatch` | TIFF 计数与 slice_report 不一致 | 必须阻断 |
| `missing_metadata` | 下游需要额外稳定字段 | 进入 Stage 10/11 backlog |
| `diagnostic_request` | 需要更细 warning / issue code | 进入 report enhancement |
| `preview_confusion` | 误用 preview PNG | 文档澄清，不改变生产数据 |
| `rip_policy_request` | 半色调、喷头映射、ICC | 转交 RIP / 设备阶段 |

---

## 8. 已知限制

```text
preview PNG 不是生产数据；
真实颜色准确性不由 Stage 10 验收；
ICC、打印机校准、RIP 半色调不在本阶段；
OpenVDB surface shell 仍是 experimental diagnostic path；
绝对路径不做 cross-machine exact match；
warnings 文案不作为 hard gate；
texture fallback fixture 不能代表 production-safe 模型；
materialBindingCoverage 仍是 Candidate 指标。
```

---

## 9. Handoff 完成判定

一次 handoff 只有在以下事项齐备时才算完成：

```text
1. 下游收到至少一个 production package；
2. manifest / TIFF 协议通过 reader；
3. slice_report 与 TIFF channel stats 可对齐；
4. 纹理模型提供 texture fidelity summary；
5. 3MF 模型提供 three_mf_report；
6. negative fixture 的不可 production-safe 原因清楚；
7. 下游反馈按第 7 节模板记录；
8. 任何 RIP/device 需求未误并入 slicer_core。
```

---

## 10. 后续任务衔接

```text
10-6：将本 handoff checklist 的核心字段转为 schema / golden 验证；
10-7：REPORT_10 记录 handoff 是否完整、下游待确认项和是否进入 11；
11：UI layer preview 读取 output contract 展示，不能要求下游使用 preview PNG 作为生产输入。
```
