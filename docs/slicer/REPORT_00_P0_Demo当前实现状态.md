# REPORT_00_P0_Demo当前实现状态

> 文档版本：v0.4  
> 文档状态：Current Implementation Snapshot  
> 适用阶段：P0 Demo / 00B / 00A / 00C  
> 更新日期：2026-06-05  

## 1. 当前工程结构

当前实现主线位于：

```text
slice_soft_demo/
  apps/
    slicer_cli/
    rip_reader_test/
  src/
    slicer_core/
      config.*
      json_value.*
      model.*
      rip_reader.*
      slicer.*
      tiff_io.*
  samples/
    configs/
      slice_config.json
      slice_config_model_0_3.json
      slice_config_relief_varnish.json
    models/
      0.3.obj
      0.3.mtl
      sample.stl
  docs/
    slicer/
```

## 2. 当前 CMake Target

```text
slicer_core
slicer_cli
rip_reader_test
```

`slicer_core` 负责配置、模型导入、采样、RGBWSV 合成、TIFF、reports、manifest、RIP reader。  
`slicer_cli` 负责命令行切片、模型检查、preview-only。  
`rip_reader_test` 负责正向 package 校验和 `--expect-error` 负向验收。

## 3. 当前已实现功能

### 3.1 00B 输出协议

已实现并保持：

```text
uint8
0 = 打印
255 = 不打印
polarity = black_is_print
channelOrder = R G B W S V
```

Manifest 写入：

```text
bitDepth = 8
printValue = 0
emptyValue = 255
polarity = black_is_print
```

TIFF tile padding 已按 00B 修正为默认 255，避免 tile 外补齐区域被误读为打印。

### 3.2 00A 稳定化增强

已实现：

- PNG / PPM preview
- `preview.format`
- `preview.channels`
- `preview.layerRange`
- `preview.onlyNonEmptyLayers`
- Binary STL 读取
- OBJ `mtllib` / `usemtl` 基础统计
- `slice_report.json` 每层统计
- `contour_report.json`
- `rip_reader_test --expect-error --expect-message`

### 3.3 00C 单材料浮雕模式

已实现新增模式：

```json
{
  "slicingMode": "relief_heightfield"
}
```

原默认模式保持：

```text
closed_mesh_scanline
```

新增配置字段：

```text
modelMaterial.materialChannel = auto / RGB / W / V
modelMaterial.applyMode = solid_volume
relief.fillMode = surface_to_base / intersection_range
relief.baseZMm
```

`relief_heightfield` 第一版采样算法：

```text
对三角面按 XY 覆盖像素进行列采样
每个 XY column 记录 z_min / z_max
surface_to_base: 填充 baseZ..z_max
intersection_range: 填充 z_min..z_max
```

单材料光油模式：

```text
R/G/B/W/S = 255
V = 0
```

00C 默认不启用 S 支撑通道，避免把浮雕基底误写为支撑。

输出新增：

```text
reports/relief_report.json
manifest.slicing.mode
manifest.slicing.reliefFillMode
```

## 4. 当前未实现功能

00C 不包含且当前未实现：

- 彩色纹理
- UV / Texture 采样
- MTL 材质映射到材料通道
- 局部光油 / 上表面光油策略
- top surface only / top N layers
- 正式高度图输入
- OpenVDB / SDF 正式内核
- 复杂支撑
- Qt 产品 UI

00C `relief_heightfield` 是 P0+ 简化实现，不等同于正式 2.5D 浮雕产品路线。

## 5. 当前配置文件说明

### samples/configs/slice_config.json

普通 P0 sample，默认 `closed_mesh_scanline`。

### samples/configs/slice_config_model_0_3.json

0.3 OBJ 的普通 closed mesh 路径，用于对比旧 scanline/bottom projection 行为。

### samples/configs/slice_config_relief_varnish.json

00C 新增样例，使用：

```text
slicingMode = relief_heightfield
materialChannel = V
applyMode = solid_volume
support.enabled = false
relief.fillMode = surface_to_base
```

输出：

```text
output/SlicePackage_relief_varnish
```

## 6. 当前运行方式

构建：

```powershell
cmake --build build --config Debug
```

普通模式：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
build\Debug\rip_reader_test.exe --package output\SlicePackage
```

浮雕光油模式：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_relief_varnish.json
build\Debug\rip_reader_test.exe --package output\SlicePackage_relief_varnish
```

真实模型 preview-only：

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --preview-only
```

## 7. 当前符合情况

### PRD_00 / DEV_00 / DEMO_00

基础 P0 闭环仍可运行：

```text
model input
config
slice
RGBWSV TIFF
manifest/reports
rip_reader_test
```

### 00B

符合：

```text
uint8
0 = 打印
255 = 不打印
black_is_print
R G B W S V
```

### 00A

已完成第一轮稳定化增强：PNG preview、导入统计、几何诊断、reports 增强、RIP 负向验收入口。

### 00C

已完成：

- `slicingMode = relief_heightfield`
- `materialChannel = V / W / RGB / auto`
- `applyMode = solid_volume`
- `relief.fillMode`
- `relief.baseZMm`
- relief column sampler
- `relief_report.json`
- manifest `slicing`
- relief varnish 样例配置
- 普通模式不破坏
- `rip_reader_test` 通过

## 8. 已执行验证

已运行：

```powershell
cmake --build build --config Debug
build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
build\Debug\rip_reader_test.exe --package output\SlicePackage
build\Debug\slicer_cli.exe --config samples\configs\slice_config_relief_varnish.json
build\Debug\rip_reader_test.exe --package output\SlicePackage_relief_varnish
```

00C relief 第 50 层和第 400 层有效像素统计已确认：

```text
R/G/B/W/S = 255
V contains print pixels with value 0
S print pixels = 0
```

## 9. 下一步建议 Milestone

### Milestone 01：正式 2.5D Relief 路线

- 设计正式高度图输入
- 支持局部高度与材料策略
- 明确 top surface / solid volume / top N layers 的产品语义
- 增加 relief 专用测试模型集

### Milestone 02：彩色纹理模型切片

- UV / texture 采样
- MTL 材质映射
- RGB 与 W/V 联合策略

### Milestone 03：协议固化

- 固化 RGBWSV TIFF tag 要求
- 固化 manifest schema
- 增加自动化回归脚本
