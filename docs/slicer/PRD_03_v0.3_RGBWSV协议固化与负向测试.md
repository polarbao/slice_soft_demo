# PRD_03_v0.3_RGBWSV协议固化与负向测试

> 文档版本：v0.3  
> 文档状态：Draft / PRD 强化版  
> 适用阶段：REPORT_01 后，可与 PRD_02 并行阅读  
> 建议提交目录：`docs/slicer/`

---

## 1. 目标

固化当前已经稳定的 RGBWSV TIFF / manifest / RIP reader 协议，形成后续彩色纹理、多材料、RIP 对接的稳定输入契约。

---

## 2. 协议冻结项

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
planarConfig = contiguous
tiled = true
tile padding = 255
```

---

## 3. 通道语义

| 通道 | 语义 | 打印值 | 空白值 |
|---|---|---:|---:|
| R | RGB Red | 0-254 | 255 |
| G | RGB Green | 0-254 | 255 |
| B | RGB Blue | 0-254 | 255 |
| W | White Ink | 0 | 255 |
| S | Support | 0 | 255 |
| V | Varnish | 0 | 255 |

说明：

```text
当前 P0+/Relief 阶段以二值材料为主：
0 = 打印
255 = 不打印

未来若支持灰度墨量，可使用 1-254 表示中间输出，但必须显式写入协议版本或材料策略。
```

---

## 4. Manifest Schema

Manifest 必须包含：

```json
{
  "schema": "p0.rgbwsv.1",
  "tiff": {
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "channelCount": 6,
    "bitDepth": 8,
    "sampleFormat": "uint",
    "planarConfig": "contiguous",
    "tiled": true,
    "polarity": "black_is_print",
    "printValue": 0,
    "emptyValue": 255
  },
  "grid": {
    "widthPx": 0,
    "heightPx": 0,
    "dpiX": 600,
    "dpiY": 600,
    "pixelSizeXmm": 0.0423333333,
    "pixelSizeYmm": 0.0423333333,
    "layerThicknessMm": 0.01,
    "layerCount": 0
  },
  "slicing": {
    "mode": "relief_heightfield",
    "reliefFillMode": "intersection_range"
  },
  "layers": [],
  "reports": {}
}
```

---

## 5. Layer 文件规则

推荐命名：

```text
layers/layer_000000.tif
layers/layer_000001.tif
...
```

Reader 应以 manifest 中记录的 layer 列表为准。

如果 manifest 没有 layer 列表，可降级按命名规则读取，但应输出 warning。

---

## 6. TIFF 要求

每个 layer TIFF 必须满足：

```text
ImageWidth = manifest.grid.widthPx
ImageLength = manifest.grid.heightPx
SamplesPerPixel = 6
BitsPerSample = 8
PlanarConfig = contiguous
TileWidth / TileLength 与 manifest 一致或可读
SampleFormat = uint
```

Tile padding 必须为：

```text
255
```

避免 tile 边缘补齐区域被误读为打印。

---

## 7. Reader 错误码

建议错误码：

```text
E_MANIFEST_MISSING
E_SCHEMA_UNSUPPORTED
E_CHANNEL_ORDER_INVALID
E_BIT_DEPTH_INVALID
E_POLARITY_INVALID
E_PRINT_EMPTY_VALUE_INVALID
E_LAYER_COUNT_MISMATCH
E_LAYER_MISSING
E_LAYER_SIZE_MISMATCH
E_TIFF_SAMPLE_COUNT_INVALID
E_TIFF_PLANAR_CONFIG_INVALID
E_TIFF_READ_FAILED
```

错误信息必须包含：

```text
错误码
字段路径
实际值
期望值
```

---

## 8. 负向测试

必须构造 bad packages：

```text
bad_missing_manifest
bad_schema
bad_bit_depth
bad_channel_order
bad_polarity
bad_print_value
bad_empty_value
bad_missing_layer
bad_layer_size
bad_samples_per_pixel
bad_planar_config
```

每个负向测试必须支持：

```powershell
rip_reader_test.exe --package <bad_package> --expect-error --expect-message <keyword>
```

---

## 9. 统计字段规范

推荐字段：

```text
printPixels
fullPrintPixels
partialPrintPixels
emptyPixels
```

定义：

```text
printPixels = value < emptyValue
fullPrintPixels = value == printValue
partialPrintPixels = printValue < value < emptyValue
emptyPixels = value == emptyValue
```

旧字段：

```text
nonZeroPixels
```

不建议继续作为主字段，因为当前 `0 = 打印`。

可短期兼容，但新报告应以 `printPixels` 为准。

---

## 10. Preview 与 Production 分离

生产 TIFF：

```text
0 = 打印
255 = 不打印
```

Preview：

```text
visible = 255 - productionValue
```

Preview 可使用伪彩色：

```text
support = green
varnish = purple
white = gray/white
rgb = visible RGB
island = red
```

Preview 不得作为 RIP 输入。

---

## 11. 兼容策略

Reader 对未知字段：

```text
允许忽略 unknown optional fields
```

Reader 对协议核心字段：

```text
必须严格校验
```

如果 schema major 不兼容：

```text
必须拒绝 package
```

---

## 12. 验收标准

1. 所有正向 package 通过。
2. 所有负向 package 按预期失败。
3. 错误信息包含错误码和字段路径。
4. Manifest 写入 schema = p0.rgbwsv.1。
5. Reader 校验 bitDepth / polarity / channelOrder。
6. Reader 校验 TIFF sample count / size / planar config。
7. Tile padding 为 255。
8. 报告字段使用 printPixels。
9. Preview 与 production 分离。
10. 协议文档可作为 RIP 输入契约。

---

## 13. 非目标

PRD_03 不做：

```text
RIP 半色调
CMYK 分色
喷头 bitstream
ICC 色彩管理
墨量曲线
材料物理策略
```

---

## 14. 结论

PRD_03 v0.3 的目标是把当前已验证协议固化为正式输入契约，而不是改变输出格式。
