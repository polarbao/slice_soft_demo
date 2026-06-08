# REPORT_03_RGBWSV协议固化当前实现状态

> 文档版本：v0.4  
> 文档状态：当前实现状态  
> 阶段范围：03 / RGBWSV 协议固化与负向测试  
> 生成时间：2026-06-05

---

## 1. 阶段结论

03 阶段已按 `PRD_03_v0.4_RGBWSV协议固化与负向测试.md` / `DEV_03_v0.4_TIFFWriter_RIPReader协议固化设计.md` 完成协议固化、reader 严格校验、错误码、bad package 负向测试、统计字段和回归脚本。

本阶段没有新增切片能力，未处理彩色纹理、OpenVDB、Qt UI、复杂支撑树、RIP 半色调。

---

## 2. Manifest 协议状态

当前生产包 manifest 已固化为：

- `schema = p0.rgbwsv.1`
- 保留兼容字段 `schemaVersion = p0.rgbwsv.1`
- `tiff.channelOrder = [R, G, B, W, S, V]`
- `tiff.channelCount = 6`
- `tiff.bitDepth = 8`
- `tiff.sampleFormat = uint`
- `tiff.planarConfig = contiguous`
- `tiff.storage = tiled`
- `tiff.tiled = true`
- `tiff.polarity = black_is_print`
- `tiff.printValue = 0`
- `tiff.emptyValue = 255`
- 顶层 `layers` 与 `tiff.layers` 均记录 layer list
- layer 记录包含 `index`、`path`、`widthPx`、`heightPx`、`tileWidthPx`、`tileHeightPx`
- `grid` 增加 `dpiX`、`dpiY`、`pixelSizeXmm`、`pixelSizeYmm`

输出极性保持 00B 结论：`0 = 打印`，`255 = 不打印 / 空白`。

---

## 3. TIFF Writer 状态

当前 TIFF writer 输出仍为 RGBWSV 六通道 tiled TIFF：

- 每像素通道顺序固定为 `R G B W S V`
- 每通道 `uint8`
- `BitsPerSample = 8`
- `SamplesPerPixel = 6`
- `PlanarConfiguration = contiguous`
- tile 内空白区域与 tile padding 使用 `255`
- 模型区域按配置写入 RGB/W/V 的 8-bit 打印值
- 支撑区域 S 通道写 `0`，其他通道保持 `255`

---

## 4. RIP Reader 校验状态

`rip_reader_test` 现在通过 `slicer_core::ValidationErrorCode` 输出稳定错误码，并支持：

- `--expect-error`
- `--expect-message <text>` 兼容旧行为
- `--expect-code <code>` 用于负向测试精确校验

Reader 已严格校验：

- package 路径存在
- manifest 存在且可解析
- `manifest.schema`
- `tiff.channelOrder`
- `tiff.channelCount`
- `tiff.bitDepth`
- `tiff.sampleFormat`
- `tiff.planarConfig`
- `tiff.storage` / `tiff.tiled`
- `tiff.polarity`
- `tiff.printValue` / `tiff.emptyValue`
- grid 尺寸、层数、DPI、层厚
- layer list 类型与数量
- layer index、path、widthPx、heightPx
- layer 文件存在
- TIFF `SamplesPerPixel`
- TIFF `BitsPerSample`
- TIFF `PlanarConfiguration`
- TIFF image dimensions
- tile padding 是否为 `255`

---

## 5. 错误码状态

当前实现的错误码包括：

- `E_PACKAGE_NOT_FOUND`
- `E_MANIFEST_MISSING`
- `E_MANIFEST_PARSE_FAILED`
- `E_SCHEMA_UNSUPPORTED`
- `E_CHANNEL_ORDER_INVALID`
- `E_CHANNEL_COUNT_INVALID`
- `E_BIT_DEPTH_INVALID`
- `E_POLARITY_INVALID`
- `E_PRINT_EMPTY_VALUE_INVALID`
- `E_GRID_INVALID`
- `E_LAYER_LIST_INVALID`
- `E_LAYER_COUNT_MISMATCH`
- `E_LAYER_MISSING`
- `E_LAYER_SIZE_MISMATCH`
- `E_TIFF_OPEN_FAILED`
- `E_TIFF_SAMPLE_COUNT_INVALID`
- `E_TIFF_BIT_DEPTH_INVALID`
- `E_TIFF_PLANAR_CONFIG_INVALID`
- `E_TIFF_READ_FAILED`

错误消息包含 code 前缀，并尽量包含 field、expected、actual、path，便于自动化测试和人工定位。

---

## 6. Bad Package 负向测试状态

新增脚本：

```powershell
.\scripts\make_bad_packages.ps1 -GoodPackage output\SlicePackage -OutputRoot tests\packages\bad
```

当前生成并验证的坏包：

- `bad_missing_manifest` -> `E_MANIFEST_MISSING`
- `bad_manifest_parse` -> `E_MANIFEST_PARSE_FAILED`
- `bad_schema` -> `E_SCHEMA_UNSUPPORTED`
- `bad_bit_depth` -> `E_BIT_DEPTH_INVALID`
- `bad_channel_order` -> `E_CHANNEL_ORDER_INVALID`
- `bad_channel_count` -> `E_CHANNEL_COUNT_INVALID`
- `bad_polarity` -> `E_POLARITY_INVALID`
- `bad_print_value` -> `E_PRINT_EMPTY_VALUE_INVALID`
- `bad_empty_value` -> `E_PRINT_EMPTY_VALUE_INVALID`
- `bad_grid` -> `E_GRID_INVALID`
- `bad_missing_layer` -> `E_LAYER_MISSING`
- `bad_layer_size` -> `E_LAYER_SIZE_MISMATCH`
- `bad_samples_per_pixel` -> `E_TIFF_SAMPLE_COUNT_INVALID`
- `bad_planar_config` -> `E_TIFF_PLANAR_CONFIG_INVALID`

---

## 7. 统计字段状态

`slice_report.json` 已增加按通道统计字段 `channelStats`，覆盖 `R/G/B/W/S/V`：

- `printPixels`
- `fullPrintPixels`
- `partialPrintPixels`
- `emptyPixels`
- `minValue`
- `maxValue`

统计定义：

- `emptyPixels`：值为 `255`
- `printPixels`：值小于 `255`
- `fullPrintPixels`：值为 `0`
- `partialPrintPixels`：值在 `1..254`

兼容状态：

- 原有 `modelPrintPixels`、`supportPrintPixels`、`rgbPrintPixels`、`whitePrintPixels`、`varnishPrintPixels` 保留。
- preview 相关 `nonZeroPixels` / `displayNonZeroPixels` 仍保留给旧脚本兼容；03 后建议优先读取 `printPixels` 或 `channelStats.*.printPixels`。

---

## 8. 回归脚本状态

新增统一回归入口：

```powershell
.\scripts\run_regression.ps1
```

可选跳过重型浮雕样例：

```powershell
.\scripts\run_regression.ps1 -SkipHeavyRelief
```

完整回归覆盖：

- Debug 构建
- ordinary P0：`samples/configs/slice_config.json`
- Support：`support_bottom_projection.json`
- Support：`support_unsupported_only.json`
- Support：`support_bottom_plus_unsupported.json`
- Support：`support_island_filter.json`
- Relief V：`relief_nail_varnish_support.json`
- Relief W：`relief_nail_white_support.json`
- Relief RGB：`relief_rgb_gray.json`
- bad package 负向错误码矩阵

本次验证结果：

```powershell
.\scripts\run_regression.ps1
```

结果：通过。脚本完成构建、正向切片、RIP reader 读取、bad package 生成与负向错误码校验，最终输出 `Regression complete.`。

---

## 9. 当前未实现范围

以下内容不是 03 阶段目标，当前仍未实现：

- 彩色纹理切片
- OBJ/MTL 真实纹理采样
- OpenVDB / volumetric slicing
- Qt UI
- 复杂支撑树 / 树状支撑
- RIP 半色调
- ICC / 色彩管理
- 多材料自动分区
- 多模型排版与 nesting
- 面向生产设备的加密、压缩、传输协议

---

## 10. 下一阶段建议

建议后续阶段以 03 固化协议为边界继续推进：

1. 将 `ValidationErrorCode` 作为后续 reader / CLI / UI 的稳定错误接口。
2. 补充更细粒度的 TIFF tag 负向样例，例如缺失 tag、错误 tile 尺寸、损坏 tile offset。
3. 将 `channelStats` 接入更高层验收报告，用于自动判断模型、支撑、光油是否稳定存在。
4. 若进入真实 RIP 对接阶段，先基于 `p0.rgbwsv.1` 制定设备侧读取协议，不要再隐式依赖 preview 图。
5. 若进入能力增强阶段，应另开 PRD，不在 03 协议固化范围内混入彩色纹理、半色调或复杂支撑算法。
