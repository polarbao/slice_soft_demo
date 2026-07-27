# REPORT 12E-09C X/Y DPI 当前状态

> 状态：COMPLETE / 09C-01..06 PASS
> 日期：2026-07-24
> 当前结论：600/600 与 635/600 首批软件生产组合通过；未包含打印机硬件标定

## 1. 阶段目标

12E-09C 将输出分辨率从隐含方形像素改为显式 X/Y 独立合同，覆盖配置、两套生产引擎、
RGBWSV package、RIP Reader、Qt 设置、一键切片和物理比例预览。

本阶段保持以下生产不变量：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Legacy 默认；Global 显式 opt-in；禁止 silent fallback
```

## 2. 已完成内容

| 原子任务 | 状态 | 结果 |
|---|---|---|
| 09C-01 | COMPLETE | 默认 X=635/Y=600、72..2400 范围、显式 600/600 兼容 |
| 09C-02 | COMPLETE | Writer/Reader 独立 DPI、物理像素一致性与坏包校验 |
| 09C-03 | COMPLETE | Legacy/Global 非等方 Raster、外侧光油 X/Y 物理离散 |
| 09C-04 | COMPLETE | Qt 配置、保存回读、Effective Config 与一键切片透传 |
| 09C-05 | COMPLETE | Layer/Overlay Preview 按物理比例显示并提供旧包降级提示 |
| 09C-06 | COMPLETE | Release 真实模型生产矩阵、RIP strict、Debug/Release 与回归收口 |

新增脚本：

```text
scripts/run_12e_09c_06_dpi_matrix.ps1
```

脚本固定验证同一真实模型、同一 0.01 mm 层厚下的四个生产 case，并输出机器可读摘要：

```text
output/benchmarks/12e_09c_06_dpi_matrix/dpi_matrix_summary.json
schema = slicesoft.xy_dpi_matrix.12e_09c.1
```

## 3. 真实模型生产矩阵

模型：

```text
model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj
```

Release 实测结果：

| Case | DPI | Grid | 物理范围 mm | sliceProcessingMs | outputWriteMs | 峰值工作集 MiB |
|---|---|---|---|---:|---:|---:|
| Legacy 显式兼容 | 600/600 | 284x550x551 | 12.0227x23.2833 | 5995.482 | 3961.448 | 685.0 |
| Legacy 非等方 | 635/600 | 301x550x551 | 12.0400x23.2833 | 6085.686 | 3491.885 | 720.3 |
| Global restricted | 635/600 | 303x552x564 | 12.1200x23.3680 | 32593.220 | 16907.754 | 5839.3 |
| Global material parity | 635/600 | 307x556x564 | 12.2800x23.5373 | 34920.056 | 13530.808 | 5943.7 |

以上时间和内存只属于当前参考机器证据，不是产品 SLA，也不能把不同 DPI 的总耗时直接作为引擎
优劣结论。09C 不改变 09B 已冻结的结论：Legacy 继续作为默认引擎。

## 4. 物理一致性

```text
Legacy 600/600 vs 635/600：
  物理宽度差 0.017333 mm，物理高度差 0 mm，容差 0.05 mm，PASS。

Legacy vs Global restricted，均为 635/600：
  物理宽度差 0.080000 mm，物理高度差 0.084667 mm，
  容差 0.25 mm，PASS。
```

Global material parity 的外侧光油结果：

```text
requestedThicknessMm = 0.05
radiusXPx = 2
radiusYPx = 2
effectiveThicknessXmm = 0.080000
effectiveThicknessYmm = 0.084667
pixelPitchSource = output_dpi
```

两轴分别按 `25.4/dpiX` 与 `25.4/dpiY` 离散，不再用单一 42.3 um 同时计算 X/Y。

## 5. Package 与 UI 验证

四个 case 均满足：

```text
manifest / slice_report requestedPipelineMode、effectivePipelineMode 一致；
productionOutputWritten = true；
fallbackApplied = false；
manifest grid.dpiX/dpiY、dpi[]、pixelSizeXmm/Ymm、pixelSizeMm[] 一致；
TIFF layer list 完整且文件存在；
preview_report = p0.preview_report.1，且预览文件属于当前 package；
RIP Reader strict PASS；
要求的 RGB/W/S/V 通道打印像素存在，restricted 的 S/V 保持空；
RGBWSV、uint8、black_is_print 不变。
```

Qt Smoke 覆盖：

```text
self-test
slice-settings-model
generated-effective-config
preview-physical-aspect
production-mode-selector
```

其中物理比例 fixture 在 635/600 下按 94x100 显示；缺少物理元数据的旧包明确降级为方形像素。

## 6. 实际验证

已执行并通过：

```powershell
.\scripts\run_12e_09c_06_dpi_matrix.ps1 -BuildDir build -Config Release
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

结果：

```text
09C Release matrix：4/4 PASS；
Debug CTest：58/58 PASS；
Release CTest：58/58 PASS；
schema：PASS；
golden：PASS；
Quick CI：PASS。
```

## 7. 当前边界

```text
600/600 与 635/600 是首批软件 package/RIP 认证组合，不等于打印机硬件标定；
TIFF 单文件仍不写 XResolution/YResolution，物理尺寸以 package manifest 为准；
Global 仍是显式候选 Profile，未替换 Legacy 默认路径；
复杂自相交浮雕模型仍按 strict admission 阻断；
09C 不实现 RIP 半色调、设备通信或打印机运动补偿。
```

## 8. 后续阶段与准备度

正式顺序保持：

```text
12E-09C COMPLETE
  -> 12E-09A-02..06 Diagnostic UI
  -> 12E-10A..D Final Closure
```

下一原子任务应为 `12E-09A-02 Diagnostic Effective Config`。现有任务清单已给出 09A-02..06
的目标和依赖，但正式准备文档还不完整：当前只有职责决策、09A-01 报告和 TASKS，缺少独立的
09A PRD、DEV、DEMO、CODEX_PROMPT 以及 09A-02 的详细 schema/验收夹具定义。因此 09A
可以确认方向，但在开发前应先补齐这些执行级文档。

12E-10 的 Final Closure Matrix schema、模型范围和 10A..D 粒度已经定义，但准备文档仍引用旧的
09B 等待状态，且缺少独立 PRD/DEV/DEMO/TASKS/CODEX_PROMPT。09C 完成后：

```text
10B/10C 的 09C 依赖已解除；
10A 仍等待 09A-05；
10D 仍等待 10A/10B/10C；
正式启动 12E-10 前需刷新准备文档并补齐独立执行文档。
```
