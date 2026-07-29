# REPORT 13D-01 顶部作业栏当前状态

> 状态：COMPLETE / 13D-02 READY
> 日期：2026-07-29
> 范围：Qt 工作台顶部主作业动作

## 1. 完成内容

`SceneActionBar` 已从左侧滚动运行面板迁移到 `MainWindow` 中央根布局顶部，并保持唯一实例。当前固定
提供：

```text
导入模型；
保存场景；
传统切片 / Global Surface Shell 模式摘要；
当前 Profile 摘要；
切片当前场景；
取消当前切片；
场景 revision、可见实例数或运行状态。
```

## 2. 动作复用

顶部栏没有建立第二套业务：

```text
导入 -> MainWindow::OnImportModelPreview；
保存 -> MainWindow::OnSaveSceneTransform；
切片 -> MainWindow::OnSliceCurrentScene；
取消 -> SceneSliceActionController::Cancel；
状态 -> MainWindow::UpdateActionAvailability。
```

无模型时保存和切片禁用但不隐藏；切片运行中仅取消动作可用。模式/Profile 直接反映配置页当前选择，
切换模型、预览和配置页不会隐藏顶部栏。

## 3. 边界

本任务未修改 `slicer_core`、SceneDocument、切片引擎、RGBWSV TIFF 协议、PreviewWorkspace、
ContextInspector 或 DiagnosticsDock。

## 4. 验证

```text
Debug slicer_debug_ui 构建：PASS；
workbench-job-action-bar：PASS；
scene-batch-import-three：PASS；
scene-slice-current：PASS；
UI self-test：PASS。
```

## 5. 下一任务

13D-02 原子准备文档已完成，13D-01 Smoke Gate 已关闭。下一步可将模型列表、变换、排版、切片设置
和预检重组为唯一右侧 `ContextInspector`。
