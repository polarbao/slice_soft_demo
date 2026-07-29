# REPORT 13D-03 项目区与诊断 Dock 当前状态

> 状态：COMPLETE / 13D-04 READY
> 日期：2026-07-29

## 1. 目标

将长期占据首屏的仓库路径、构建、旧单模型兼容、OpenVDB、RIP 和回归入口迁入默认收起的项目区，
并把参数、诊断和工艺对比统一迁入底部诊断 Dock。

## 2. 已实现

```text
新增 ProjectToolsDock，固定为左侧可折叠 Dock，默认隐藏；
中央 mainSplitter 从项目/中央/右侧三列收敛为中央工作区 + ContextInspector 两列；
视图菜单提供“项目与高级工具”和“诊断区域”两个明确入口；
ContextInspector 删除临时“高级诊断”，最终只保留场景/变换/排版/切片设置/预检；
DiagnosticsDock 统一承载报告/材料闭环/曲线/材料参数/诊断/工艺对比/日志；
旧单模型、OpenVDB、RIP、构建和快速回归能力未删除，命令参数和输出路径未改变。
```

## 3. 影响边界

本任务只改变 Qt 工作台容器、导航与测试，不修改 `SceneDocument`、切片核心、RGBWSV TIFF、Package、
RIP 或生产准入策略。

## 4. 验证

```text
Debug slicer_debug_ui build：PASS；
workbench-project-diagnostics：PASS；
workbench-context-inspector：PASS；
workbench-job-action-bar：PASS；
diagnostics-collapse：PASS；
workspace-layout-sizes：PASS（1440x900、1280x720、1024x768）；
multi-model-list：PASS；
scene-grid-layout：PASS；
scene-batch-import-three：PASS；
scene-slice-current：PASS；
UI self-test：PASS；
git diff --check：PASS。
```

`diagnostics-collapse` 使用 `output/benchmarks/13c_04/diagnostics/package`；`workspace-layout-sizes`
使用 `output/benchmarks/12e_08d_04_global_production/xiao_ma/package`。

## 5. 后续

13D-04 准备文档已完成且前序 Gate 已关闭。下一步实现版本化布局持久化、安全默认恢复、中文长文本、
键盘焦点和 1280x720/高 DPI 收口。
