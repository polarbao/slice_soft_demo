# REPORT 13C-02 MaterialPreviewComposer 当前状态

> 文档状态：COMPLETE / 13C-03 PREPARATION AUDIT NEXT
> 日期：2026-07-28
> 前置：13C-01 COMPLETE
> 下一任务：补齐并复核 13C-03 Unified Production Preview 执行级合同

## 1. 阶段结论

13C-02 已在 `slicer_core` 建立无 Qt 的确定性 RGBWSV 材料预览合成器。合成器只接受
13C-01 解码得到的同层 `RgbwsvLayerBuffer`，输出连续 RGBA 显示数据、生产通道统计和精确
六通道像素探针。

本阶段没有接入 Qt Widget、没有读取或改写 preview PNG、没有修改生产 TIFF，也没有改变
`p0.rgbwsv.2`、`R G B W S V`、uint8 或 `black_is_print`。

## 2. 核心实现

新增：

```text
src/slicer_core/preview/MaterialPreviewComposer.h；
src/slicer_core/preview/MaterialPreviewComposer.cpp；
tests/unit/material_preview_composer/Main.cpp；
material_preview_composer_unit_tests。
```

已支持显示模式：

```text
R / G / B / W / S / V；
RGB；
RGB+W / RGB+S / RGB+V；
RGB+S+W+V；
Occupancy；
Empty。
```

固定显示语义：

```text
RGB 直接显示 TIFF 原始 R/G/B 生产值，不做整图反相；
W/S/V 使用 coverage=255-channelValue 计算伪彩覆盖率；
组合模式按 RGB -> W -> S -> V 的固定显示顺序合成；
空白区域使用显示用白色背景；
显示顺序不改变生产材料优先级和 TIFF 原始值。
```

## 3. 统计、探针与错误

合成结果携带：

```text
sourceIdentity；
真实 layerIndex / zMm；
width / height；
dpiX / dpiY；
pixelCount；
rgbPixels / whitePixels / supportPixels / varnishPixels；
occupiedPixels / emptyPixels / multiMaterialPixels。
```

`Probe` 返回指定 TIFF 原始坐标的 R/G/B/W/S/V 六通道值和材料打印标志，不使用显示伪彩
反推生产数据。

稳定错误码：

```text
MATERIAL_PREVIEW_BUFFER_INVALID；
MATERIAL_PREVIEW_DIMENSION_INVALID；
MATERIAL_PREVIEW_PIXEL_OUT_OF_RANGE；
MATERIAL_PREVIEW_MODE_INVALID。
```

错误不会降级为白图成功结果。

## 4. 自动化覆盖

`material_preview_composer_unit_tests` 覆盖：

```text
R/G/B/W/S/V 单通道；
RGB、RGB+W、RGB+S、RGB+V、RGB+S+W+V；
0、部分覆盖值和 255；
同像素多材料及固定覆盖顺序；
Empty/Occupancy；
sourceIdentity、layerIndex、zMm、尺寸和 DPI 透传；
生产统计；
精确六通道探针；
坏尺寸、坏字节数、越界探针和非法模式。
```

## 5. 实际验证

2026-07-28 实际运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
ctest --test-dir build -C Debug -R "^(material_preview_composer|tiff_layer_(source|cache))_unit_tests$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check
```

结果：

```text
Debug 全量构建：PASS；
Debug 全量 CTest：75/75 PASS；
13C-01/02 定向 CTest：3/3 PASS；
Qt UI self-test：PASS；
Quick CI：PASS，包含 regression、golden、UI self-test 和真实 Overlay smoke；
git diff --check：PASS。
```

## 6. 未完成与下一 Gate

仍未实现：

```text
MaterialPreviewResult 到 QImage 的 UI adapter；
TiffLayerLoadWorker 与统一生产预览 Widget 接线；
真实 layerIndex/zMm/dpiX/dpiY 的统一滑层显示；
RGB+S+W+V 的 UI 选项和六通道像素探针交互；
默认关闭重复生产 preview PNG；
无 preview 目录 UI smoke。
```

`13C-02` 已满足 `13C-03` 的代码前置。下一原子步骤先复核并补齐 13C-03 的任务级
PREP/PROMPT，再进入 Unified Production Preview 开发。
