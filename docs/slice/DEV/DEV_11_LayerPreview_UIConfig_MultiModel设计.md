# DEV_11_LayerPreview_UIConfig_MultiModel设计

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 11
> 生成日期：2026-06-30
> 阶段定位：Layer Preview、UI Config、Multi-Model Capability

---

## 1. 技术目标

11 阶段不改变切片协议主线，而是在现有输出、report、preview、package 之上建立 UI 可消费的数据契约。

核心原则：

```text
UI 读取稳定 data contract；
UI 不直接访问 slicer.cpp 内部临时结构；
UI 不直接依赖 OpenVDB 类型；
slicer_core 不依赖 Qt；
p0.rgbwsv.2 不改变；
多模型先做数据模型和最小能力评估。
```

---

## 2. Layer Preview Data Contract

建议新增或固化以下概念：

```text
LayerPreviewManifest
LayerPreviewFrame
LayerPreviewChannel
LayerPreviewStats
PseudoColorMap
LayerDiagnosticOverlay
```

最小字段：

```text
schema
packagePath
layerCount
zMin / zMax
layerHeight
resolution
availableChannels
frames[]
diagnostics[]
```

单层帧字段：

```text
layerIndex
z
width
height
source
channels
stats
thumbnailPath
optionalRawPath
```

---

## 3. UI 模块边界

建议 UI 分层：

```text
LayerPreviewDataProvider
LayerPreviewModel
LayerPreviewView
LayerPreviewController
PseudoColorMapper
ConfigEditorPanel
JobWorkspacePanel
ReportDiagnosticsPanel
```

依赖方向：

```text
slicer_debug_ui -> process/package/report/preview services
slicer_debug_ui -> Qt widgets
slicer_core -> STL/domain DTO only
```

禁止：

```text
slicer_core -> Qt
UI -> OpenVDB internal types
UI -> slicer.cpp temporary structs
UI -> direct production admission override
```

---

## 4. 伪彩策略

伪彩映射建议从 UI 层配置，不写入生产输出。

首批 palette：

```text
grayscale
rgb_composite
white_heat
support_mask
varnish_mask
occupancy
diagnostic_overlay
texture_fidelity
```

通道解释：

```text
RGB：纹理颜色预览；
W：白墨强度；
S：支撑区域；
V：光油区域；
occupancy：模型占用；
diagnostic：fallback、missing texture、blocked admission 等标记。
```

---

## 5. 交互配置设计

配置 UI 不应直接拼写临时 JSON。建议通过 config DTO / normalized config / validation result 进行读写。

首批 UI 控件：

```text
segmented control：texture application policy；
checkbox：support enable / preview generation；
slider/spinbox：layer height / shell thickness；
combo box：material profile / output profile；
path picker：input model / output directory；
toggle：experimental OpenVDB enable，默认关闭并显示实验标记；
button：validate config / run slice / open output。
```

配置保存：

```text
UI draft config
validate
normalized config
write profile or launch CLI
report validation errors
```

---

## 6. 多模型能力评估

11 阶段先定义数据模型，不直接承诺完整生产能力。

候选数据结构：

```text
SceneModel
ModelInstance
ModelTransform
ModelResourceSet
ModelMaterialBinding
ModelSliceStats
```

必须回答：

```text
单 package 是否包含多个 modelId？
多模型是否共享 material profile？
同名纹理资源如何隔离？
模型之间是否做碰撞/重叠诊断？
多模型支撑是否联合计算？
输出 layer stats 如何按 modelId 拆分？
UI 如何选择、隐藏、锁定、变换模型？
```

推荐 11 阶段最小结论：

```text
允许 UI 和文档定义多模型数据模型；
允许导入多模型列表作为 experimental scene；
不默认允许多模型 production 输出；
不实现复杂自动排版；
不实现跨模型支撑联合优化。
```

---

## 7. 验证设计

建议新增验证：

```text
LayerPreviewManifest schema test
PseudoColorMapper unit test
UI self-test layer preview fixture
UI smoke: load package -> move layer slider -> switch channel
Config panel validation smoke
Multi-model decision fixture / report only
```

推荐命令：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
git diff --check
```

