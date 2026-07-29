# DEMO 13D Qt 工作台交互验证方案

> 文档版本：v1.1
> 文档状态：APPROVED / 13D-01..02 COMPLETE / 13D-03 READY
> 日期：2026-07-28

## 1. 目标

证明工作台重排后，主作业流更短、两个右侧区域已合为单一上下文检查器，并且所有原有调试能力仍可
到达。

## 2. 核心场景

### Case 13D-01 默认工作台

```text
启动后顶部可见“导入模型”“保存场景”和“切片当前场景”；
顶部显示当前模式/Profile、取消动作和作业状态；
中央默认进入模型页，切换模型/预览/配置页后顶部栏始终可见；
主按钮无模型时禁用原因明确。
```

本 Case 只作为 `13D-01` 验收；单一检查器、项目区和诊断 Dock 分别由
`13D-02/03` 验收，不得提前计入 `13D-01 PASS`。
### Case 13D-02 三模型编辑

批量导入三个带纹理模型，选择不同实例，在场景/变换/排版/预检页间切换，确认画布不跳动、当前
instance identity 不串、中文标签完整显示。

实现结果：单一 `ContextInspector` 已接入场景、变换、排版、切片设置和预检；13D-03 前保留的
参数/诊断/工艺对比收在同一检查器的“高级诊断”临时页，不再形成第二个右侧栏。

### Case 13D-03 切片与结果检查

点击顶部主动作，查看进度摘要；成功后进入生产预览。报告、材料闭环、曲线、工艺对比和日志都可从
底部 Dock 打开，右侧不出现第二套诊断栏。

### Case 13D-04 折叠与恢复

分别折叠项目区、检查器和诊断 Dock，重启应用后恢复。注入旧/非法布局状态时回退安全默认值。

### Case 13D-05 分辨率与缩放

在 1280x720、1440x900、1920x1080 和 150% 缩放运行截图检查：

```text
主按钮不被遮挡；
中央画布可操作；
最长中文文本不越界；
没有重叠和不可达控件；
滚动条只出现在需要的检查页。
```

## 3. 自动化计划

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workbench-default-layout
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workbench-context-inspector
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workbench-layout-restore
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case workbench-1280x720
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
```

命令在实现前仅是计划入口，不宣称当前存在或通过。

## 4. 通过标准

```text
从导入三个模型到点击场景切片不需要进入高级工具；
截图中的两个右侧区域不再同时常驻；
所有旧报告/诊断/对比/日志能力仍可到达；
布局折叠和恢复确定性；
四种视口无关键遮挡；
13B-08、13C 和既有 UI Smoke 不回归。
```
