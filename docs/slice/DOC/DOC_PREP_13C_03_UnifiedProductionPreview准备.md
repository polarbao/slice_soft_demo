# DOC_PREP 13C-03 Unified Production Preview 准备

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.0
> 日期：2026-07-28
> 对应任务：13C-03
> 前置：13C-01/02 COMPLETE

## 1. 目标

把 13C-01 的 manifest/TIFF 权威层源和 13C-02 的 RGBWSV 材料合成器接入 Qt 工作台，使生产
预览只从同一层生产 TIFF 派生，不再依赖逐通道 preview PNG。

主预览收敛为两个一级入口：

```text
生产预览：RGBWSV TIFF 权威数据；
诊断预览：report、semantic mask、closure gap 和原始调试图。
```

13C-03 不关闭 preview PNG 写出；该 IO 收口属于 13C-04。

## 2. 当前代码事实

已完成：

```text
TiffLayerSource：manifest 权威层表、stripped/tiled、真实 layerIndex/zMm/dpiX/dpiY；
TiffLayerCache：默认 5 层/256 MiB LRU；
TiffLayerLoadWorker：Qt 线程池、generation、cancel、stale 和稳定错误；
MaterialPreviewComposer：R/G/B/W/S/V、RGB 组合、全材料、统计和六通道探针；
PreviewWorkspace：共享真实 layerIndex、统一图例和三个旧预览 Panel；
PreviewPhysicalScale：635/600 等非等方 DPI 显示。
```

仍存在：

```text
LayerPreviewPanel 的 W/S/V/Occupancy 仍主要读取 preview PNG；
LayerPreviewPanel 的 RGB 和像素探针会各自同步解码 TIFF；
PreviewOverlayPanel 仍以 preview PNG 组合材料；
PreviewWorkspace 仍暴露“生产层检查/材料叠加/原始调试预览”三个一级入口；
没有 RGB+S+W+V 的生产 UI 选项；
快速滑层尚未接入 13C-01 Worker。
```

## 3. 实施决策

### 3.1 不新增重复的第四个预览 Panel

复用现有 `PreviewWorkspace` 和 `LayerPreviewPanel`：

```text
PreviewWorkspace：继续承担统一容器、共享层和图例；
LayerPreviewPanel：演进为 TIFF-native 生产材料预览；
PreviewOverlayPanel：降为诊断材料/closure 预览，不再代表生产材料叠加；
PreviewPanel：保留原始诊断图；
诊断预览内部通过次级选择器切换 Overlay/Raw；
一级入口只保留“生产预览/诊断预览”。
```

遵守 `wrap first, move later, rewrite last`。13C-03 不删除旧 Panel 和旧 preview 读取代码；只有生产
入口不再调用这些旧路径。

### 3.2 数据流

```text
PackageSummary.manifest_path
  -> TiffLayerLoadWorker::IndexPackage
  -> manifest 真实 layerIndex 列表
  -> TiffLayerLoadWorker::RequestLayer
  -> RgbwsvLayerBuffer
  -> MaterialPreviewComposer
  -> QImage 深拷贝
  -> 仅显示坐标执行一次 Y 翻转
  -> LayerPreviewPanel
```

生产模式切换只重新合成当前缓存 buffer，不重复读 TIFF。层切换才发起异步 layer request。

### 3.3 Package 索引 DTO

`TiffLayerLoadWorker` 在成功 `IndexPackage` 后保存 UI 可读快照，至少提供：

```text
packageIdentity；
manifestHash；
width/height；
dpiX/dpiY；
按 layerIndex 升序的 layerIndex/zMm 列表。
```

该快照只能来自 `TiffLayerSource::IndexPackage` 返回值，不允许再按 preview 文件名构造生产层范围。
切换 package 时先取消旧请求，再替换快照。

## 4. 生产预览 UI 合同

### 4.1 模式

中文选项和核心枚举映射：

| UI 名称 | `MaterialPreviewMode` |
|---|---|
| RGB 真彩 | `Rgb` |
| R 通道 | `Red` |
| G 通道 | `Green` |
| B 通道 | `Blue` |
| W 白墨 | `White` |
| S 支撑 | `Support` |
| V 光油 | `Varnish` |
| RGB + W | `RgbWhite` |
| RGB + S | `RgbSupport` |
| RGB + V | `RgbVarnish` |
| RGB + S + W + V | `RgbSupportWhiteVarnish` |
| 材料占用 | `Occupancy` |
| 真实空白 | `Empty` |

加载 package 后默认选择 `RGB + S + W + V`，便于一次检查所有生产材料。显示默认值不改变
切片配置、生产 TIFF 或材料优先级。

### 4.2 状态

状态区必须显示：

```text
当前第 n/N 个 manifest 层；
真实 layerIndex；
zMm；
dpiX/dpiY；
当前生产预览模式；
cache hit/miss；
RGB/W/S/V/occupied/empty/multiMaterial 统计；
加载中、失败和稳定错误码。
```

当前层失败时清空旧图，不得继续显示上一层并伪装成功。

### 4.3 图例和颜色

W/S/V 调色板继续从 UI-only pseudoColors 读取，默认值与 13C-02 一致。调色板只影响
`MaterialPreviewRequest`，不得写回配置或 TIFF。图例显示 RGB、W、S、V、Empty，并提示
`black_is_print / 0=打印 / 255=不打印`。

## 5. 异步与生命周期

固定规则：

```text
UI 主线程不得同步解码 TIFF；
每次切包、切层均递增 generation；
快速滑层取消旧逻辑请求；
仅 consumerId、generation、layerIndex 都匹配时接受结果；
切换模式不发起新 TIFF 请求；
窗口析构先 Cancel，迟到结果不得访问已销毁 QWidget；
cache 命中仍通过同一结果路径更新 UI。
```

加载中禁用像素探针，但不必禁用层滑块；连续滑动只保留最新请求。

## 6. 坐标与像素探针

生产 buffer 使用 TIFF 原始坐标。QImage 显示沿用当前项目约定，只执行一次：

```text
displayX = rawX；
displayY = height - 1 - rawY。
```

鼠标探针反向映射：

```text
rawX = displayX；
rawY = height - 1 - displayY。
```

随后调用 `MaterialPreviewComposer::Probe`。探针显示精确 R/G/B/W/S/V 值、打印通道、真实
layerIndex 和 raw/display 坐标。禁止从 QImage 颜色反推材料。

## 7. 文件所有权

计划修改：

```text
apps/slicer_debug_ui/services/TiffLayerLoadWorker.h/.cpp；
apps/slicer_debug_ui/widgets/LayerPreviewPanel.h/.cpp；
apps/slicer_debug_ui/widgets/PreviewWorkspace.h/.cpp；
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
apps/slicer_debug_ui/CMakeLists.txt；
相关状态、任务和用户说明文档。
```

允许新增一个轻量 UI adapter：

```text
apps/slicer_debug_ui/services/MaterialPreviewImageAdapter.h/.cpp
```

其职责仅限：

```text
QColor/QImage 与核心 DTO 转换；
中文模式映射；
显示 Y 翻转。
```

不得在 adapter 中读取文件、决定生产材料或跨层兜底。

## 8. 自动化验证

新增 UI smoke：

```text
tiff-native-preview-all-materials
```

必须覆盖：

```text
只通过 manifest/layers 载入生产预览；
R/G/B/W/S/V、RGB 组合和 RGB+S+W+V 均可选；
首层/中间层/末层真实 layerIndex 与 zMm；
635/600 物理比例；
六通道探针与 core Probe 一致；
快速请求后只显示最新 generation；
切换模式不增加 TIFF decode；
错误层不跨层兜底；
一级入口为生产/诊断两个；
诊断 Overlay/Raw 仍可访问。
```

13C-03 的 smoke 可以使用仍含 preview 目录的现有 package，但生产断言必须验证图像来源为
`manifest/layers/*.tiff`。完全删除 preview 目录的独立 smoke 属于 13C-05。

最低验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui material_preview_composer_unit_tests tiff_layer_source_unit_tests tiff_layer_cache_unit_tests
ctest --test-dir build -C Debug -R "^(material_preview_composer|tiff_layer_(source|cache))_unit_tests$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case tiff-native-preview-all-materials --package output\UiSmokeOverlayRgbwv
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-workspace-shared-layer --package output\UiSmokeOverlayRgbwv
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check
```

## 9. 验收标准

```text
生产预览的全部模式只读取同层 TIFF；
模式切换复用当前 RgbwsvLayerBuffer；
真实 layerIndex/zMm/dpiX/dpiY 正确；
RGB+S+W+V 可见；
探针返回精确六通道值；
快速滑层无 stale 覆盖；
生产与诊断只有两个一级入口；
诊断缺失显示“未提供”，不从 TIFF 猜测；
现有 RIP、协议和旧诊断能力不回归；
新增 smoke 和全量 Gate 通过。
```

## 10. 非目标与停止条件

非目标：

```text
不关闭或删除 preview PNG；
不修改 preview 配置 schema；
不删除 PreviewOverlayPanel/PreviewPanel；
不实现 Texture Surface/Model Fill/Partition 推断；
不修改生产 package、TIFF 或 RIP；
不引入新第三方库。
```

以下任一发生即停止并记录：

```text
生产预览仍需 preview PNG 才能显示材料；
UI 主线程同步读取 TIFF；
发现跨层兜底；
stale 结果可覆盖当前层；
坐标翻转超过一次；
必须改变 p0.rgbwsv.2 才能继续。
```

`13C-03 PASS -> 12E-09A-05 解锁，同时 13C-04 READY`。
