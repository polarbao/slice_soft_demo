# DEMO_03B_v0.2_TIFF存储模式兼容验证方案

> 文档版本：v0.2  
> 文档状态：Draft / DEMO  
> 适用阶段：REPORT_05 之后 / 03B  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证：

```text
1. 默认输出 stripped TIFF
2. 可配置输出 tiled TIFF
3. RIP Reader 两者都可读取
4. legacy p0.rgbwsv.1 tiled package 仍可读取
5. 05 MaterialPolicy 六个样例不被破坏
```

---

## 2. 样例配置

建议新增：

```text
samples/configs/storage_mode/
  storage_stripped_default.json
  storage_tiled_compat.json
  storage_material_policy_rgbwv_stripped.json
  storage_material_policy_rgbwv_tiled.json
```

---

## 3. Stripped 默认样例

配置：

```json
{
  "output": {
    "storageMode": "stripped",
    "rowsPerStrip": 64
  }
}
```

验收：

```text
manifest.schema = p0.rgbwsv.2
manifest.tiff.storageMode = stripped
manifest.tiff.tiled = false
manifest.tiff.rowsPerStrip = 64
rip_reader_test pass
```

---

## 4. Tiled 兼容样例

配置：

```json
{
  "output": {
    "storageMode": "tiled",
    "tileSize": [256, 256]
  }
}
```

验收：

```text
manifest.schema = p0.rgbwsv.2
manifest.tiff.storageMode = tiled
manifest.tiff.tiled = true
rip_reader_test pass
```

---

## 5. MaterialPolicy 样例回归

默认 stripped 模式下必须验证 6 个 MaterialPolicy 样例：

```text
textured_rgb_only.json
textured_rgb_white_underbase.json
textured_rgb_varnish_top2.json
textured_rgb_white_varnish.json
varnish_only_all_model.json
white_only_all_model.json
```

额外 tiled compatibility 至少验证：

```text
textured_rgb_white_varnish.json
```

---

## 6. Legacy Tiled 兼容

使用历史 `p0.rgbwsv.1` package 或生成 legacy 样例。

验收：

```text
reader accepts p0.rgbwsv.1
reader treats it as legacy tiled
rip_reader_test pass
```

---

## 7. Bad Package

新增：

```text
bad_storage_mode
bad_rows_per_strip
bad_tiff_storage_mismatch
bad_tile_size
```

验收：

```text
rip_reader_test --expect-error
```

---

## 8. 回归 Checklist

- [ ] 默认 stripped package 通过。
- [ ] tiled compatibility package 通过。
- [ ] legacy p0.rgbwsv.1 tiled package 通过。
- [ ] P0 stripped 通过。
- [ ] Relief stripped 通过。
- [ ] Support stripped 通过。
- [ ] Texture stripped 通过。
- [ ] MaterialPolicy 六个 stripped 样例通过。
- [ ] MaterialPolicy tiled compatibility 样例通过。
- [ ] Bad storage packages 按预期失败。
- [ ] RGB/W/V/S printPixels 语义与 REPORT_05 基线一致。
- [ ] RGBWSV 语义不变。

---

## 9. 验证命令示例

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\storage_mode\storage_stripped_default.json
build\Debug\rip_reader_test.exe --package output\StorageStrippedDefault

build\Debug\slicer_cli.exe --config samples\configs\storage_mode\storage_tiled_compat.json
build\Debug\rip_reader_test.exe --package output\StorageTiledCompat

.\scripts\run_regression.ps1
```

---

## 10. 状态报告

完成后生成：

```text
docs/slicer/REPORT_03B_TIFF存储模式兼容当前实现状态.md
```

报告必须说明：

```text
默认 storageMode
schema 迁移策略
p0.rgbwsv.1 兼容结果
p0.rgbwsv.2 stripped/tiled 结果
MaterialPolicy 六个样例回归结果
bad storage package 结果
```
