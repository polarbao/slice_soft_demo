# DEV_03_v0.4_TIFFWriter_RIPReader协议固化设计

> 文档版本：v0.4  
> 文档状态：Draft / DEV  
> 适用阶段：PRD_03  
> 所属模块：Slicer / Protocol / RIP Reader  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

在不改变现有 RGBWSV 输出协议的前提下，增强：

```text
manifest schema
TIFF writer metadata
RIP reader 校验
错误码
负向测试
回归脚本
统计字段
```

---

## 2. 推荐代码边界

短期可以保留现有文件，但建议形成逻辑边界：

```text
src/slicer_core/
  manifest.*
  tiff_io.*
  rip_reader.*
  protocol/
    rgbwsv_protocol.*
    validation_error.*
```

如果当前不拆目录，至少应形成函数边界：

```text
write_manifest_schema
validate_manifest_schema
validate_protocol_fields
validate_layer_list
validate_tiff_metadata
collect_channel_stats
```

---

## 3. Protocol Struct

建议新增或等价实现：

```cpp
struct RgbwsvProtocol {
    std::string schema{"p0.rgbwsv.1"};
    std::array<std::string, 6> channel_order{"R", "G", "B", "W", "S", "V"};
    int channel_count{6};
    int bit_depth{8};
    std::string sample_format{"uint"};
    std::string planar_config{"contiguous"};
    bool tiled{true};
    std::string polarity{"black_is_print"};
    int print_value{0};
    int empty_value{255};
};
```

---

## 4. Writer 修改

### 4.1 Manifest Writer

Manifest writer 应写入：

```text
schema
tiff protocol fields
grid fields
slicing fields
layer list
reports
```

### 4.2 TIFF Writer

必须保证：

```text
BitsPerSample = 8
SamplesPerPixel = 6
PlanarConfig = contiguous
Tile padding = 255
```

Tile padding 不允许使用 0。

---

## 5. RIP Reader 校验顺序

建议校验顺序：

```text
1. package directory exists
2. manifest exists
3. manifest parse
4. schema supported
5. tiff protocol fields
6. grid fields
7. layer list
8. layer file exists
9. TIFF metadata
10. TIFF dimensions
11. channel stats
```

这样可以保证错误定位清晰。

---

## 6. ValidationError

建议新增：

```cpp
enum class ValidationErrorCode {
    PackageNotFound,
    ManifestMissing,
    ManifestParseFailed,
    SchemaUnsupported,
    ChannelOrderInvalid,
    ChannelCountInvalid,
    BitDepthInvalid,
    PolarityInvalid,
    PrintEmptyValueInvalid,
    GridInvalid,
    LayerListInvalid,
    LayerCountMismatch,
    LayerMissing,
    LayerSizeMismatch,
    TiffOpenFailed,
    TiffSampleCountInvalid,
    TiffBitDepthInvalid,
    TiffPlanarConfigInvalid,
    TiffReadFailed
};
```

错误字符串建议格式：

```text
E_BIT_DEPTH_INVALID: manifest.tiff.bitDepth expected 8, actual 16
```

---

## 7. Negative Package 生成

### 7.1 静态目录

```text
tests/packages/bad/
  bad_missing_manifest/
  bad_schema/
  bad_bit_depth/
  bad_channel_order/
  bad_layer_size/
```

### 7.2 动态脚本

建议新增：

```text
scripts/make_bad_packages.ps1
```

逻辑：

```text
以一个 good package 为模板
复制 package
修改 manifest 或 layer
生成 bad package
```

优先实现动态脚本，避免维护大量二进制测试文件。

---

## 8. CLI 增强

当前 `rip_reader_test` 已支持：

```text
--expect-error
--expect-message
```

建议增加可选：

```text
--expect-code
```

不是必须，但推荐。

示例：

```powershell
rip_reader_test.exe --package tests/packages/bad/bad_bit_depth --expect-error --expect-message bitDepth
rip_reader_test.exe --package tests/packages/bad/bad_bit_depth --expect-error --expect-code E_BIT_DEPTH_INVALID
```

---

## 9. ChannelStats

建议统一统计：

```cpp
struct ChannelStats {
    std::uint64_t print_pixels;
    std::uint64_t full_print_pixels;
    std::uint64_t partial_print_pixels;
    std::uint64_t empty_pixels;
    int min_value;
    int max_value;
};
```

计算：

```cpp
if (value == emptyValue) empty_pixels++;
else print_pixels++;

if (value == printValue) full_print_pixels++;
else if (value > printValue && value < emptyValue) partial_print_pixels++;
```

---

## 10. Regression Script

新增：

```text
scripts/run_regression.ps1
```

覆盖：

```text
1. cmake build
2. ordinary P0
3. Relief packages
4. Support packages
5. Positive rip_reader_test
6. Negative rip_reader_test
7. summary table
```

---

## 11. 兼容策略

Reader 对 unknown optional fields：

```text
允许忽略
```

Reader 对核心字段：

```text
必须严格校验
```

不兼容 schema：

```text
拒绝读取
```

---

## 12. 实施顺序

```text
1. Manifest writer 加 schema
2. Reader 校验 schema / protocol fields
3. TIFF metadata 校验增强
4. 错误码封装
5. ChannelStats 重命名 / 增强
6. Bad package 生成脚本
7. Regression script
8. REPORT_03
```

---

## 13. 不做内容

DEV_03 不做：

```text
RIP 半色调
喷头数据格式
ICC 色彩管理
墨量曲线
彩色纹理
OpenVDB
Qt UI
```

---

## 14. 结论

DEV_03 v0.4 是协议固化阶段，不是格式变更阶段。

最重要的是：

```text
严格校验
明确错误
负向测试
回归脚本
不破坏 02 baseline
```
