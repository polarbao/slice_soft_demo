# PRD_03_RGBWSV多通道TIFF协议

> 文档版本：v0.1  
> 文档状态：Draft / 协议需求文档  
> 所属模块：切片软件 / RIP 输入协议  
> 输入依据：P0 Demo 当前实现与 `REPORT_00_P0_Demo当前实现状态.md`

---

## 1. 背景

当前 P0 Demo 已经实现内部最小 TIFF writer/reader，并由 `rip_reader_test` 完成 RGBWSV 数据包验证。

当前协议已实际运行：

```text
每层一个 TIFF
六通道
uint16
tiled
contiguous
channelOrder = R G B W S V
manifest schema = p0.rgbwsv.1
```

为了避免后续彩色、支撑、白墨、光油、RIP 对接过程中协议漂移，需要将 RGBWSV 多通道 TIFF 固化为正式协议文档。

---

## 2. 协议目标

本协议用于定义：

```text
切片模块输出什么
RIP 如何读取
每个通道代表什么
manifest 必须包含什么
错误如何报告
后续如何扩展
```

---

## 3. 文件组织

切片包结构：

```text
SlicePackage/
  manifest.json
  layers/
    layer_000001.tif
    layer_000002.tif
    ...
  reports/
    model_report.json
    slice_report.json
    repair_report.json
    support_report.json
    preview_report.json
```

---

## 4. TIFF 通道协议

### 4.1 通道顺序

固定为：

```text
R G B W S V
```

不得在 P0 / P0+ 中修改。

### 4.2 通道语义

| 通道 | 名称 | 语义 |
|---|---|---|
| R | Red | 单材料 RGB 或后续彩色目标色 R |
| G | Green | 单材料 RGB 或后续彩色目标色 G |
| B | Blue | 单材料 RGB 或后续彩色目标色 B |
| W | White | 白墨语义强度 |
| S | Support | 支撑语义强度 |
| V | Varnish | 光油语义强度 |

### 4.3 数据类型

```text
uint16
```

取值范围：

```text
0 - 65535
```

### 4.4 planarConfig

固定：

```text
contiguous
```

即像素布局：

```text
pixel0: R G B W S V
pixel1: R G B W S V
...
```

### 4.5 TIFF 存储

固定：

```text
tiled
```

P0/P0+ 可使用 classic TIFF。超过限制后再升级 BigTIFF。

---

## 5. 像素优先级

当模型和支撑在同一像素冲突时：

```text
Model > Support
```

模型像素：

```text
R/G/B = 模型配置色或目标色
W = 白墨强度
S = 0
V = 光油强度
```

支撑像素：

```text
R/G/B/W/V = 0
S = 支撑强度
```

---

## 6. manifest 要求

manifest 必须包含：

```text
protocol
schemaVersion
channelOrder
bitDepth
sampleFormat
planarConfig
tiled
dpiX
dpiY
layerThicknessMm
pixelSizeMm
totalLayers
widthPx
heightPx
sourceModel
support
```

示例：

```json
{
  "protocol": "PrivateMultiChannelTiff",
  "schemaVersion": "p0.rgbwsv.1",
  "channelOrder": ["R", "G", "B", "W", "S", "V"],
  "bitDepth": 16,
  "sampleFormat": "uint",
  "planarConfig": "contiguous",
  "tiled": true,
  "dpiX": 600,
  "dpiY": 600,
  "layerThicknessMm": 0.01,
  "totalLayers": 100,
  "widthPx": 1200,
  "heightPx": 800
}
```

---

## 7. RIP Reader 验收要求

RIP Reader 必须校验：

```text
manifest 是否存在
protocol 是否支持
schemaVersion 是否支持
channelOrder 是否为 R G B W S V
bitDepth 是否为 16
planarConfig 是否为 contiguous
所有 layer 是否存在
所有 layer 尺寸是否一致
所有 layer channel count 是否为 6
```

---

## 8. 错误码建议

```text
RIP_SLICE_MANIFEST_MISSING
RIP_SLICE_UNSUPPORTED_PROTOCOL
RIP_SLICE_UNSUPPORTED_SCHEMA
RIP_SLICE_INVALID_CHANNEL_ORDER
RIP_SLICE_INVALID_BIT_DEPTH
RIP_SLICE_INVALID_PLANAR_CONFIG
RIP_SLICE_LAYER_MISSING
RIP_SLICE_LAYER_SIZE_MISMATCH
RIP_SLICE_TIFF_READ_FAILED
RIP_SLICE_CHECKSUM_FAILED
```

---

## 9. 后续扩展

后续可增加：

```text
Alpha
MaterialId
SurfaceType
SupportType
BigTIFF
Compression
PrivateTag
ICC / colorSpace
```

但不能破坏 P0 基线：

```text
R G B W S V
```

如需扩展，必须提升 schemaVersion。

---

## 10. 结论

RGBWSV 是切片模块与 RIP 之间的核心数据契约。当前 P0/P0+ 必须以协议稳定为优先，不应随意改变通道顺序、位深、planarConfig 或 manifest 核心字段。
