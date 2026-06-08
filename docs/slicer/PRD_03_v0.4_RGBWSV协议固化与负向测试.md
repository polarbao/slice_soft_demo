# PRD_03_v0.4_RGBWSV协议固化与负向测试

> 文档版本：v0.4  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_02 之后  
> 所属模块：Slicer / Protocol / RIP Reader  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

当前项目已完成：

```text
00B：uint8 + black_is_print
00C：relief_heightfield + V 光油 + S 支撑
01：Relief 正式样例路线
02：支撑生成、孤岛检测与 SupportType 元数据扩展
```

当前输出数据已经被多个路径验证：

```text
普通 P0
Relief V/W/RGB
bottom_projection
unsupported_only
bottom_projection_plus_unsupported
support island filter
```

因此 03 阶段应把当前输出协议固化为正式输入契约。

---

## 2. 03 阶段目标

03 的目标：

```text
让 RGBWSV TIFF package 成为可被 RIP 阶段稳定读取、严格校验、明确报错、可回归测试的数据包格式。
```

核心工作：

```text
1. Manifest schema 固化
2. TIFF tag / metadata 校验固化
3. RIP Reader 校验顺序固化
4. 错误码体系
5. 负向测试包
6. 回归脚本
7. 统计字段命名统一
```

---

## 3. 协议冻结项

03 不允许修改：

```text
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

SupportType 继续保持：

```text
只进入 report / metadata / debug
不进入 TIFF 通道
不增加 SamplesPerPixel
```

---

## 4. 通道语义

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
当前阶段主要为二值材料：
0 = 打印
255 = 不打印
```

未来如支持灰度墨量：

```text
1-254 可表示部分输出
但必须显式更新 schema 或材料策略
```

---

## 5. Manifest Schema

03 应固定：

```text
schema = p0.rgbwsv.1
```

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
    "pixelSizeXmm": 0.0,
    "pixelSizeYmm": 0.0,
    "layerThicknessMm": 0.01,
    "layerCount": 0
  },
  "slicing": {
    "mode": "closed_mesh_scanline",
    "reliefFillMode": null
  },
  "layers": [],
  "reports": {}
}
```

---

## 6. Layer 列表规则

Manifest 中应优先记录 `layers` 数组：

```json
{
  "layers": [
    {
      "index": 0,
      "path": "layers/layer_000000.tif",
      "widthPx": 0,
      "heightPx": 0
    }
  ]
}
```

Reader 优先按 manifest layer list 读取。

如果没有 layer list，可兼容旧命名规则，但必须输出 warning。

---

## 7. TIFF 校验要求

每层 TIFF 必须满足：

```text
ImageWidth == manifest.grid.widthPx
ImageLength == manifest.grid.heightPx
SamplesPerPixel == 6
BitsPerSample == 8
PlanarConfig == contiguous
SampleFormat == uint 或 TIFF 默认 unsigned byte
TileWidth / TileLength 可读
```

Tile padding 必须是：

```text
255
```

用于保证 tile 边缘补齐区域不被误判为打印。

---

## 8. RIP Reader 错误码

Reader 应输出稳定错误码。

建议错误码：

```text
E_PACKAGE_NOT_FOUND
E_MANIFEST_MISSING
E_MANIFEST_PARSE_FAILED
E_SCHEMA_UNSUPPORTED
E_CHANNEL_ORDER_INVALID
E_CHANNEL_COUNT_INVALID
E_BIT_DEPTH_INVALID
E_POLARITY_INVALID
E_PRINT_EMPTY_VALUE_INVALID
E_GRID_INVALID
E_LAYER_LIST_INVALID
E_LAYER_COUNT_MISMATCH
E_LAYER_MISSING
E_LAYER_SIZE_MISMATCH
E_TIFF_OPEN_FAILED
E_TIFF_SAMPLE_COUNT_INVALID
E_TIFF_BIT_DEPTH_INVALID
E_TIFF_PLANAR_CONFIG_INVALID
E_TIFF_READ_FAILED
```

错误信息必须包含：

```text
错误码
字段路径
期望值
实际值
文件路径
```

---

## 9. 负向测试要求

必须构造 bad packages：

```text
bad_missing_manifest
bad_manifest_parse
bad_schema
bad_bit_depth
bad_channel_order
bad_channel_count
bad_polarity
bad_print_value
bad_empty_value
bad_grid
bad_missing_layer
bad_layer_size
bad_samples_per_pixel
bad_planar_config
```

每个负向测试应支持：

```powershell
rip_reader_test.exe --package <bad_package> --expect-error --expect-message <keyword>
```

如果实现错误码参数，则支持：

```powershell
rip_reader_test.exe --package <bad_package> --expect-error --expect-code E_BIT_DEPTH_INVALID
```

---

## 10. 统计字段规范

03 后新报告应使用：

```text
printPixels
fullPrintPixels
partialPrintPixels
emptyPixels
minValue
maxValue
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

不应作为新文档主字段。

如果代码仍保留该字段，只作为 backward compatibility。

---

## 11. Preview 与 Production 分离

生产数据：

```text
0 = 打印
255 = 不打印
```

Preview 显示：

```text
visible = 255 - productionValue
```

Preview 可使用伪彩色：

```text
support = green
varnish = purple
white = gray / white
rgb = RGB
island = red
unsupported = orange
```

Preview 文件不得作为 RIP 输入。

---

## 12. 回归测试范围

03 回归必须覆盖：

```text
普通 P0 package
Relief V + S
Relief W + S
Relief RGB
SupportBottomProjection
SupportUnsupportedOnly
SupportBottomPlusUnsupported
SupportIslandFilter
Bad packages
```

---

## 13. 验收标准

1. Manifest 写入 `schema = p0.rgbwsv.1`。
2. Reader 严格校验 schema。
3. Reader 严格校验 channelOrder / bitDepth / polarity。
4. Reader 校验 TIFF metadata。
5. Reader 能识别 missing layer / wrong layer size。
6. 负向测试能按预期失败。
7. 错误信息包含错误码或明确字段关键词。
8. 统计字段以 printPixels 为主。
9. 回归脚本覆盖 P0 / Relief / Support / Bad package。
10. 不改变任何生产协议字段。
11. 不破坏 02 支撑样例。

---

## 14. 非目标

03 不做：

```text
RIP 半色调
CMYK 分色
喷头 bitstream
ICC 色彩管理
墨量曲线
彩色纹理
3MF
OpenVDB
Qt UI
复杂支撑树
```

---

## 15. 结论

03 阶段不是新增切片能力，而是将当前切片输出数据包固化为稳定、可验证、可拒绝错误输入的 RIP 前置协议。
