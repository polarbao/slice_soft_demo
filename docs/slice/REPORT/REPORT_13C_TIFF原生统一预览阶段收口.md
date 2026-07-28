# REPORT 13C TIFF 原生统一预览阶段收口

> 状态：COMPLETE / M13-4 PASS
> 日期：2026-07-28
> 覆盖任务：13C-01..05
> 下一任务：13D-01 顶部作业栏

## 1. 阶段成果

13C 已把生产预览从逐通道 PNG/PPM 迁移到生产 RGBWSV TIFF 真源：

```text
manifest/layers 是唯一生产层索引；
TiffLayerSource 支持 stripped/tiled、真实 layerIndex、zMm 和独立 dpiX/dpiY；
TiffLayerCache 默认 5 层 / 256 MiB LRU，支持取消、stale 和 package 切换；
MaterialPreviewComposer 支持 RGB、R/G/B、W/S/V、RGB+W/S/V、
RGB+S+W+V、Occupancy、Empty；
六通道像素探针显示原始 R/G/B/W/S/V 生产值；
PreviewWorkspace 收敛为“生产预览/诊断预览”两个一级入口；
默认 outputPolicy=tiff_native，不再自动写重复诊断图；
显式 tiff_native_with_diagnostics 继续兼容 RGB/W/S/V PNG/PPM；
生产 TIFF、RIP 和固定协议不受诊断图策略影响。
```

## 2. 固定协议

本阶段未修改生产协议：

| 项目 | 固定值 |
|---|---|
| schema | `p0.rgbwsv.2` |
| 通道顺序 | `R G B W S V` |
| 位深 | uint8 |
| 极性 | `black_is_print` |
| 打印值 | 0 |
| 空白值 | 255 |

所有显示伪彩只存在于 UI buffer，不回写 TIFF。

## 3. 数据源与生命周期

```text
PackageLoader -> manifest.layers -> TiffLayerSource -> TiffLayerCache
-> MaterialPreviewComposer -> Qt Image
```

生命周期已覆盖：

```text
快速滑层只接受最新 generation；
取消和 stale 结果不进入缓存；
manifest 修改使旧 LayerRef 失效；
切换 package 清理旧 package 缓存；
模式切换复用同层 TIFF buffer，不重复读取文件；
诊断图缺失时不跨层兜底。
```

## 4. 材料与显示矩阵

| 能力 | 证据 | 结果 |
|---|---|---|
| R/G/B 与 RGB 真彩 | `material_preview_composer_unit_tests` | PASS |
| W/S/V 单通道伪彩 | 同上 | PASS |
| RGB+W/S/V | 同上 | PASS |
| RGB+S+W+V | 同上及 UI Smoke 13 模式 | PASS |
| Occupancy / Empty | 同上 | PASS |
| 部分覆盖值 | black-is-print 确定性混合测试 | PASS |
| 六通道探针 | RGB/W/S/V 同像素夹具 | PASS |
| 635/600 非等方 DPI | `non_square_raster_pipeline_unit_tests`、`preview-physical-aspect` | PASS |

## 5. 存储与 Package 矩阵

| 场景 | 生成方式 | RIP | 结果 |
|---|---|---|---|
| stripped | 共享 `RgbwsvPackageWriter` | strict PASS | PASS |
| tiled | 共享 `RgbwsvPackageWriter` | strict PASS | PASS |
| 无 preview 目录 | 共享多模型 writer，`13B-M01/package` | strict PASS | PASS |
| 显式诊断图 | 共享 writer 单测与 13C-04 IO 双策略 | strict PASS | PASS |
| 635/600 | 共享 per-layer TIFF writer / resolution fixture | strict PASS | PASS |

无 preview 目录的共享 writer 实包：

```text
output/benchmarks/13c_05/scene_matrix/13B-M01/package
```

实测包含 28 层，UI `tiff-native-preview-no-png` 成功浏览首/中/末层和 13 种显示模式，
RIP 摘要为 `schema=p0.rgbwsv.2`、uint8、`R G B W S V`、0 warnings。

## 6. 错误矩阵

`tiff_layer_source_unit_tests` 已验证稳定 fail-closed：

| 错误 | 结果码 |
|---|---|
| schema 不匹配 | `ProtocolMismatch` |
| bitDepth 非 8 | `ProtocolMismatch` |
| polarity 非 black_is_print | `ProtocolMismatch` |
| layer path 越界 | `PathEscape` |
| TIFF 文件缺失 | `FileMissing` |
| TIFF/manifest 尺寸不一致 | `DimensionMismatch` |
| 用户取消 | `Cancelled` |
| generation 过期 | `StaleResult` |
| manifest 被修改 | `StaleResult` |

`MaterialPreviewComposer` 另验证错误 byte count、越界探针和未知模式均稳定失败，不显示伪造图像。

## 7. Preview IO

13C-04 同 fixture 双策略实测：

```text
tiff_native：25 TIFF / 178750 bytes，0 诊断图，previewWriteMs=0；
with_diagnostics：25 TIFF / 178750 bytes，100 诊断图 / 354800 bytes，
previewWriteMs=364.400；
两组 TIFF 逐层 SHA-256 完全一致。
```

这是本机 Debug 单次功能基线，不是跨设备性能承诺。可复核运行：

```powershell
.\scripts\run_13c_04_preview_io.ps1 -BuildDir build -Config Debug
```

## 8. 实际验证

2026-07-28 在最终 13C-04 代码上实际执行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe `
  --ui-smoke-test --case tiff-native-preview-no-png `
  --package output\benchmarks\13c_05\scene_matrix\13B-M01\package
.\build\Debug\rip_reader_test.exe `
  --package output\benchmarks\13c_05\scene_matrix\13B-M01\package --summary
.\scripts\run_13c_04_preview_io.ps1 -BuildDir build -Config Debug
.\scripts\run_ci_quick.ps1
git diff --check
```

结果：

```text
Debug 全量构建：PASS；
CTest：82/82 PASS；
Qt self-test：PASS；
无 preview 目录 UI Smoke：PASS，28 层、13 模式、TIFF 真源；
共享 writer 实包 RIP strict：PASS，0 warnings；
Preview IO 双策略：PASS；
Quick CI：PASS；
git diff --check：PASS，仅有换行转换提示。
```

## 9. Gate 结论

`13C-01..05 COMPLETE / M13-4 PASS`。

已解除：

```text
13D-01 顶部作业栏的 13C-05 顺序 Gate；
12E-09A-05 对 TIFF 原生底图的依赖。
```

仍未解除：

```text
13B 正式设备 buildVolume、轴方向和 22 实例性能预算；
12E-09A-03..06 自身顺序任务；
12E-10A 对 09A-05 的依赖；
中期 3D viewport 和自动 nesting。
```

13C 收口不代表设备 production GO，也不改变 Legacy/Global 的生产准入状态。
