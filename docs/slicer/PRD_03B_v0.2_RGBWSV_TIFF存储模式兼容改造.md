# PRD_03B_v0.2_RGBWSV_TIFF存储模式兼容改造

> 文档版本：v0.2  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_05 之后 / 03B  
> 所属模块：TIFF Writer / RIP Reader / Manifest  
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

03B 阶段目标是让切片输出包同时支持：

```text
stripped / scanline-friendly TIFF
tiled / block-oriented TIFF
```

并将默认输出改为：

```text
schema = p0.rgbwsv.2
storageMode = stripped
rowsPerStrip = 64
```

同时保持对旧包的读取兼容：

```text
p0.rgbwsv.1 legacy tiled package
```

---

## 2. 为什么现在执行

05 已完成材料策略基础版，当前已有：

```text
RGB texture only
RGB + W underbase
RGB + V top_n_layers
RGB + W + V
V only
W only
```

这些样例已经成为新的输出基线。

03B 现在执行，可以解决 RIP scanline 读取兼容问题，同时避免在 06/3MF 或 05A 真实参数验证阶段继续被 TIFF storage 问题干扰。

---

## 3. 功能范围

### 3.1 必须支持

```text
1. output.storageMode = stripped / tiled
2. output.rowsPerStrip
3. output.tileSize
4. TIFFWriter 写 stripped TIFF
5. TIFFWriter 保留 tiled TIFF
6. RIPReader 读 stripped TIFF
7. RIPReader 读 tiled TIFF
8. Manifest 写 storageMode
9. run_regression.ps1 覆盖 stripped/tiled
10. MaterialPolicy 六个样例在 stripped 默认模式下全部通过
11. 至少一个 MaterialPolicy tiled compatibility 样例通过
12. bad package 覆盖 storage mismatch
```

### 3.2 默认配置

```json
{
  "output": {
    "storageMode": "stripped",
    "rowsPerStrip": 64
  }
}
```

### 3.3 Tiled 兼容配置

```json
{
  "output": {
    "storageMode": "tiled",
    "tileSize": [256, 256]
  }
}
```

---

## 4. Manifest 需求

03B 后 Writer 默认写：

```json
{
  "schema": "p0.rgbwsv.2",
  "tiff": {
    "storageMode": "stripped",
    "tiled": false,
    "rowsPerStrip": 64,
    "channelOrder": ["R", "G", "B", "W", "S", "V"],
    "channelCount": 6,
    "bitDepth": 8,
    "sampleFormat": "uint",
    "planarConfig": "contiguous",
    "polarity": "black_is_print",
    "printValue": 0,
    "emptyValue": 255
  }
}
```

Tiled package 写：

```json
{
  "schema": "p0.rgbwsv.2",
  "tiff": {
    "storageMode": "tiled",
    "tiled": true,
    "tileSize": [256, 256]
  }
}
```

---

## 5. Reader 兼容需求

Reader 必须支持：

```text
p0.rgbwsv.1:
  legacy tiled package

p0.rgbwsv.2:
  stripped package
  tiled package
```

Reader 读取时必须以实际 TIFF 结构为准：

```text
if TIFFIsTiled:
  read_tiled_tiff
else:
  read_stripped_or_scanline_tiff
```

Manifest 的 `storageMode` 用于一致性校验，不应用于盲读。

---

## 6. Writer 需求

Writer 根据配置分流：

```text
storageMode = stripped:
  write_stripped_tiff

storageMode = tiled:
  write_tiled_tiff
```

Stripped writer 设置：

```text
ROWSPERSTRIP = rowsPerStrip
```

Tiled writer 继续保证：

```text
tile padding = 255
```

---

## 7. MaterialPolicy 回归需求

03B 必须确认 storageMode 改造不改变材料输出语义。

默认 stripped 模式必须覆盖：

```text
MaterialPolicyRgbOnly
MaterialPolicyRgbWhiteUnderbase
MaterialPolicyRgbVarnishTop2
MaterialPolicyRgbWhiteVarnish
MaterialPolicyVarnishOnly
MaterialPolicyWhiteOnly
```

Tiled 兼容模式至少覆盖：

```text
MaterialPolicyRgbWhiteVarnish
```

用于验证多通道组合在 tiled 模式下仍可读取。

---

## 8. 错误与负向测试

新增错误或错误信息：

```text
E_TIFF_STORAGE_MODE_INVALID
E_TIFF_STORAGE_MISMATCH
E_ROWS_PER_STRIP_INVALID
E_TILE_SIZE_INVALID
```

负向测试：

```text
bad_storage_mode
bad_rows_per_strip
bad_tiff_storage_mismatch
bad_tile_size
```

---

## 9. 验收标准

1. 默认配置输出 stripped TIFF。
2. 默认 manifest 写 `schema = p0.rgbwsv.2`。
3. 默认 manifest 写 `storageMode = stripped`。
4. RIP Reader 可读取 stripped TIFF。
5. RIP Reader 可读取 tiled TIFF。
6. 旧 `p0.rgbwsv.1` tiled package 仍可读取。
7. `Can not read scanlines from a tiled image` 不再出现在合法 package 读取流程中。
8. run_regression.ps1 默认覆盖 MaterialPolicy 六个 stripped 样例。
9. run_regression.ps1 至少覆盖一个 tiled compatibility 样例。
10. RGB/W/V/S printPixels 语义与 REPORT_05 基线一致。
11. RGBWSV 通道语义不变。

---

## 10. 非目标

03B 不做：

```text
材料策略修改
纹理采样修改
真实模型材料参数调优
RIP 半色调
CMYK
ICC
3MF
OpenVDB
Qt UI
支撑形态修复
```

---

## 11. 结论

03B 是 TIFF 物理存储兼容改造。

它的核心目标是：

```text
默认按行友好；
保留分块能力；
不破坏 05 材料策略结果。
```
