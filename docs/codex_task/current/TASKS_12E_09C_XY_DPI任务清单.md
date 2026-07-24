# TASKS 12E-09C X/Y DPI 任务清单

> 状态：09C-01..05 COMPLETE / 09C-06 READY
> 日期：2026-07-24

## 1. 09C-01 核心配置与协议边界

状态：COMPLETE（2026-07-24）

目标：

```text
默认值改为 635/600；
集中 DPI 合法范围；
移除固定 600/600 校验；
保持显式旧配置兼容。
```

验证：default、explicit 600、635/600、零、负数、过大值单测。

实际落点：

```text
OutputConfig 新默认固定为 dpiX=635、dpiY=600；
集中定义默认值和 72..2400 防御范围；
移除配置层固定 600/600 限制；
显式 600/600 与合法非等方 DPI 保持兼容；
新增 output_resolution_config_unit_tests。
```

验证：

```text
output_resolution_config_unit_tests PASS；
experimental_config_unit_tests PASS；
slice_pipeline_router_unit_tests PASS。
```

## 2. 09C-02 Core 与 RIP Reader 非等方 DPI

状态：COMPLETE（2026-07-24）

目标：

```text
移除固定 600/600 validator；
Reader 接受合法非等方 DPI；
校验 dpi、pixelSize、grid 和 TIFF 尺寸一致；
补充 bad package 负向测试。
```

实际落点：

```text
共享 25.4 / DPI 物理像素换算与 1e-9 mm 一致性判断；
RIP Reader 严格读取独立 dpiX/dpiY 和 pixelSizeXmm/pixelSizeYmm；
允许 72..2400 范围内合法非等方 DPI，并保留显式 600/600；
冗余 dpi[]、pixelSizeMm[] 存在时必须与独立字段一致；
manifest grid 与 TIFF 尺寸一致性校验保持不变；
RgbwsvPackageWriter 输出并校验独立 DPI 与物理像素字段；
RIP 摘要输出两轴 DPI 和物理像素尺寸；
新增 rip_reader_resolution_unit_tests 正向、兼容和 bad package 覆盖。
```

验证：

```text
rip_reader_resolution_unit_tests PASS；
rgbwsv_production_package_writer_unit_tests PASS；
global_surface_shell_production_pipeline_unit_tests PASS；
rip_reader_test --summary PASS；
Debug full build PASS；
CTest Debug 56/56 PASS；
schema tests PASS；
golden tests PASS。
Quick CI PASS。
```

## 3. 09C-03 两引擎 Raster 与外侧光油

状态：COMPLETE（2026-07-24）

目标：

```text
Legacy/Global 按 X/Y pitch 建网格；
外侧光油按 X/Y 物理半径离散化；
报告记录两轴实际厚度；
同模型同 DPI 的两引擎物理范围可比较。
```

实际落点：

```text
新增共享 OuterVarnishDiscretization，按 output.dpiX/dpiY 独立计算 X/Y 像素尺寸和离散半径；
Legacy 与 Global 均使用物理椭圆判定外侧光油，不再以单一 pixelPitchUm 同时扩张两轴；
两引擎建网格时分别使用 X/Y 物理 padding，并遵守 allowXYExpansion；
共享 RGBWSV writer 和 slice_report 记录 requested thickness、X/Y 半径、两轴 effective thickness 与 pixelPitchSource；
pixelPitchUm 只保留旧配置兼容和报告追踪，不再作为 09C 生产物理真源；
新增非等方 Raster、外侧光油离散和 Global 生产管线单测。
```

验证：

```text
outer_varnish_discretization_unit_tests PASS；
non_square_raster_pipeline_unit_tests PASS；
global_surface_shell_material_evidence_unit_tests PASS；
global_surface_shell_production_pipeline_unit_tests PASS；
rgbwsv_production_package_writer_unit_tests PASS；
Debug full build PASS；
CTest Debug 58/58 PASS。
```

## 4. 09C-04 Qt 配置与一键切片

状态：COMPLETE（2026-07-24）

目标：

```text
QuickConfigPanel 新增 X/Y DPI；
默认 635/600；
显示物理像素尺寸；
一键切片读取当前值，不硬编码；
改变 DPI 使 admission/package stale。
```

验证：ConfigDocument、self-test、三窗口尺寸和最长中文 smoke。

已提前完成的 UI 子范围：

```text
配置 -> 常用 -> 基础新增独立 X/Y DPI 控件；
范围 72..2400，缺省显示 635/600；
实时显示 25.4 / DPI 的 X/Y 物理像素尺寸；
配置保存、重新加载和非法范围校验已覆盖；
Legacy/Global Effective Config 与 OpenVDB 候选配置不再硬编码 600/600；
配置变更沿用既有 changed 信号，使 admission/package 标记为 stale。
```

端到端验证：

```text
slicer_debug_ui --self-test PASS；
slice-settings-model PASS；
generated-effective-config PASS；
setting-help-metadata PASS；
production-mode-selector 在 1280x720、1440x900、1920x1080 PASS。
```

## 5. 09C-05 物理比例 Preview

状态：COMPLETE（2026-07-24）

目标：

```text
LayerPreviewPanel 和 PreviewOverlayPanel 使用 manifest X/Y pixel size；
按物理宽高显示非等方图像；
状态栏显示 dpi 和 pixel size；
缺失 grid 元数据时明确降级提示。
```

实际落点：

```text
新增共享 PreviewPhysicalScaleResolver；
LayerPreviewPanel 和 PreviewOverlayPanel 优先读取 manifest grid，slice_report 作为低优先级补充；
预览尺寸按 widthPx*pixelSizeXmm 与 heightPx*pixelSizeYmm 的物理比例校正；
图像缩放改为按物理目标尺寸非等比映射，避免 635/600 被按方形像素拉伸；
状态栏显示 DPI 和 X/Y 物理像素尺寸；
缺失或不一致的物理元数据明确降级为方形像素显示。
```

验证：

```text
preview-physical-aspect PASS：635/600 的 100x100 raster 显示基准为 94x100；
缺失物理元数据时 100x100 方形降级并显示警告；
layer-preview-load PASS；
overlay-load-real PASS；
preview-workspace-shared-layer PASS；
slicer_debug_ui --self-test PASS；
schema tests PASS；
golden tests PASS；
Quick CI PASS。
```

## 6. 09C-06 生产矩阵与收口

状态：READY

目标：

```text
两种引擎在 635/600 下生成 TIFF/package；
manifest、preview/report、RIP strict 一致；
同模型同 DPI 比较物理范围；
外侧光油与 preview 物理比例验证；
保留显式 600/600 regression；
Debug/Release、schema/golden/Quick CI；
用户手册和输出合同更新；
状态报告、总览、索引和上下文同步。
```

固定状态报告：

```text
docs/slice/REPORT/REPORT_12E_09C_XY_DPI当前状态.md
```

## 7. 执行规则

```text
每次只执行用户明确授权的原子任务；
测试先行；
不得批量修改历史 fixture；
不得改变生产通道、位深或极性；
不得把 600/600 与 635/600 的耗时直接作为引擎优劣对比。
```
