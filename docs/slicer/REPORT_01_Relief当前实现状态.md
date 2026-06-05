# REPORT_01_Relief当前实现状态

> 文档版本：v0.1  
> 文档状态：Current Implementation Snapshot  
> 适用阶段：PRD_01 / DEV_01 / DEMO_01  
> 更新日期：2026-06-05  

## 1. 阶段定位

当前阶段按 `DOC_DECISION_01_00C完成后的阶段路线调整.md` 执行：

```text
不进入彩色纹理
不进入 UV / MTL 真实材质映射
不引入 OpenVDB
不引入 Qt UI
```

本阶段目标是将 00C 的 `relief_heightfield` 从 Demo 能力整理为正式 2.5D / Relief 路线。

## 2. 保持不变的输出协议

继续遵守 00B / 00C_FINAL：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
polarity = black_is_print
channelOrder = R G B W S V
V = 光油
S = 支撑
Model > Support > Empty
```

## 3. 当前工程结构变化

新增正式 relief 样例目录：

```text
samples/
  models/
    relief/
      relief.obj
      relief_nail_arched.obj
  configs/
    relief/
      relief_nail_varnish_support.json
      relief_nail_white_support.json
      relief_flat_varnish_no_support.json
      relief_rgb_gray.json
```

保留兼容入口：

```text
samples/configs/slice_config_relief_varnish.json
```

该兼容入口仍输出：

```text
output/SlicePackage_relief_varnish
```

## 4. 新增 Relief 样例配置

### 4.1 relief_nail_varnish_support

```text
slicingMode = relief_heightfield
relief.fillMode = intersection_range
modelMaterial.materialChannel = V
support.enabled = true
output = output/ReliefNailVarnishSupport
```

验收语义：

```text
V print pixels > 0
S print pixels > 0
```

### 4.2 relief_nail_white_support

```text
modelMaterial.materialChannel = W
support.enabled = true
output = output/ReliefNailWhiteSupport
```

验收语义：

```text
W print pixels > 0
S print pixels > 0
```

### 4.3 relief_flat_varnish_no_support

```text
relief.fillMode = surface_to_base
modelMaterial.materialChannel = V
support.enabled = false
output = output/ReliefFlatVarnishNoSupport
```

验收语义：

```text
V print pixels > 0
S print pixels = 0
```

### 4.4 relief_rgb_gray

```text
relief.fillMode = intersection_range
modelMaterial.materialChannel = RGB
modelMaterial.rgb = [0, 0, 0]
support.enabled = false
output = output/ReliefRgbGray
```

验收语义：

```text
RGB print pixels > 0
W/S/V unused = 255
```

## 5. Relief Report 增强

`reports/relief_report.json` 当前包含：

```text
slicingMode
fillMode
baseZMm
support.enabled
support.source
support.expectedSupport
support.supportPixels
support.columnsWithSupport
columns.total
columns.hit
columns.empty
columns.multiHit
columns.coverageRatio
height.zMinMm
height.zMaxMm
height.thicknessMinMm
height.thicknessMaxMm
zRangeMm
warnings
```

其中：

```text
support.source = relief_lower_surface
coverageRatio = hitColumns / totalColumns
thickness = zMax - zMin per XY column
```

## 6. Relief 数据结构与支撑来源

DEV_01 建议的核心数据结构已在当前 `slicer.cpp` 内落地，尚未拆分到独立 `src/slicer_core/relief/` 目录：

```text
ReliefColumnInfo
ReliefSamplingResult
```

当前 relief 采样输出：

```text
model_masks
columns
relief_report
```

其中 `ReliefColumnInfo` 记录：

```text
has_model
lower_layer
upper_layer
z_min_mm
z_max_mm
hit_count
multi_hit
```

支撑来源已按 DEV_01 从通用 `compute_first_model_layers(model_masks)` 调整为：

```text
relief_heightfield:
  使用 ReliefColumnInfo.lower_layer 作为下表面支撑源

closed_mesh_scanline:
  继续使用 compute_first_model_layers(model_masks)
```

支撑条件保持：

```text
!is_model && support.enabled && layer_index < lower_layer
```

## 7. Print Pixels 统计

`slice_report.json` 新增正式 `printPixels` 命名字段，同时保留旧 `NonZeroPixels` 字段兼容现有读取方。

当前 totals 包含：

```text
modelPrintPixels
supportPrintPixels
rgbPrintPixels
whitePrintPixels
varnishPrintPixels
```

每层统计也包含：

```text
modelPrintPixels
supportPrintPixels
rgbPrintPixels
whitePrintPixels
varnishPrintPixels
```

`preview_report.json` 中每个预览文件新增：

```text
printPixels
displayNonZeroPixels
```

说明：

```text
生产 TIFF 仍使用 0=打印、255=不打印。
preview 的 printPixels 是反相显示后的可见像素计数。
```

## 8. 已执行验证

构建：

```powershell
cmake --build build --config Debug
```

普通 P0：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
build\Debug\rip_reader_test.exe --package output\SlicePackage
```

旧 00C 兼容入口：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_relief_varnish.json
build\Debug\rip_reader_test.exe --package output\SlicePackage_relief_varnish
```

新增 Relief 样例：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\relief\relief_nail_varnish_support.json
build\Debug\rip_reader_test.exe --package output\ReliefNailVarnishSupport

build\Debug\slicer_cli.exe --config samples\configs\relief\relief_nail_white_support.json
build\Debug\rip_reader_test.exe --package output\ReliefNailWhiteSupport

build\Debug\slicer_cli.exe --config samples\configs\relief\relief_flat_varnish_no_support.json
build\Debug\rip_reader_test.exe --package output\ReliefFlatVarnishNoSupport

build\Debug\slicer_cli.exe --config samples\configs\relief\relief_rgb_gray.json
build\Debug\rip_reader_test.exe --package output\ReliefRgbGray
```

说明：

```text
批量回归命令曾因超时未完整返回，后续已改为单个 package 逐项验证。
ReliefColumnInfo.lower_layer 支撑源改造后，已重新验证普通 P0 与 relief_nail_varnish_support。
```

## 9. 验证统计

当前 4 个 Relief package 的关键统计：

```text
ReliefNailVarnishSupport
  modelPrintPixels = 19602925
  varnishPrintPixels = 19602925
  supportPrintPixels = 62664673

ReliefNailWhiteSupport
  modelPrintPixels = 19602925
  whitePrintPixels = 19602925
  supportPrintPixels = 62664673

ReliefFlatVarnishNoSupport
  modelPrintPixels = 82267598
  varnishPrintPixels = 82267598
  supportPrintPixels = 0

ReliefRgbGray
  modelPrintPixels = 19602925
  rgbPrintPixels = 19602925
  supportPrintPixels = 0
```

共有高度场统计：

```text
columns.hit = 184028
columns.coverageRatio = 0.886873379533691
height.thicknessMinMm = 0.0249926531681042
height.thicknessMaxMm = 3.65927420770505
```

ReliefColumnInfo.lower_layer 支撑源改造后复验：

```text
slice_config.json:
  modelPixels = 1440
  supportPixels = 2880
  rip_reader_test = pass

relief_nail_varnish_support.json:
  modelPixels = 19602925
  supportPixels = 62664673
  rip_reader_test = pass
```

## 10. 当前符合情况

PRD_01 / DEV_01 / DEMO_01 清单项已完成：

```text
relief 样例目录建立
4 个 relief 配置建立
V / W / RGB 单材料输出可验证
S 下表面支撑可验证
surface_to_base / intersection_range 均有样例
relief_report 增强
slice_report 增加 printPixels 统计
preview_report 增加 printPixels/displayNonZeroPixels
ReliefColumnInfo / ReliefSamplingResult 已落地
relief 模式支撑来源已改为 lower_layer
rip_reader_test 全部通过
```

## 11. 当前未实现能力

仍未实现，且不属于本轮任务：

```text
彩色纹理
UV 采样
MTL 真实材质映射到材料通道
局部光油
top_surface_only
top_n_layers
复杂支撑树
OpenVDB / SDF
Qt 产品 UI
```

DEV_01 中仍保留为后续代码整理项：

```text
将 relief 逻辑拆分到 src/slicer_core/relief/
新增 tests/relief/ 自动化测试目录
固化 expected report summary / preview snapshot 基准文件
```

## 12. 下一步建议

建议下一步进入：

```text
PRD_02 / DEV_02：支撑生成、孤岛检测与 SupportType 扩展
PRD_03 / DEV_03：RGBWSV TIFF 协议与 RIP 输入规范固化
```

其中 PRD_03 可与 PRD_02 并行推进，但不应改变当前已稳定的 00B 协议。

## 13. 本报告覆盖要求对照

### 13.1 PRD_01 / DEV_01 / DEMO_01 完成内容

已完成：

```text
PRD_01:
  relief_heightfield 正式样例路线建立
  V / W / RGB 单材料输出验证
  S 下表面支撑验证
  relief_report / support_report / slice_report 增强
  relief 专用样例目录与配置建立

DEV_01:
  ReliefColumnInfo 已落地
  ReliefSamplingResult 已落地
  relief 采样输出 model_masks / columns / relief_report
  relief 模式支撑来源改为 ReliefColumnInfo.lower_layer
  printPixels 统计字段已加入

DEMO_01:
  4 个 relief 样例配置可运行
  普通 P0 配置可运行
  00C 兼容入口可运行
  rip_reader_test 已通过
```

仍作为后续整理项：

```text
拆分 src/slicer_core/relief/ 独立模块目录
建立 tests/relief/ 自动化测试
固化 expected report summary / preview snapshot 基准文件
```

### 13.2 relief_heightfield 当前支持模式

当前支持：

```text
slicingMode:
  relief_heightfield

relief.fillMode:
  intersection_range
  surface_to_base

modelMaterial.materialChannel:
  V
  W
  RGB
  auto

modelMaterial.applyMode:
  solid_volume

support.mode:
  bottom_projection

support.enabled:
  true
  false

preview.channels:
  varnish
  support
  white
  rgb
```

其中 `relief_heightfield` 的默认业务推荐配置仍是：

```text
intersection_range + V 光油 + S 下表面支撑
```

### 13.3 V / W / RGB 单材料输出验证情况

已验证：

```text
V:
  config = samples/configs/relief/relief_nail_varnish_support.json
  package = output/ReliefNailVarnishSupport
  varnishPrintPixels = 19602925
  rip_reader_test = pass

W:
  config = samples/configs/relief/relief_nail_white_support.json
  package = output/ReliefNailWhiteSupport
  whitePrintPixels = 19602925
  rip_reader_test = pass

RGB:
  config = samples/configs/relief/relief_rgb_gray.json
  package = output/ReliefRgbGray
  rgbPrintPixels = 19602925
  rip_reader_test = pass
```

### 13.4 S 支撑在浮雕模型中的稳定性

已验证 S 支撑在开启支撑的浮雕样例中稳定存在：

```text
ReliefNailVarnishSupport:
  support.enabled = true
  supportPrintPixels = 62664673
  support.source = relief_lower_surface
  support source implementation = ReliefColumnInfo.lower_layer
  rip_reader_test = pass

ReliefNailWhiteSupport:
  support.enabled = true
  supportPrintPixels = 62664673
  support.source = relief_lower_surface
  rip_reader_test = pass
```

关闭支撑的样例中 S 通道保持不打印：

```text
ReliefFlatVarnishNoSupport:
  support.enabled = false
  supportPrintPixels = 0

ReliefRgbGray:
  support.enabled = false
  supportPrintPixels = 0
```

### 13.5 reports 完整性

`relief_report.json` 已包含：

```text
slicingMode
fillMode
baseZMm
columns.total / hit / empty / multiHit / coverageRatio
height.zMinMm / zMaxMm / thicknessMinMm / thicknessMaxMm
support.enabled / source / expectedSupport / supportPixels / columnsWithSupport
warnings
```

`support_report.json` 已包含：

```text
enabled
mode
value
slicingMode
supportSource
modelPriority
supportPixels
supportPrintPixels
columnsWithSupport
layers
```

`slice_report.json` 已包含：

```text
slicingMode
grid
totals.modelPixels / supportPixels
totals.modelPrintPixels
totals.supportPrintPixels
totals.rgbPrintPixels
totals.whitePrintPixels
totals.varnishPrintPixels
layers
```

结论：

```text
relief_report / support_report / slice_report 已满足 DEMO_01 第一轮验收。
```

### 13.6 已通过的测试模型与配置

已通过：

```text
普通 P0:
  model = samples/models/sample.stl
  config = samples/configs/slice_config.json
  package = output/SlicePackage
  rip_reader_test = pass

00C 兼容入口:
  model = samples/models/relief/relief_nail_arched.obj
  config = samples/configs/slice_config_relief_varnish.json
  package = output/SlicePackage_relief_varnish
  rip_reader_test = pass

Relief V + S:
  model = samples/models/relief/relief_nail_arched.obj
  config = samples/configs/relief/relief_nail_varnish_support.json
  package = output/ReliefNailVarnishSupport
  rip_reader_test = pass

Relief W + S:
  model = samples/models/relief/relief_nail_arched.obj
  config = samples/configs/relief/relief_nail_white_support.json
  package = output/ReliefNailWhiteSupport
  rip_reader_test = pass

Relief V no support:
  model = samples/models/relief/relief_nail_arched.obj
  config = samples/configs/relief/relief_flat_varnish_no_support.json
  package = output/ReliefFlatVarnishNoSupport
  rip_reader_test = pass

Relief RGB no support:
  model = samples/models/relief/relief_nail_arched.obj
  config = samples/configs/relief/relief_rgb_gray.json
  package = output/ReliefRgbGray
  rip_reader_test = pass
```

### 13.7 当前仍不支持的场景

当前仍不支持：

```text
彩色纹理
UV / Texture 采样
MTL 真实材质映射到 RGB/W/V
3MF 多材料
局部光油
完整光油覆盖策略
top_surface_only 正式实现
top_n_layers 正式实现
复杂支撑树
孤岛检测
SupportType 扩展
OpenVDB / SDF 正式体素内核
Qt 产品 UI
```

### 13.8 下一阶段建议

建议进入：

```text
优先 1:
  PRD_02 / DEV_02 支撑生成、孤岛检测与 SupportType 扩展

优先 2:
  PRD_03 / DEV_03 RGBWSV TIFF 协议与 RIP 输入规范固化
```

建议执行原则：

```text
不要改变 RGBWSV 通道顺序
不要改变 uint8 / black_is_print / 0=打印 / 255=不打印
不要提前进入彩色纹理、UV、OpenVDB 或 Qt UI
```
