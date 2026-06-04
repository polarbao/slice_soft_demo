# DEV_03_TIFFWriter与RIPReader协议设计

> 文档版本：v0.1  
> 文档状态：Draft / 协议技术设计  
> 所属模块：切片软件 / TIFF Writer / RIP Reader  
> 输入依据：当前 P0 Demo 实现与 `REPORT_00_P0_Demo当前实现状态.md`

---

## 1. 当前实现状态

当前已实现：

```text
内部最小 TIFF writer
内部最小 TIFF reader
little-endian classic TIFF
tiled storage
six-channel
uint16
contiguous planar config
每层一个 TIFF
rip_reader_test 校验数据包
```

当前没有依赖外部 `libtiff`，用于 P0 Demo 验证。

---

## 2. 技术目标

DEV_03 的目标是将当前 TIFF writer / reader 变成稳定协议实现。

目标：

```text
稳定写入 RGBWSV TIFF
稳定读取 RGBWSV TIFF
校验 manifest 和 layer 一致性
输出明确错误码
输出 channel checksum
为后续 RIP 接入提供协议边界
```

---

## 3. Writer 设计

### 3.1 输入

```cpp
struct LayerBuffer {
    int width;
    int height;
    std::vector<uint16_t> data; // interleaved RGBWSV
};
```

数据布局：

```text
index = ((y * width + x) * 6 + channelIndex)
```

通道索引：

```text
0 R
1 G
2 B
3 W
4 S
5 V
```

### 3.2 TIFF 参数

```text
ByteOrder = little-endian
SamplesPerPixel = 6
BitsPerSample = 16
SampleFormat = unsigned int
PlanarConfig = contiguous
Tiled = true
TileWidth = config.tileSize[0]
TileHeight = config.tileSize[1]
```

### 3.3 文件命名

```text
layers/layer_000001.tif
layers/layer_000002.tif
```

建议 layer index 与 manifest 中的 layerIndex 保持一致。

---

## 4. Reader 设计

### 4.1 输入

```text
SlicePackage/
  manifest.json
  layers/
```

### 4.2 校验流程

```text
1. 读取 manifest
2. 校验 schemaVersion
3. 校验 channelOrder
4. 遍历 layers
5. 读取 TIFF header
6. 校验 width/height/samples/bitDepth/planarConfig/tile
7. 计算 channel checksum
8. 输出验证结果
```

### 4.3 Channel checksum

建议每层输出：

```json
{
  "layerIndex": 1,
  "checksum": {
    "R": 123,
    "G": 123,
    "B": 123,
    "W": 0,
    "S": 456,
    "V": 0
  }
}
```

checksum 初期可以使用简单 sum64：

```cpp
uint64_t checksum = 0;
for each pixel channel:
    checksum += value;
```

后续可替换为 CRC64 / xxHash。

---

## 5. Manifest Schema

建议将当前 `p0.rgbwsv.1` 固化。

字段：

```json
{
  "protocol": "PrivateMultiChannelTiff",
  "schemaVersion": "p0.rgbwsv.1",
  "channelOrder": ["R", "G", "B", "W", "S", "V"],
  "bitDepth": 16,
  "sampleFormat": "uint",
  "planarConfig": "contiguous",
  "tiled": true,
  "tileSize": [256, 256],
  "dpiX": 600,
  "dpiY": 600,
  "layerThicknessMm": 0.01,
  "widthPx": 0,
  "heightPx": 0,
  "totalLayers": 0
}
```

---

## 6. 错误处理

建议定义：

```cpp
enum class RipReadError {
    None,
    ManifestMissing,
    UnsupportedProtocol,
    UnsupportedSchema,
    InvalidChannelOrder,
    InvalidBitDepth,
    InvalidPlanarConfig,
    LayerMissing,
    LayerSizeMismatch,
    TiffReadFailed,
    ChecksumFailed
};
```

CLI 输出必须清楚：

```text
ERROR RIP_SLICE_LAYER_MISSING: layers/layer_000123.tif
```

---

## 7. libtiff 评估边界

当前内部 TIFF writer 可以保留用于 P0/P0+。

后续评估 `libtiff` 的触发条件：

```text
需要 BigTIFF
需要压缩
需要更复杂 tag
需要跨工具强兼容
RIP 侧开始接入标准 TIFF 工具链
```

在接入 libtiff 前，必须保证：

```text
输出文件与当前 schema 一致
channelOrder 不变
rip_reader_test 仍可通过
```

---

## 8. 测试计划

### 8.1 正常样例

```text
单层小图
多层小图
真实模型输出
support-only layer
model-only layer
空 layer
```

### 8.2 错误样例

```text
缺失 manifest
缺失 layer
错误 channelOrder
错误 bitDepth
错误 planarConfig
尺寸不一致
损坏 TIFF
```

### 8.3 自动化建议

```text
ctest
golden manifest
golden TIFF checksum
negative test packages
```

---

## 9. 后续扩展

可能扩展：

```text
BigTIFF
Compression
Alpha
MaterialId
SurfaceType
SupportType
PrivateTag
colorSpace / ICC
```

扩展规则：

```text
任何破坏性变更必须提升 schemaVersion
任何新增通道必须保持向后兼容或提供明确版本判断
```

---

## 10. 结论

当前内部 TIFF writer/reader 已足够支撑 P0 Demo。下一步不是立刻替换为 libtiff，而是先固化协议、增加错误用例、增强 checksum 和 manifest schema。
