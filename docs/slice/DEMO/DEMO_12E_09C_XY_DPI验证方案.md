# DEMO 12E-09C X/Y DPI 验证方案

> 状态：COMPLETE / 09C-01..06 PASS
> 日期：2026-07-24

## 1. 配置矩阵

| Case | dpiX | dpiY | 期望 |
|---|---:|---:|---|
| omitted-default | 省略 | 省略 | 635/600 |
| legacy-compatible | 600 | 600 | PASS，旧输出合同不变 |
| production-default | 635 | 600 | PASS，非等方像素 |
| symmetric-high | 1200 | 1200 | PASS |
| invalid-zero | 0 | 600 | FAIL |
| invalid-negative | 635 | -1 | FAIL |
| invalid-ui-high | 2401 | 600 | UI 拒绝 |
| inconsistent-manifest | 635 | 600 | dpi[] 或 pixelSize 不一致时 Reader FAIL |

## 2. 引擎矩阵

```text
Legacy + 600/600；
Legacy + 635/600；
Global restricted + 635/600；
Global material parity + 635/600。
```

同一模型同一 DPI 时比较物理包围盒和 layerCount；不得用不同 DPI 的总耗时直接比较引擎性能。

首批矩阵固定采用以下物理范围容差：

```text
Legacy 600/600 vs Legacy 635/600：0.05 mm；
Legacy 635/600 vs Global restricted 635/600：0.25 mm。
```

前者覆盖一像素量化差；后者覆盖 Global partition 0.2 mm 体素边界与一像素 raster 量化。
Global material parity 因外侧光油会主动扩张 XY，只记录外侧光油半径和有效厚度，不与 restricted
case 强制同尺寸。

## 3. UI Matrix

```text
新建/导入配置显示 635/600；
修改 X、Y 后像素尺寸即时更新；
保存、回读、另存为、回退正确；
一键切片的 session Effective Config 使用当前值；
模式/Profile/DPI 改变后旧 package 标记 stale；
635/600 预览按物理尺寸显示，不出现约 5.83% 的横向拉伸；
1280x720、1440x900、1920x1080 无遮挡。
```

## 4. Package 与 Reader

每个正向 case 验证：

```text
manifest grid.dpiX/dpiY；
pixelSizeXmm/pixelSizeYmm；
TIFF width/height；
layer list；
preview/report 同源；
RIP strict PASS；
RGBWSV/uint8/black_is_print 不变。
```

外侧光油 case 还需验证 X/Y 离散半径和实际物理厚度，不允许继续用单一 42.3um 计算两轴。

## 5. 回归

```powershell
cmake --build build --config Debug
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

09C-06 已执行 `xiao_ma_wu_yu_new` Release 四 case，覆盖 Legacy 600/600、Legacy 635/600、
Global restricted 635/600 和 Global material parity 635/600；四个 package 的 TIFF、
preview/report、RIP strict、物理范围与外侧光油均通过。完整结果见
`REPORT_12E_09C_XY_DPI当前状态.md`。
