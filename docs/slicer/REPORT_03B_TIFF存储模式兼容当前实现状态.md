# REPORT_03B_TIFF存储模式兼容当前实现状态

> 日期：2026-06-08  
> 阶段：03B / REPORT_05 后 TIFF 存储模式兼容改造  
> 状态：已完成实现与验证

---

## 1. 本阶段目标

03B 只处理 RGBWSV TIFF 的物理存储模式兼容，不改变切片几何、支撑生成、纹理采样、MaterialPolicy 或 RGB/W/S/V 通道语义。

本阶段完成：

- 默认输出从 tiled TIFF 改为 stripped TIFF。
- manifest schema 从 `p0.rgbwsv.1` 升级到 `p0.rgbwsv.2`。
- 保留 tiled TIFF 兼容输出和 reader 兼容读取。
- RIP Reader 支持 `p0.rgbwsv.1` legacy tiled、`p0.rgbwsv.2` stripped、`p0.rgbwsv.2` tiled。
- 增加 TIFF storage 相关负向错误码与 bad package 测试。
- MaterialPolicy 六个样例保持回归通过。

---

## 2. 配置状态

`output` 新增字段：

- `storageMode`: `stripped` 或 `tiled`，默认 `stripped`。
- `rowsPerStrip`: stripped TIFF 使用，默认 `64`。

保留字段：

- `tileSize`: tiled TIFF 使用。
- `tiled`: 旧配置兼容字段；没有 `storageMode` 时，`tiled=true` 解析为 `storageMode=tiled`，`tiled=false` 解析为 `storageMode=stripped`。

当前样例配置已更新为默认 stripped；tiled 兼容样例单独放在：

- `samples/configs/storage_mode/storage_tiled_compat.json`
- `samples/configs/storage_mode/storage_material_policy_rgbwv_tiled.json`

---

## 3. TIFF Writer 状态

当前 writer 支持两种物理存储：

- `stripped`: 写入 `StripOffsets`、`StripByteCounts`、`RowsPerStrip`，不写 tile tag。
- `tiled`: 保留原 tiled writer，继续写入 `TileWidth`、`TileLength`、`TileOffsets`、`TileByteCounts`，tile padding 仍为 `255`。

上层通过 `write_rgbwsv_tiff(...)` 按 `TiffImageSpec.storage_mode` 分流。

生产协议仍保持：

- `uint8`
- `R G B W S V`
- `0=打印`
- `255=不打印`
- `black_is_print`

---

## 4. Manifest 状态

新生成包：

- `schema = p0.rgbwsv.2`
- `schemaVersion = p0.rgbwsv.2`

stripped 包写入：

- `tiff.storageMode = stripped`
- `tiff.storage = stripped`
- `tiff.tiled = false`
- `tiff.rowsPerStrip = 64`

tiled 包写入：

- `tiff.storageMode = tiled`
- `tiff.storage = tiled`
- `tiff.tiled = true`
- `tiff.tileSize = [256, 256]`

---

## 5. RIP Reader 状态

Reader 当前支持：

- `p0.rgbwsv.1` legacy tiled package。
- `p0.rgbwsv.2` stripped package。
- `p0.rgbwsv.2` tiled package。

Reader 会检查：

- schema 是否为 `p0.rgbwsv.1` 或 `p0.rgbwsv.2`。
- `storageMode/storage/tiled` 是否一致。
- stripped 必须有合法 `rowsPerStrip > 0`。
- tiled 必须有合法 `tileSize`。
- 实际 TIFF tag 结构必须与 manifest 声明一致。

新增错误码：

- `E_TIFF_STORAGE_MODE_INVALID`
- `E_TIFF_STORAGE_MISMATCH`
- `E_ROWS_PER_STRIP_INVALID`
- `E_TILE_SIZE_INVALID`

---

## 6. 样例与回归状态

新增样例：

- `samples/configs/storage_mode/storage_stripped_default.json`
- `samples/configs/storage_mode/storage_tiled_compat.json`
- `samples/configs/storage_mode/storage_material_policy_rgbwv_stripped.json`
- `samples/configs/storage_mode/storage_material_policy_rgbwv_tiled.json`

MaterialPolicy 六个样例继续作为回归基线：

- `textured_rgb_only.json`
- `textured_rgb_white_underbase.json`
- `textured_rgb_varnish_top2.json`
- `textured_rgb_white_varnish.json`
- `varnish_only_all_model.json`
- `white_only_all_model.json`

---

## 7. 验证结果

已运行并通过：

- `cmake --build build --config Debug`
- `slicer_cli + rip_reader_test` for `storage_stripped_default.json`
- `slicer_cli + rip_reader_test` for `storage_tiled_compat.json`
- `.\scripts\run_regression.ps1`
- `.\scripts\run_regression.ps1 -SkipHeavyRelief`
- legacy v1 tiled 临时包 reader 校验
- 重型 relief 三个样例单独运行：
  - `relief_nail_varnish_support.json`
  - `relief_nail_white_support.json`
  - `relief_rgb_gray.json`

说明：

- 曾以 10 分钟工具超时运行 `.\scripts\run_regression.ps1`，该次被终止；随后放宽超时后完整脚本通过，用时约 15 分钟。

---

## 8. Bad Package 验证

当前 bad package 覆盖：

- `bad_missing_manifest`
- `bad_manifest_parse`
- `bad_schema`
- `bad_bit_depth`
- `bad_channel_order`
- `bad_channel_count`
- `bad_polarity`
- `bad_print_value`
- `bad_empty_value`
- `bad_grid`
- `bad_missing_layer`
- `bad_layer_size`
- `bad_samples_per_pixel`
- `bad_planar_config`
- `bad_storage_mode`
- `bad_rows_per_strip`
- `bad_tiff_storage_mismatch`
- `bad_tile_size`

新增 storage bad package 均返回预期错误码。

---

## 9. 当前限制

- 当前 TIFF writer/reader 仍是 P0 RGBWSV 专用手写实现，不支持压缩、BigTIFF、多 IFD、planar separate 或其他 sample format。
- Reader 当前根据 tag 结构区分 stripped/tiled，不提供通用 TIFF 浏览器能力。
- `run_regression.ps1` 完整模式包含重型 relief 样例，总耗时可能超过 10 分钟；长时间验证建议分段运行或提高超时。

---

## 10. 下一步建议

建议 03B 后进入以下收口：

1. 为 `rip_reader_test` 增加更短的 summary 输出模式，避免大层数模型输出过长。
2. 将 heavy relief 回归拆成独立脚本，避免主回归总时长过长。
3. 若后续进入 RIP 对接，应确认目标 RIP 对 stripped/tiled 的首选存储方式、RowsPerStrip 推荐值和 TIFF tag 兼容矩阵。
