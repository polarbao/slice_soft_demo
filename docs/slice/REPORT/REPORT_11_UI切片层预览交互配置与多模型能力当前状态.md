# REPORT_11_UI切片层预览交互配置与多模型能力当前状态

> 文档版本：v0.1
> 文档状态：Stage Report / 11
> 生成日期：2026-07-01
> 分支：`spike/09P-openvdb-experimental-pipeline`

---

## 1. 阶段目标

Stage 11 的目标是在不修改生产切片协议的前提下，把当前输出包变成 UI 可交互检查的工作流：

```text
LayerPreview 数据契约；
按层滑动预览；
RGB / W / S / V / occupancy / diagnostic 通道切换；
UI 伪彩显示；
常用配置交互编辑；
UI smoke 验证；
多模型能力边界评估。
```

本阶段明确不实现：

```text
多模型 production 输出；
复杂自动排版；
联合支撑优化；
RIP 半色调；
设备通信；
喷头 bitstream；
OpenVDB 默认生产启用；
p0.rgbwsv.2 协议修改。
```

---

## 2. 已完成任务

| Task | 状态 | 主要提交 |
|---|---|---|
| 11-0 阶段入口同步 | 已完成 | `6e18207 docs(11): 同步阶段入口` |
| 11-1 LayerPreview data contract | 已完成 | `18164be docs(11): 固化层预览数据契约` |
| 11-2 Layer slider / pseudo color viewer | 已完成 | `6d0ccfd feat(11): 增加层预览滑动与伪彩视图` |
| 11-3 UI layout refresh | 已完成 | `caea661 ui(11): 调整层预览主工作区布局` |
| 11-4 Interactive settings panel | 已完成 | `d25d101 feat(11): 增加常用配置交互面板` |
| 11-5 Multi-model capability decision | 已完成 | `63c1feb docs(11): 固化多模型能力边界决策` |
| 11-6 UI smoke / golden preview | 已完成 | `192ec02 test(11): 增加层预览 UI smoke` |
| 11-7 本报告 | 本轮完成 | `REPORT_11_UI切片层预览交互配置与多模型能力当前状态.md` |

---

## 3. 新增和关键修改

新增正式文档：

```text
docs/slice/DEV/DEV_11_LayerPreview_DataContract.md
docs/slice/DEV/DEV_11_MultiModel_CapabilityDecision.md
docs/slice/REPORT/REPORT_11_UI切片层预览交互配置与多模型能力当前状态.md
```

新增验证资产：

```text
tests/golden/expected/11_layer_preview_manifest_schema.json
samples/configs/ui_smoke/ui_layer_preview.json
```

新增 UI 代码：

```text
apps/slicer_debug_ui/services/LayerPreviewDataProvider.*
apps/slicer_debug_ui/widgets/LayerPreviewPanel.*
apps/slicer_debug_ui/widgets/QuickConfigPanel.*
```

关键修改：

```text
apps/slicer_debug_ui/MainWindow.*
apps/slicer_debug_ui/services/UiSmokeTestRunner.*
apps/slicer_debug_ui/services/ConfigValidator.cpp
apps/slicer_debug_ui/widgets/ConfigEditorPanel.*
apps/slicer_debug_ui/CMakeLists.txt
docs/codex_task/current/TASKS_11_UI切片层预览交互配置与多模型评估任务清单.md
```

---

## 4. 当前已实现能力

### 4.1 LayerPreview 数据契约

已定义 UI 派生契约：

```text
schema = p0.layer_preview_manifest.1
```

该契约由现有输出派生：

```text
manifest.json
reports/preview_report.json
reports/slice_report.json
reports/texture_report.json
reports/support_report.json
reports/material_policy_report.json
preview/*.png
layers/*.tiff
```

当前只作为 UI DTO / golden schema 使用，不写入 production package，不修改 `p0.rgbwsv.2`。

### 4.2 层预览 UI

已新增中心工作区“层预览”：

```text
按 package layerIndex 滑动；
显示当前层 z / RGB / W / S / V 统计；
切换 RGB / 纹理 RGB / W / S / V / occupancy / diagnostic；
支持适应窗口、1:1、放大、缩小和滚动区域平移；
W / S / V / occupancy / diagnostic 使用 UI 伪彩；
RGB / texture RGB 保持 true-color 预览。
```

伪彩默认：

```text
empty = [255, 255, 255]
support = [0, 255, 0]
white = [0, 170, 255]
varnish = [127, 127, 127]
diagnostic = [255, 180, 0]
```

### 4.3 UI 布局

已调整为：

```text
左侧：作业 / 路径 / 运行入口；
中心：层预览作为默认主工作区，报告 / 曲线紧随其后；
右侧：参数 / 诊断 / 工艺对比；
底部：运行日志。
```

旧 preview 和 overlay 仍保留为辅助调试页。

### 4.4 常用配置面板

新增“常用”配置页：

```text
模型文件；
输出目录；
层高；
纹理策略；
支撑启用；
白墨启用；
光油启用；
光油顶部层数；
预览开关；
预览间隔；
OpenVDB 实验开关。
```

写入方式：

```text
全部通过 ConfigDocument::setValue；
继续复用 dirty / validate / save / saveAs / revert；
显示当前配置 JSON 预览；
OpenVDB 启用时强制 writeProductionRgbwsv=false。
```

### 4.5 UI Smoke

新增：

```text
samples/configs/ui_smoke/ui_layer_preview.json
--ui-smoke-test --case layer-preview-load
```

该 smoke 会验证：

```text
输出包存在 manifest / preview；
LayerPreviewPanel 读取 layerCount；
通道包含 rgb / support / white / varnish / occupancy / diagnostic；
选择首层 / 中间层 / 末层；
逐通道切换并确认渲染图非空。
```

---

## 5. 生产协议符合情况

Stage 11 保持 Stage 10 固化协议：

```text
manifest.schema = p0.rgbwsv.2
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
```

Stage 11 新增的 LayerPreview / QuickConfig / UI smoke 均不修改生产 TIFF、manifest 协议、channel order 或 RIP 边界。

---

## 6. 多模型当前判断

已固化到：

```text
docs/slice/DEV/DEV_11_MultiModel_CapabilityDecision.md
docs/slice/DOC/DOC_DECISION_11_多模型切片处理范围决策.md
```

结论：

```text
Stage 11 不默认启用多模型 production 输出；
modelId 标识模型资源来源；
instanceId 标识一次摆放实例；
transform 使用 translateMm / rotateDeg / scale；
resourceScope 隔离 OBJ / MTL / texture / 3MF package 内部资源；
productionEligible 默认 false；
recommendedPath = sequential_first。
```

后续如果进入多模型，应优先评估：

```text
顺序切片 orchestration；
per-model / per-instance report；
package metadata 扩展；
build volume / placement / nesting；
联合切片和联合支撑作为更后阶段。
```

---

## 7. 实际验证结果

本阶段实际运行过：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_layer_preview.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
git diff --check
```

结果：

```text
Debug build 通过；
ctest 5/5 通过；
slicer_debug_ui target 构建通过；
self-test 通过：startup / experimental-report-summary；
ui_layer_preview 配置生成 output/UiSmokeLayerPreview 成功；
layer-preview-load smoke 通过，25 层，通道 rgb,white,support,varnish,occupancy,diagnostic；
git diff --check 通过，仅有 Windows LF/CRLF 工作区提示。
```

备注：

```text
output/UiSmokeLayerPreview 是本地验证产物，不提交到仓库。
```

---

## 8. 当前未完成和风险

```text
LayerPreviewManifest 仍是 UI 派生契约，尚未作为 package 内独立 report 写出；
诊断 overlay 当前没有逐像素 mask，只有 report badge / 边框级提示；
QuickConfigPanel 已能编辑常用字段，但尚未做逐字段 UI 自动化编辑 smoke；
配置 JSON 预览目前是当前 document pretty JSON，不是 slicer_core normalized config 输出；
多模型只完成能力边界和数据模型决策，没有实现 production 输出；
未做真实人工截图验收，当前 UI 验证以 Qt smoke 和构建为主。
```

---

## 9. 是否可以进入下一阶段

建议可以进入下一阶段，但建议先选择一个小收口任务：

```text
11R：补 QuickConfigPanel 字段级 smoke、LayerPreviewManifest report 写出和诊断 overlay 小收口；
或 12：进入多模型顺序切片 orchestration 预研；
或 10R：补 texture_fidelity_summary.json / downstream package summary。
```

推荐优先级：

```text
P0：11R UI smoke 小收口；
P1：顺序切片 orchestration 的多模型 pre-production design；
P1：LayerPreviewManifest 物化为 reports/layer_preview_manifest.json；
P2：diagnostic overlay 逐像素 mask；
P2：联合切片 / placement / nesting 独立阶段。
```
