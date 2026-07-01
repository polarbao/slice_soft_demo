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

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

---

## 5. Task 11-2：Layer slider / pseudo color viewer

目标：

```text
UI 加载 layer preview；
滑动显示每层；
切换 RGB / W / S / V / occupancy / diagnostic 伪彩视图；
支持基础缩放和平移。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 6. Task 11-3：UI layout refresh

目标：

```text
调整作业区、预览区、参数区、报告区布局；
避免卡片嵌套和调试信息堆叠；
让 layer preview 成为 UI 主工作区。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 7. Task 11-4：Interactive settings panel

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

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

---

## 8. Task 11-5：Multi-model capability decision

目标：

```text
评估多模型导入、排版、联合切片、顺序切片的影响；
定义 modelId / instanceId / transform / resource scope；
输出是否进入实现的决策；
不默认实现 production 多模型输出。
```

验证：

```powershell
git diff --check
```

---

## 9. Task 11-6：UI smoke / golden preview

目标：

```text
建立 layer preview fixture；
建立 slider / channel switch / config panel smoke；
必要时增加 golden preview manifest。
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

目标：

```text
生成 docs/slice/REPORT/REPORT_11_UI切片层预览交互配置与多模型能力当前状态.md；
记录已完成能力、验证命令、未完成风险、多模型后续判断。
```

验证：

```powershell
git status --short
git diff --check
```

