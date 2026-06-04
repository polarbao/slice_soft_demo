# REPORT_00_P0_Demo当前实现状态

> 文档版本：v0.3  
> 文档状态：Current Implementation Snapshot  
> 适用阶段：P0 Demo / 00B 输出协议修正后 / 00A 稳定化增强后  
> 生成日期：2026-06-04  

## 1. 当前工程结构

当前仓库用于验证 UV 3D 打印切片软件上游数据闭环。实际工程根目录为 `slice_soft_demo`。

```text
slice_soft_demo/
  AGENTS.md
  CMakeLists.txt
  README.md
  vcpkg.json

  apps/
    slicer_cli/
      main.cpp
    rip_reader_test/
      main.cpp

  src/
    slicer_core/
      config.h / config.cpp
      json_value.h / json_value.cpp
      model.h / model.cpp
      slicer.h / slicer.cpp
      tiff_io.h / tiff_io.cpp
      rip_reader.h / rip_reader.cpp

  samples/
    configs/
      slice_config.json
      slice_config_model_0_3.json
    models/
      sample.stl
      0.3.obj
      0.3.mtl

  docs/
    slicer/
      PRD_00_单材料体素切片与RIP前置数据生成_v0.2.md
      DEV_00_单材料体素切片引擎与RGBWSV多通道TIFF输出设计_v0.2.md
      DEMO_00_单材料体素切片Demo实施方案_v0.1.md
      TASKS_00_P0切片Demo任务清单.md
      ROADMAP_后续PRD_DEV文档生成计划.md
      CODEX_HANDOFF_切片软件开发上下文.md
      REPORT_00_P0_Demo当前实现状态.md
```

模型文件已统一放置到 `samples/models/`。原根目录 `model/` 已整合移除，`slice_config_model_0_3.json` 已更新为：

```json
"modelPath": "samples/models/0.3.obj"
```

## 2. 当前 CMake Target

当前 `CMakeLists.txt` 定义 3 个 target：

```text
slicer_core
slicer_cli
rip_reader_test
```

### slicer_core

类型：静态/默认 CMake library。

职责：

- JSON 配置读取与校验
- STL/OBJ 模型读取
- Binary STL 读取
- OBJ `mtllib` / `usemtl` 基础统计
- 自动摆放与高度约束
- 三角面截面采样
- 几何采样诊断
- 下表面投影支撑
- RGBWSV 六通道 layer compose
- 8-bit black-is-print tiled TIFF 写入与读取
- PNG/PPM preview 写入
- manifest/reports/contour report 写入
- RIP package 校验

### slicer_cli

类型：命令行可执行程序。

职责：

- 从 `--config` 读取切片配置
- 执行完整切片
- 支持 `--inspect-model` 只检查模型 bbox 和自动摆放结果
- 支持 `--preview-only` 只生成 reports/preview，不写 TIFF layers

### rip_reader_test

类型：命令行可执行程序。

职责：

- 读取 `manifest.json`
- 校验 RGBWSV TIFF 协议
- 校验 layer 文件存在、尺寸一致、通道数、通道顺序、bit depth、planar config、polarity
- 输出每层六通道 checksum

## 3. 当前已实现功能

### 3.1 配置系统

已实现 `SliceConfig`，支持以下配置段：

- `input`
- `output`
- `modelTransform`
- `autoOrient`
- `modelMaterial`
- `support`
- `preview`

当前使用本地轻量 JSON parser/writer：`json_value.h/.cpp`。这样可以在未配置 vcpkg 包路径时仍完成 P0 Demo 构建。

### 3.2 模型导入

已实现：

- ASCII STL 顶点与三角面读取
- Binary STL 检测与三角面读取
- OBJ `v` 顶点读取
- OBJ `f` 面读取
- 支持 `f v/vt/vn` 常见索引格式
- 支持 OBJ 多边形 fan triangulation
- 支持 OBJ `mtllib` / `usemtl` 基础识别
- 统计 face / triangle / material / degenerate triangle
- 计算原始 bbox 和自动摆放后 bbox
- 输出 `model_report.json`

### 3.3 模型变换与自动摆放

已实现：

- 单位换算：`mm` / `cm` / `m` / `inch` / `in`
- scale
- rotationDeg
- translationMm
- `autoOrient`

`autoOrient` 当前策略：

```text
minimize_height_by_right_angle_rotation
```

当模型高度超过 `maxHeightMm` 时，会尝试：

```text
identity
rotate_x_90
rotate_x_minus_90
rotate_y_90
rotate_y_minus_90
```

并选择可将 Z 高度压到阈值内的姿态；若多个姿态满足条件，优先选择 footprint 面积较小的姿态。

`0.3.obj` 当前验证结果：

```text
original height: 26.3582 mm
selectedOrientation: rotate_x_90
oriented height: 5.48746 mm
```

注意：`0.3.obj` 的实际 bbox 会随样例模型文件更新而变化；以上数值用于说明 autoOrient 能将异常高度模型旋转到 P0 高度阈值内。

### 3.4 层采样

已从 bbox 整层填充升级为真实三角面截面采样：

```text
Triangle mesh
→ Z plane intersection
→ 2D segment list
→ scanline fill
→ model mask
```

当前 layer z 采样位置为：

```text
(layerIndex + 0.5) * layerThicknessMm
```

00A 后新增几何诊断：

```text
segmentCount
odd scanline intersection warning
filledSpans
model/support/RGB/W/V 非空像素统计
```

相关数据写入：

```text
reports/slice_report.json
reports/contour_report.json
```

### 3.5 下表面投影支撑

已实现 bottom projection 支撑：

```text
for each XY pixel:
  找到 first model layer
  first model layer 以下写入 S 通道
```

通道优先级遵守：

```text
Model > Support
```

### 3.6 RGBWSV Layer Compose

已按 00B 修正为六通道 `uint8` layer buffer，通道顺序固定：

```text
R G B W S V
```

模型像素：

```text
R/G/B = modelMaterial.rgb
W = modelMaterial.whiteValue
S = background.value
V = modelMaterial.varnishValue
```

支撑像素：

```text
R/G/B/W/V = background.value
S = support.value
```

00B 输出极性固定为：

```text
0   = 打印
255 = 不打印
polarity = black_is_print
```

### 3.7 TIFF Writer / Reader

已实现内部最小 TIFF writer/reader：

- little-endian classic TIFF
- tiled storage
- six-channel
- `uint8`
- contiguous planar config
- 每层一个 TIFF

当前 TIFF writer 不依赖外部 `libtiff`，用于 P0 Demo 验证。

### 3.8 Manifest 和 Reports

每个 package 生成：

```text
manifest.json
reports/model_report.json
reports/slice_report.json
reports/repair_report.json
reports/support_report.json
reports/preview_report.json
reports/contour_report.json
```

`manifest.json` 当前 schema：

```text
p0.rgbwsv.1
```

00B 后 `manifest.json` 的 TIFF 协议字段包含：

```text
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

### 3.9 Preview

已实现 preview 正式化输出，支持 PNG 与保留 PPM。

输出文件类型：

```text
preview/model_rgb_*.png
preview/support_s_*.png
preview/white_w_*.png
preview/varnish_v_*.png
```

用途：

- `model_rgb`：查看模型 RGB 区域
- `support_s`：查看 S 支撑区域，绿色显示
- `white_w`：查看 W 白墨区域，灰白显示
- `varnish_v`：查看 V 光油区域，紫色显示

Preview 显示不直接使用生产值。由于生产数据 `0` 表示打印、`255` 表示空白，preview 会按通道做反相或伪彩色：

```text
visible = 255 - productionValue
support_s = 绿色
white_w = 灰白色
varnish_v = 紫色
```

00A 后支持：

```text
preview.format = png / ppm
preview.channels = rgb / support / white / varnish
preview.layerRange = [start, end]
preview.onlyNonEmptyLayers = true / false
```

`preview_report.json` 会记录每张 preview 的 layer、channel、path、nonZeroPixels、maxValue。

### 3.10 RIP Reader 错误用例

`rip_reader_test` 保持正向 package 校验，并新增负向验收参数：

```text
--expect-error
--expect-message <text>
```

当前已验证错误类型：

```text
错误 bitDepth
错误 channelOrder
缺失 layer TIFF
```

### 3.11 CLI 辅助模式

当前 `slicer_cli` 支持：

```text
--config <path>
--inspect-model
--preview-only
```

`--inspect-model` 不切片，只输出模型路径、格式、顶点数、原始 bbox、自动摆放后的 bbox。

`--preview-only` 运行采样和组层逻辑，但不写 TIFF layers，仅写 manifest/reports/preview，适合大模型验证。

## 4. 当前未实现功能

### 4.1 仍未实现的 P0 内部增强

- OBJ texture / UV 采样
- 边界轮廓修复、非流形修复
- 多轮廓孔洞鲁棒处理
- 正式 mesh repair report
- 使用外部 `libtiff` 进行标准 TIFF 写入
- 使用 Assimp 统一模型导入
- 完整单元测试/自动化测试套件
- 00A 测试样例集仍未全部固化到 `samples/configs`

### 4.2 明确非 P0 范围

根据 `PRD_00` / `DEV_00` / `DEMO_00` / `CODEX_HANDOFF`，当前不做：

- 全彩纹理切片
- 3MF
- glTF / GLB
- 多材料切片
- 复杂支撑树
- 上表面牺牲层
- 侧壁辅助支撑
- Unity
- VTK
- 完整 Qt UI
- GPU 加速
- 板卡控制、运动控制、打印任务执行

## 5. 当前配置文件说明

### 5.1 samples/configs/slice_config.json

默认 sample 配置，输入：

```text
samples/models/sample.stl
```

输出：

```text
output/SlicePackage
```

适合快速验证完整 P0 数据闭环：

```text
slicer_cli → TIFF layers → manifest/reports → rip_reader_test
```

### 5.2 samples/configs/slice_config_model_0_3.json

真实模型配置，输入：

```text
samples/models/0.3.obj
```

输出：

```text
output/SlicePackage_model_0_3
```

推荐先运行：

```text
--inspect-model
--preview-only
```

确认模型姿态、层数、preview 后，再运行完整 TIFF 输出。

### 5.3 关键配置字段

`input`：

- `modelPath`：模型路径
- `format`：`auto` / `stl` / `obj`

`output`：

- `packageDir`：输出包目录
- `dpiX` / `dpiY`：P0 固定要求 600
- `layerThicknessMm`：默认 0.01
- `channelOrder`：固定 `R G B W S V`
- `bitDepth`：00B 后固定 8
- `planarConfig`：固定 `contiguous`
- `tiled`：固定 true
- `tileSize`：默认 `[256, 256]`

`modelTransform`：

- `unit`
- `scale`
- `rotationDeg`
- `translationMm`

`autoOrient`：

- `enabled`
- `maxHeightMm`
- `strategy`

`modelMaterial`：

- `rgb`
- `whiteValue`
- `varnishValue`

`background`：

- `value`：00B 固定要求 255

`support`：

- `enabled`
- `mode`：P0 仅支持 `bottom_projection`
- `value`：00B 默认 0

`preview`：

- `enabled`
- `format`：`png` / `ppm`
- `interval`
- `layerRange`
- `channels`
- `onlyNonEmptyLayers`

## 6. 当前运行方式

### 6.1 配置和构建

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

### 6.2 默认 sample 完整切片

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
```

输出：

```text
output/SlicePackage
```

### 6.3 默认 sample RIP 验证

```powershell
build\Debug\rip_reader_test.exe --package output\SlicePackage
```

### 6.4 真实模型姿态检查

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --inspect-model
```

### 6.5 真实模型 preview-only 验证

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json --preview-only
```

输出：

```text
output/SlicePackage_model_0_3/reports
output/SlicePackage_model_0_3/preview
```

### 6.6 RIP Reader 负向验证

```powershell
build\Debug\rip_reader_test.exe --package output\RipError_bitDepth --expect-error --expect-message "bitDepth must be 8"
build\Debug\rip_reader_test.exe --package output\RipError_channelOrder --expect-error --expect-message "R G B W S V"
build\Debug\rip_reader_test.exe --package output\RipError_missingLayer --expect-error --expect-message "layer TIFF does not exist"
```

### 6.7 真实模型完整 TIFF 输出

```powershell
build\Debug\slicer_cli.exe --config samples\configs\slice_config_model_0_3.json
```

注意：完整 TIFF 输出会写入大量 tiled layer 文件，建议先用 `--preview-only` 验证。

## 7. 当前与 PRD_00 / DEV_00 / DEMO_00 的符合情况

### 7.1 与 PRD_00 的符合情况

已符合：

- 支持 STL
- 支持 OBJ
- 单模型
- 单材料 RGB 配置色
- 下表面投影支撑
- 每层一个 TIFF
- 通道顺序固定为 `R G B W S V`
- 00B 后输出为 `uint8`
- tiled TIFF
- contiguous planar config
- 600 DPI
- 默认层厚 0.01mm
- `slicer_cli --config samples/configs/slice_config.json` 可运行
- 生成 `manifest.json`
- 生成 TIFF layers
- 支撑写入 S 通道
- `rip_reader_test` 可验证数据包
- manifest 写入 `polarity = black_is_print`
- manifest 写入 `printValue = 0` / `emptyValue = 255`

部分超出 PRD_00 的 P0 辅助实现：

- `autoOrient` 自动摆放
- `--inspect-model`
- `--preview-only`
- PNG/PPM preview 输出
- OBJ face 几何截面采样
- 00B 输出位深和打印极性修正
- 00A Preview / 导入 / 几何诊断 / Reports 稳定化增强

仍未覆盖：

- 更正式的 mesh repair
- 更标准的第三方 TIFF / Assimp 集成

### 7.2 与 DEV_00 的符合情况

已覆盖 DEV_00 架构中的主要链路：

```text
slice_config.json
→ ModelLoader
→ ModelNormalizer
→ MeshRepairLite
→ Voxelizer / LayerSampler
→ BottomProjectionSupportGenerator
→ LayerChannelComposer
→ PrivateTiffWriter
→ ManifestWriter
→ ReportWriter
```

当前实现映射：

- `ModelLoader`：`model.cpp`
- `ModelNormalizer`：`modelTransform` + `autoOrient`
- `MeshRepairLite`：仅轻量加载校验，未做正式修复
- `Voxelizer / LayerSampler`：三角面 Z 截面 + scanline raster
- `BottomProjectionSupportGenerator`：最低模型层投影
- `LayerChannelComposer`：`compose_layer`，00B 后使用 `uint8` 与 `0=打印/255=不打印`
- `PrivateTiffWriter`：`tiff_io.cpp`，00B 后写入 8-bit RGBWSV TIFF
- `ManifestWriter / ReportWriter`：`slicer.cpp`

### 7.3 与 DEMO_00 的符合情况

已完成 DEMO_00 的核心实施顺序：

1. `slicer_cli` 空流程：已完成
2. 配置读取：已完成
3. STL/OBJ loader：已完成基础版
4. 层采样：已完成真实三角截面原型
5. 下表面投影支撑：已完成
6. RGBWSV layer composer：已完成
7. TIFF writer：已完成内部版
8. manifest writer：已完成
9. `rip_reader_test`：已完成
10. preview：已完成 PNG/PPM 版
11. Qt demo：未实现

00B 修正状态：

```text
TIFF bitDepth: 8
生产极性: black_is_print
打印值: 0
空白值: 255
通道顺序: R G B W S V
```

00A 稳定化状态：

```text
PNG preview: 已完成
preview channel / interval / layerRange / onlyNonEmptyLayers: 已完成
Binary STL: 已完成
OBJ mtllib/usemtl 基础统计: 已完成
slice_report layer statistics: 已完成
contour_report: 已完成
rip_reader_test expect-error: 已完成
```

## 8. 下一步建议实现 Milestone

### Milestone A：固化 00A 自动化测试和样例集

优先级：高。

建议任务：

- 将临时 Binary STL 验证样例固化到 `samples/models`
- 增加 `multi_material_stub.obj`
- 增加缺层、错误 channelOrder、错误 bitDepth 的自动化脚本
- 增加 preview PNG 文件签名检查
- 增加 `slice_report.json` / `contour_report.json` 字段回归检查

原因：00A 功能已经进入代码路径，但还需要从“人工命令验证”固化为可重复的自动化测试。

### Milestone B：模型导入增强

优先级：高。

建议任务：

- 固化 binary STL 样例配置
- 扩展 OBJ MTL 内容解析
- 输出更完整的 OBJ face/triangle/material 统计
- 输出模型闭合性和非流形风险报告
- 评估是否接入 Assimp

原因：真实模型输入会比当前 sample 更复杂，导入鲁棒性会直接影响切片稳定性。

### Milestone C：几何采样鲁棒性增强

优先级：高。

建议任务：

- 改进多轮廓和孔洞处理
- 增强退化三角面处理
- 增加 slice contour report
- 增加每层 model/support 非零像素统计
- 增加可重复的测试模型集

原因：当前 scanline fill 已可用于 P0 验证，但还不是正式工业级几何内核。

### Milestone D：RIP 协议规范化

优先级：中。

建议任务：

- 生成 `PRD_03_RGBWSV多通道TIFF协议.md`
- 生成 `DEV_03_TIFFWriter与RIPReader协议设计.md`
- 固化 manifest schema
- 定义错误码
- 定义 channel checksum 规则
- 明确 TIFF tag 要求

原因：当前协议可运行，但还需要文档化，方便和 RIP 后端对接。

### Milestone E：Qt 调试 UI

优先级：中。

建议任务：

- 创建 `slicer_qt_demo`
- 加载配置文件
- 显示 CLI/core 日志
- 显示 layer/channel preview
- 后续增加 QOpenGLWidget mesh preview

原因：当前 CLI 已可验证数据链路，下一阶段可以增加调试效率，但仍不建议先做完整产品 UI。

### Milestone F：正式体素/SDF 内核评估

优先级：中低，建议在 P0 数据闭环稳定后进行。

建议任务：

- 生成 `PRD_06_正式SDF体素切片内核.md`
- 生成 `DEV_06_OpenVDB_SDF_LevelSet切片内核设计.md`
- 评估 OpenVDB / CGAL
- 设计稀疏体素、SDF、壳层、白墨/光油厚度模型

原因：这是从 Demo 走向正式切片软件的核心技术升级，但不应阻塞当前 P0 验证链路。
