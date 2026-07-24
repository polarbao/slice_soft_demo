# TASKS 12E-09C X/Y DPI 任务清单

> 状态：PREPARATION COMPLETE / 09C-01 READY
> 日期：2026-07-23

## 1. 09C-01 核心配置与协议边界

目标：

```text
默认值改为 635/600；
集中 DPI 合法范围；
移除固定 600/600 校验；
保持显式旧配置兼容。
```

验证：default、explicit 600、635/600、零、负数、过大值单测。

## 2. 09C-02 Core 与 RIP Reader 非等方 DPI

目标：

```text
移除固定 600/600 validator；
Reader 接受合法非等方 DPI；
校验 dpi、pixelSize、grid 和 TIFF 尺寸一致；
补充 bad package 负向测试。
```

## 3. 09C-03 两引擎 Raster 与外侧光油

目标：

```text
Legacy/Global 按 X/Y pitch 建网格；
外侧光油按 X/Y 物理半径离散化；
报告记录两轴实际厚度；
同模型同 DPI 的两引擎物理范围可比较。
```

## 4. 09C-04 Qt 配置与一键切片

目标：

```text
QuickConfigPanel 新增 X/Y DPI；
默认 635/600；
显示物理像素尺寸；
一键切片读取当前值，不硬编码；
改变 DPI 使 admission/package stale。
```

验证：ConfigDocument、self-test、三窗口尺寸和最长中文 smoke。

## 5. 09C-05 物理比例 Preview

目标：

```text
LayerPreviewPanel 和 PreviewOverlayPanel 使用 manifest X/Y pixel size；
按物理宽高显示非等方图像；
状态栏显示 dpi 和 pixel size；
缺失 grid 元数据时明确降级提示。
```

## 6. 09C-06 生产矩阵与收口

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
