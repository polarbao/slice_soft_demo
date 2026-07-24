# DEV 12E-09C X/Y DPI 配置、Reader 与 UI 设计

> 状态：IN PROGRESS / 09C-01..05 COMPLETE / 09C-06 READY
> 日期：2026-07-24

## 1. 初始实现审计

| 位置 | 当前状态 | 09C 改造 |
|---|---|---|
| `src/slicer_core/config.h` | 默认 600/600 | 默认改为 635/600 |
| `src/slicer_core/config.cpp` | 可解析两轴，但强制等于 600 | 改为合法范围校验 |
| `src/slicer_core/rip_reader.cpp` | manifest DPI 固定校验 600 | 校验范围和内部一致性 |
| `apps/slicer_debug_ui/MainWindow.cpp` | 一键配置硬编码 600/600 | 读取当前 session 设置 |
| `QuickConfigPanel` | 无 DPI 控件 | 新增 X/Y QSpinBox 和像素尺寸提示 |
| Legacy raster | 已分别使用 dpi_x/dpi_y | 增加非等方回归 |
| Global raster | 已分别计算 X/Y pitch | 增加与 Legacy 同合同测试 |
| manifest/writer | 已写两轴字段 | 增加一致性断言 |
| 外侧光油 | 使用单一 `pixelPitchUm=42.3` | 改为按 X/Y pitch 分别离散化 |
| Layer/Overlay preview | 按图像像素等比例显示 | 按物理像素纵横比校正 |
| TIFF IFD | 当前未写 X/Y Resolution 标签 | 本阶段继续以 manifest 为物理真源，是否加标签另立兼容任务 |

## 2. 配置合同

```json
{
  "output": {
    "dpiX": 635,
    "dpiY": 600
  }
}
```

规则：

```text
省略字段：采用产品默认 635/600；
显式字段：保留用户值；
UI 范围：72..2400；
核心/Reader：使用同一防御性范围常量，不能各自复制魔法数；
pixelSizeXmm = 25.4 / dpiX；
pixelSizeYmm = 25.4 / dpiY。
```

建议在 `slicer_core/config` 暴露集中常量或校验函数，UI 可采用更窄的产品范围，但核心和 Reader
必须共享协议边界。

## 3. UI 设计

在 `QuickConfigPanel` 的基础/输出区域新增：

```text
X 方向 DPI：[635]
Y 方向 DPI：[600]
像素尺寸：X 0.040000 mm / Y 0.042333 mm
```

行为：

```text
使用 QSpinBox；
槽函数以 On 开头；
使用函数指针 connect；
值变化写入 ConfigDocument 的 output.dpiX/dpiY；
文档变化时阻断信号并回读；
改变 DPI 后标记当前 admission/package stale；
tooltip 说明 DPI 决定 raster 密度，不等于模型 scale。
```

预览显示尺寸应使用：

```text
physicalWidth = widthPx * pixelSizeXmm；
physicalHeight = heightPx * pixelSizeYmm。
```

不能继续直接用 `QImage::size()` 推导物理纵横比。

## 4. 一键切片

`MainWindow` 生成 session config 时不得硬编码 DPI。应按以下优先级：

```text
当前 ConfigDocument 显式值；
当前 Profile 显式值；
产品默认 635/600。
```

最终值只写入 session Effective Config，不覆盖原始 fixture。

## 5. Reader 设计

Reader 应读取并校验：

```text
grid.dpiX/grid.dpiY 为整数且在防御范围内；
grid.dpi[] 若存在，与 dpiX/dpiY 一致；
pixelSizeXmm/pixelSizeYmm 与 25.4/dpi 在容差内；
pixelSizeMm[] 若存在，与独立字段一致；
TIFF width/height 与 manifest grid 一致。
```

保持现有 `ValidationErrorCode::GridInvalid` 或新增等价稳定错误码，不得把非法 DPI 当警告继续。

## 6. Golden 与基线策略

```text
现有 fixture 显式 600/600：保持原值和 hash；
新增最小 635/600 fixture：验证新默认和非等方协议；
默认值测试：省略 dpiX/dpiY；
性能比较：同一组引擎对照必须固定同一 DPI，不把 600 与 635 混比；
真实模型矩阵：至少一个 Legacy 和一个 Global case 使用 635/600。
```

## 7. 外侧光油

当前 `OuterVarnishShellConfig.pixel_pitch_um` 和单一 `thicknessPx` 只适用于近似方形物理像素。09C
需要把厚度离散化为 X/Y 两轴半径，或由物理距离场直接判定：

```text
radiusXPx = ceil(thicknessMm / pixelSizeXmm)；
radiusYPx = ceil(thicknessMm / pixelSizeYmm)。
```

不得用 X 半径同时扩张 Y。报告应记录 requested thickness、X/Y 半径和两轴 effective thickness。

## 8. 测试层次

```text
L1 config default/range/parser 单测；
L2 RIP Reader 635/600 正向和坏包负向；
L3 Legacy/Global raster 尺寸、外侧光油和物理范围一致；
L4 QuickConfigPanel 保存/回读/tooltip/self-test；
L5 Layer/Overlay preview 物理纵横比；
L6 一键切片 session config、manifest、TIFF、preview/report、RIP strict；
L7 旧 600/600 golden 和 Quick CI 回归。
```

## 9. 风险

```text
默认 X DPI 提高会增加宽度像素数、TIFF 数据量和 X 向计算量；
旧文档中的“1px=42.3um”只对 600 dpi 近似成立；
非等方像素可能暴露使用单一 pixelSizeMm 的旧算法；
直接修改所有 fixture 会破坏历史可比性；
Reader 只放宽固定值而不校验物理尺寸，会允许不一致 package。
TIFF 单文件暂不携带 X/Y Resolution 标签，脱离 package 的外部查看器只能看到像素尺寸；
若后续需要在 TIFF IFD 增加 Resolution 标签，必须单独评估旧 hash、Reader 和第三方兼容性。
```
