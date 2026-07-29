# DOC PREP 13D-01 顶部作业栏准备

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.0
> 日期：2026-07-29

## 1. 任务目标

复用唯一 `SceneActionBar`，将其从左侧运行面板迁移到 `MainWindow` 顶部稳定区域。顶部栏固定提供：

```text
导入模型；
保存场景；
切片模式/Profile 摘要；
切片当前场景；
取消；
当前作业状态。
```

13D-01 不迁移右侧检查器、项目区或诊断 Dock，也不修改切片核心、SceneDocument 和 RGBWSV 协议。

## 2. 当前代码事实

```text
SceneActionBar 已拥有切片、取消和状态控件；
MainWindow::createRunPanel() 当前创建并承载 SceneActionBar；
导入模型由 OnImportModelPreview() 处理；
保存场景由 OnSaveSceneTransform() 处理；
切片状态由 UpdateActionAvailability() 汇总；
模式由 ConfigEditorPanel::SelectedProductionMode() 提供；
Profile 由 m_currentProfileId 提供。
```

## 3. 实现合同

```text
MainWindow 只创建一个 SceneActionBar；
SceneActionBar 由根 QVBoxLayout 在 mainSplitter 上方承载；
顶部栏发送 SigImportRequested/SigSaveRequested/SigSliceRequested/SigCancelRequested；
导入、保存、切片和取消继续连接既有槽或 controller；
无模型时保存和切片禁用但不隐藏，tooltip 说明原因；
运行中仅取消可用，导入、保存和切片禁用；
切换模型/预览/配置页不得改变顶部栏可见性；
左侧旧主动作降为高级兼容入口，不复制新的 SceneActionBar。
```

## 4. 状态输入

`SetPresentation` 输入扩展为：

```text
canImport；
canSave；
canSlice；
canCancel；
modeLabel；
profileLabel；
status；
reason。
```

模式中文显示为“传统切片”或“Global Surface Shell”；Profile 为空时显示“自定义”。

## 5. 验收与 Smoke

新增 `workbench-job-action-bar`：

```text
顶部栏是 mainSplitter 的兄弟节点而不是左侧滚动区后代；
导入、保存、切片、取消、模式/Profile 和状态控件全部存在；
空场景时导入可用，保存/切片/取消禁用；
具备可切片场景时保存/切片可用；
切换三个中央页面后顶部栏仍可见；
既有 scene-batch-import-three、scene-slice-current 和 UI self-test 通过。
```

## 6. 文件边界

允许修改：

```text
apps/slicer_debug_ui/widgets/SceneActionBar.h/.cpp；
apps/slicer_debug_ui/MainWindow.h/.cpp；
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp；
13D TASKS、REPORT 和索引文档。
```

禁止修改：

```text
src/slicer_core；
RGBWSV writer/reader；
PreviewWorkspace 数据源；
ContextInspector 和 DiagnosticsDock 布局。
```
