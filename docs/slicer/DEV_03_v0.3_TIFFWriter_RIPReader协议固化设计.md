# DEV_03_v0.3_TIFFWriter_RIPReader协议固化设计

> 文档版本：v0.3  
> 文档状态：Draft / DEV 强化版  
> 适用阶段：PRD_03  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

将当前 TIFF writer / manifest / RIP reader 从 Demo 校验提升为稳定协议组件。

---

## 2. 建议模块边界

```text
src/slicer_core/
  tiff_io.*
  rip_reader.*
  manifest.*
  protocol/
    rgbwsv_protocol.*
    validation_error.*
```

短期可以不拆目录，但建议先建立清晰函数边界：

```text
write_manifest
validate_manifest
validate_tiff_layer_metadata
validate_layer_sequence
collect_channel_stats
```

---

## 3. Manifest 结构

建议内部结构：

```cpp
struct RgbwsvProtocol {
    std::string schema{"p0.rgbwsv.1"};
    std::array<std::string, 6> channel_order{"R","G","B","W","S","V"};
    int channel_count{6};
    int bit_depth{8};
    std::string sample_format{"uint"};
    std::string polarity{"black_is_print"};
    int print_value{0};
    int empty_value{255};
};
```

---

## 4. TIFF Writer 要求

Writer 必须保证：

```text
BitsPerSample = 8
SamplesPerPixel = 6
PlanarConfig = contiguous
Tile padding = 255
Channel order = R G B W S V
```

Layer buffer 推荐：

```cpp
std::vector<std::uint8_t> layer(width * height * 6, 255);
```

---

## 5. RIP Reader 校验顺序

建议顺序：

```text
1. package directory exists
2. manifest exists
3. schema supported
4. grid fields valid
5. tiff protocol fields valid
6. layer list valid
7. each TIFF exists
8. each TIFF metadata valid
9. each layer size valid
10. channel stats collect
```

---

## 6. ValidationError

建议：

```cpp
enum class ValidationErrorCode {
    ManifestMissing,
    SchemaUnsupported,
    ChannelOrderInvalid,
    BitDepthInvalid,
    PolarityInvalid,
    PrintEmptyValueInvalid,
    LayerCountMismatch,
    LayerMissing,
    LayerSizeMismatch,
    TiffSampleCountInvalid,
    TiffPlanarConfigInvalid,
    TiffReadFailed
};
```

错误输出：

```text
E_BIT_DEPTH_INVALID: manifest.tiff.bitDepth expected 8, actual 16
```

---

## 7. 负向测试包生成

建议支持两种方式：

### 7.1 静态 bad package

```text
tests/packages/bad/bad_bit_depth/
```

### 7.2 脚本动态生成

```text
scripts/make_bad_packages.ps1
```

基于一个 good package 拷贝并修改 manifest 或 TIFF。

---

## 8. CLI 行为

已有：

```text
--expect-error
--expect-message
```

建议增加：

```text
--expect-code E_BIT_DEPTH_INVALID
```

但不是必须。

示例：

```powershell
rip_reader_test.exe --package tests/packages/bad/bad_bit_depth --expect-error --expect-message bitDepth
```

---

## 9. Channel Stats

统计函数：

```cpp
ChannelStats collect_channel_stats(layer_data, channel_index);
```

字段：

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

---

## 10. Regression Script

建议新增：

```text
scripts/run_regression.ps1
```

步骤：

```text
1. cmake build
2. run ordinary P0 package
3. run relief packages
4. run support packages
5. run good package reader
6. run bad package reader
7. print summary
```

---

## 11. 兼容策略

Reader：

```text
允许 unknown optional fields
拒绝 unknown required protocol fields
拒绝 unsupported schema
拒绝 bitDepth != 8
拒绝 channelOrder != R G B W S V
```

---

## 12. 不做内容

DEV_03 不做：

```text
RIP 半色调
喷头数据格式
颜色管理
墨量曲线
材质策略
```

---

## 13. 结论

DEV_03 v0.3 重点是：

```text
严格校验
明确错误
负向测试
统计命名统一
回归脚本
```
