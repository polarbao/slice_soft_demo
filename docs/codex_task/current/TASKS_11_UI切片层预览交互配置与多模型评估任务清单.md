# TASKS_11_UI切片层预览交互配置与多模型评估任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List / Stage 11
> 生成日期：2026-06-30
> 阶段定位：UI layer preview / interactive config / multi-model capability decision

---

## 1. 总规则

每次只执行用户明确指定的一个任务。

每个任务开始前：

```powershell
git status --short
```

每个任务完成前：

```powershell
git status --short
git diff --check
```

生产安全规则：

```text
不修改 p0.rgbwsv.2；
不默认启用 OpenVDB；
不让 UI 直接依赖 OpenVDB 类型；
不让 UI 直接访问 slicer.cpp 内部临时结构；
不实现 RIP 半色调、设备通信或喷头 bitstream；
不默认启用多模型 production 输出。
```

---

## 2. 推荐执行顺序

```text
11-0：更新路线图和阶段文档入口
11-1：固化 LayerPreview data contract
11-2：实现 layer slider / pseudo color viewer
11-3：调整 UI 布局为作业式工作台
11-4：新增 interactive settings panel
11-5：多模型能力评估与决策
11-6：UI smoke / golden preview 验证
11-7：生成 REPORT_11
```

---

## 3. Task 11-0：阶段文档入口

状态：本轮完成，提交见 `docs(11): 同步阶段入口`。

目标：

```text
新增 PRD / DEV / DEMO / DOC_DECISION / TASKS / CODEX_PROMPT；
更新 formal roadmap；
明确当前只新增 11 一个阶段；
明确多模型先评估，不新增 12 阶段。
```

验证：

```powershell
git diff --check
```

---

## 4. Task 11-1：LayerPreview data contract

状态：本轮完成，提交见 `docs(11): 固化层预览数据契约`。

目标：

```text
定义 LayerPreviewManifest；
定义 LayerPreviewFrame；
定义 LayerPreviewChannel；
定义 LayerPreviewStats；
定义 PseudoColorMap；
定义 diagnostic overlay 字段。
```

建议输出：

```text
docs/slice/DEV/DEV_11_LayerPreview_DataContract.md
tests/golden/expected/11_layer_preview_manifest_schema.json
```

已新增：

```text
docs/slice/DEV/DEV_11_LayerPreview_DataContract.md
tests/golden/expected/11_layer_preview_manifest_schema.json
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

---

## 5. Task 11-2：Layer slider / pseudo color viewer

状态：本轮完成，提交见 `feat(11): 增加层预览滑动与伪彩视图`。

目标：

```text
UI 加载 layer preview；
滑动显示每层；
切换 RGB / W / S / V / occupancy / diagnostic 伪彩视图；
支持基础缩放和平移。
```

已实现：

```text
新增 LayerPreviewDataProvider，从 manifest / preview_report / slice_report / preview 文件派生 UI 数据；
新增 LayerPreviewPanel，按 layerIndex 滑动并切换 RGB / 纹理 RGB / W / S / V / occupancy / diagnostic；
W / S / V / occupancy / diagnostic 使用 UI 伪彩渲染，preview PNG 不作为生产数据；
主窗口新增“层预览”页，保留旧预览与叠加预览页。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 6. Task 11-3：UI layout refresh

状态：本轮完成，提交见 `ui(11): 调整层预览主工作区布局`。

目标：

```text
调整作业区、预览区、参数区、报告区布局；
避免卡片嵌套和调试信息堆叠；
让 layer preview 成为 UI 主工作区。
```

已实现：

```text
层预览作为中心区第一个 Tab 和默认当前页；
报告 / 曲线紧随层预览，配置与旧预览退到后续 Tab；
左侧作业区、中心预览区、右侧参数诊断区设置最小宽度与 splitter 初始比例；
右侧 Tab 文案收敛为“参数 / 诊断 / 工艺对比”，减少调试信息堆叠。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 7. Task 11-4：Interactive settings panel

状态：本轮完成，提交见 `feat(11): 增加常用配置交互面板`。

目标：

```text
把常用配置迁移到 UI 控件；
通过 config DTO / validator 写入；
显示 normalized config 和错误信息。
```

首批配置：

```text
layer height；
material profile；
texture application policy；
support enable；
white / varnish policy；
OpenVDB experimental enable；
output directory；
preview generation。
```

已实现：

```text
新增“常用”配置页，覆盖模型路径、输出目录、层高、纹理策略、支撑启用、白墨启用、光油启用、光油顶部层数、预览开关、预览间隔、OpenVDB 实验开关；
所有字段通过 ConfigDocument::setValue 写入，继续复用 dirty / validation / save / saveAs；
OpenVDB 实验开关写入 diagnostic_only / writeProductionRgbwsv=false，保持非生产边界；
新增当前配置 JSON 预览，并补充 layerThicknessMm 与 OpenVDB production 写入的 UI 侧校验。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 8. Task 11-5：Multi-model capability decision

状态：本轮完成，提交见 `docs(11): 固化多模型能力边界决策`。

目标：

```text
评估多模型导入、排版、联合切片、顺序切片的影响；
定义 modelId / instanceId / transform / resource scope；
输出是否进入实现的决策；
不默认实现 production 多模型输出。
```

已完成：

```text
新增 DEV_11_MultiModel_CapabilityDecision.md；
明确 MultiModelScene / SceneModelSource / ModelInstance / ResourceScope / CapabilityReport；
结论为 sequential_first 优先评估，joint slicing 留待后续 build volume / placement / nesting / package metadata 阶段；
production 多模型输出默认禁用。
```

验证：

```powershell
git diff --check
```

---

## 9. Task 11-6：UI smoke / golden preview

状态：本轮完成，提交见 `test(11): 增加层预览 UI smoke`。

目标：

```text
建立 layer preview fixture；
建立 slider / channel switch / config panel smoke；
必要时增加 golden preview manifest。
```

已实现：

```text
新增 samples/configs/ui_smoke/ui_layer_preview.json，用于生成 output/UiSmokeLayerPreview；
新增 layer-preview-load UI smoke case，真实加载 LayerPreviewPanel，选择首层 / 中间层 / 末层并切换 RGB / S / W / V / occupancy / diagnostic；
保留 11-1 golden schema，当前不新增 production 输出契约。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
git diff --check
```

---

## 10. Task 11-7：REPORT_11

状态：本轮完成，提交见 `docs(11): 生成阶段状态报告`。

目标：

```text
生成 docs/slice/REPORT/REPORT_11_UI切片层预览交互配置与多模型能力当前状态.md；
记录已完成能力、验证命令、未完成风险、多模型后续判断。
```

已完成：

```text
新增 REPORT_11_UI切片层预览交互配置与多模型能力当前状态.md；
更新 docs/slice/README.md 和 docs/slice/REPORT/README.md；
记录 Stage 11 已完成任务、验证命令、未完成风险和下一阶段建议。
```

验证：

```powershell
git status --short
git diff --check
```
