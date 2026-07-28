# REPORT 13C-03 Unified Production Preview 当前状态

> 文档状态：COMPLETE / 13C-04 READY
> 日期：2026-07-28
> 前置：13C-01/02 COMPLETE
> 下一任务：13C-04 Preview IO 收口

## 1. 阶段结论

13C-03 已把 13C-01 的 manifest/TIFF 权威层源和 13C-02 的 RGBWSV 材料预览合成器接入
Qt 调试工作台。生产预览不再从逐通道 preview PNG 读取材料，也不再根据 preview 文件名推断层号；
所有生产模式均由 manifest 列出的同层 RGBWSV TIFF 异步解码后派生。

本阶段没有关闭 preview PNG 写出，没有修改生产 TIFF、RIP 或配置协议。固定协议仍为：

```text
schema = p0.rgbwsv.2；
通道顺序 = R G B W S V；
bitDepth = uint8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255。
```

## 2. 实现范围

### 2.1 TIFF 原生生产预览

`LayerPreviewPanel` 已演进为 TIFF-native 生产材料预览：

```text
PackageSummary.manifest_path
  -> TiffLayerLoadWorker::IndexPackage
  -> manifest 权威 layerIndex/zMm/dpiX/dpiY
  -> TiffLayerLoadWorker::RequestLayer
  -> RgbwsvLayerBuffer
  -> MaterialPreviewComposer
  -> MaterialPreviewImageAdapter
  -> QImage
  -> LayerPreviewPanel
```

`LayerPreviewDataProvider::LoadProductionMetadata` 只读取生产预览必要的 manifest、slice report、
调色板和语义元数据，不读取 preview report 或 preview PNG。旧 `Load` 路径继续服务诊断视图。

### 2.2 材料模式

生产预览提供 13 个稳定模式：

```text
RGB 真彩；
R / G / B；
W 白墨；
S 支撑；
V 光油；
RGB + W；
RGB + S；
RGB + V；
RGB + S + W + V；
材料占用；
真实空白。
```

默认模式为 `RGB + S + W + V`。切换模式只重新合成当前不可变
`RgbwsvLayerBuffer`，不会再次读取 TIFF。

### 2.3 Qt 显示适配

新增 `MaterialPreviewImageAdapter`，职责限定为：

```text
稳定模式 ID 与核心 MaterialPreviewMode 映射；
中文显示名称；
UI-only W/S/V/Empty 伪彩图色到核心调色板转换；
RGBA 深拷贝到 QImage；
仅在显示边界执行一次 Y 翻转。
```

适配器不读文件、不决定生产材料、不做跨层兜底。

## 3. 异步、缓存与失败策略

生产 TIFF 解码通过 `TiffLayerLoadWorker` 在线程池执行，UI 主线程不直接解码 TIFF。

当前行为：

```text
IndexPackage 保存 manifest 权威 package index 快照；
每次请求使用 consumerId、generation、layerIndex 三重身份；
连续切层只接受最新 generation；
迟到结果不能覆盖当前层；
模式切换不增加 layer request；
cache hit/miss 通过同一结果路径更新界面；
加载失败清空旧图并显示稳定错误，不显示上一层伪装成功；
Widget 析构前取消逻辑请求。
```

## 4. 层序、物理比例与探针

层列表严格按 manifest 的真实 `layerIndex` 升序显示，即低 Z 到高 Z。状态区显示：

```text
当前序号/总层数；
真实 layerIndex；
zMm；
dpiX/dpiY；
模式；
cache hit/miss；
RGB/W/S/V/occupied/empty/multiMaterial 统计；
数据源和加载错误。
```

生产 buffer 保持 TIFF 原始坐标，显示时执行：

```text
displayX = rawX；
displayY = height - 1 - rawY。
```

像素探针先把显示坐标映射回原始坐标，再调用
`MaterialPreviewComposer::Probe` 返回精确 R/G/B/W/S/V 生产值和打印通道。探针不从伪彩显示颜色
反推材料。

## 5. 预览信息架构

`PreviewWorkspace` 一级入口已收敛为：

```text
生产预览；
诊断预览。
```

旧 `PreviewOverlayPanel` 和 `PreviewPanel` 没有删除，而是保留在“诊断预览”的次级入口：

```text
材料叠加/闭环诊断；
原始调试预览。
```

三个视图继续共享同一个真实 `layerIndex`。材料闭环报告跳转会进入“诊断预览/材料叠加”，不改变
生产 TIFF 预览的数据源边界。

## 6. 自动化覆盖

新增 UI smoke：

```text
tiff-native-preview-all-materials
```

覆盖：

```text
清空 PackageSummary.preview_paths 后生产预览仍可使用；
数据源为 manifest/layers RGBWSV TIFF；
13 种材料模式均可合成；
首层、中间层、末层的真实 layerIndex/zMm/dpi；
模式切换不增加 TIFF 请求；
六通道精确探针；
快速连续切层只接受最后请求；
生产/诊断两个一级入口；
诊断 Overlay/Raw 仍可访问。
```

现有 smoke 已同步适配异步 TIFF 数据源和两级预览入口：

```text
layer-preview-load；
preview-workspace-shared-layer；
preview-legend-probe-context；
preview-physical-aspect；
material-closure-diagnostics。
```

## 7. 实际验证

2026-07-28 在最终工作树上实际运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-all-materials --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-workspace-shared-layer --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case material-closure-diagnostics --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-legend-probe-context --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-physical-aspect --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check -- apps/slicer_debug_ui
```

结果：

```text
Debug 全量构建：PASS；
Debug 全量 CTest：75/75 PASS；
tiff-native-preview-all-materials：PASS，25 层、13 模式、TIFF 数据源；
preview-workspace-shared-layer：PASS；
material-closure-diagnostics：PASS；
layer-preview-load：PASS；
preview-legend-probe-context：PASS；
preview-physical-aspect：PASS，635/600 非等方 DPI 校正有效；
Qt UI self-test：PASS；
Quick CI：PASS，包含 regression、schema、support、golden、UI 和真实 Overlay smoke；
git diff --check：PASS。
```

## 8. 未完成与下一 Gate

13C-03 明确未处理：

```text
关闭或删除 preview PNG；
修改 preview 输出 schema；
删除旧诊断 Panel；
Texture Surface/Model Fill/Partition 诊断推断；
生产 package、TIFF 或 RIP 改造；
新第三方依赖。
```

下一任务为 `13C-04 Preview IO 收口`：常规生产默认不写重复通道 PNG，诊断 preview 按需启用，
保留旧配置兼容并记录关闭前后的 IO/耗时证据。13C-04 完成后才进入 13C-05 阶段总收口。
