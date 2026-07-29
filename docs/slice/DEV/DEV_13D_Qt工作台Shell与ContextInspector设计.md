# DEV 13D Qt 工作台 Shell 与 ContextInspector 设计

> 文档版本：v1.1
> 文档状态：APPROVED / 13D-01 COMPLETE / 13D-02 READY
> 日期：2026-07-28

## 1. 设计原则

采用 `wrap first, move later, rewrite last`：先把现有 QWidget 放入稳定容器，再迁移布局和导航，最后
只删除确认重复的壳层代码。核心业务对象、信号语义和生产协议不因 UI 重排改变。

## 2. 目标组件

```text
MainWindow
  -> JobActionBar
  -> ProjectToolsDock
  -> CentralWorkspace
      -> ModelWorkspace
      -> PreviewWorkspace
      -> ConfigWorkspace
  -> ContextInspector
      -> ScenePage
      -> TransformPage
      -> LayoutPage
      -> SliceSettingsPage
      -> PreflightPage
  -> DiagnosticsDock
      -> Report
      -> MaterialClosure
      -> Curves
      -> ProcessCompare
      -> Log
```
`JobActionBar` 复用 13B-08 的 `SceneActionBar`，避免第二套主动作。

## 3. 组件职责

### 3.1 MainWindow

只负责：

```text
组装容器；
连接顶层导航；
保存/恢复窗口布局；
根据当前 workspace 和 selection 选择检查器上下文；
不承载变换、排版、预检或切片业务判断。
```

### 3.2 ContextInspector

建议新增：

```text
apps/slicer_debug_ui/widgets/ContextInspector.h/.cpp
```

接口输入为稳定 UI context DTO：

```text
workspaceKind；
selectionKind；
sceneId/instanceId；
sceneRevision/transformRevision；
availability/blockedReason；
```

现有 `ModelListPanel`、`ModelTransformPanel`、`SceneLayoutPanel` 和预检 widget 以页面方式复用，不复制
其业务实现。

### 3.3 Dock 与状态持久化

使用 Qt 原生 `QDockWidget/QSplitter/QSettings`：

```text
保存 geometry、dock state、splitter sizes、last inspector page；
状态 schema 带版本，如 ui.layout.version=13d.1；
旧版本或非法尺寸回退到安全默认布局；
不得把绝对屏幕坐标作为业务配置写入 slice config。
```

## 4. 迁移策略

### Phase 1 顶部动作

把 13B-08 `SceneActionBar` 挂到顶部稳定区域，旧左侧主动作隐藏到高级工具但暂不删除。

### Phase 2 单一检查器

将模型列表/变换/排版和参数/预检迁入一个 `ContextInspector`。迁移期间使用 adapter 复用现有信号；
确认 smoke 后再移除旧双栏容器。

### Phase 3 Dock 收口

把右侧诊断/工艺对比页移入现有 `DiagnosticsDock`，确保报告、材料闭环、曲线和日志顺序稳定。

### Phase 4 尺寸与可访问性

冻结最小尺寸、滚动策略、中文文本、tab order、快捷键和布局恢复。

## 5. 信号连接

所有自定义信号/槽继续遵循 `Sig*` / `On*`，并使用函数指针连接。例如：

```cpp
connect(m_contextInspector,
        &ContextInspector::SigRequestedInstanceSelection,
        this,
        &MainWindow::OnRequestedInstanceSelection);
```

不得通过字符串 SIGNAL/SLOT 宏迁移旧连接。

## 6. 文件计划

计划新增：

```text
apps/slicer_debug_ui/widgets/JobActionBar.*（若 13B-08 未固定 SceneActionBar 名称则统一重命名）；
apps/slicer_debug_ui/widgets/ContextInspector.*；
apps/slicer_debug_ui/widgets/ProjectToolsDock.*；
apps/slicer_debug_ui/services/WorkspaceLayoutState.*。
```

计划修改：

```text
apps/slicer_debug_ui/MainWindow.*；
apps/slicer_debug_ui/widgets/DiagnosticsDock.*；
apps/slicer_debug_ui/widgets/ModelWorkspace.*；
apps/slicer_debug_ui/widgets/PreviewWorkspace.*；
apps/slicer_debug_ui/services/UiSmokeTestRunner.*；
apps/slicer_debug_ui/CMakeLists.txt。
```

## 7. 风险与控制

| 风险 | 控制 |
|---|---|
| 一次重排破坏大量信号 | 分四个原子任务，先包装后迁移 |
| 13C 预览入口再次变化 | 13D 开发等待 13C-05 |
| 隐藏高级功能后不可发现 | “项目与高级工具”固定入口、用户手册列出 |
| 布局状态导致窗口不可恢复 | 版本化 QSettings 和安全默认布局 |
| 小屏滚动条套滚动条 | 每页独立滚动，顶层 Dock 不套全局 ScrollArea |
| 报告能力误删 | 迁移前后功能清单和 UI Smoke 对账 |

## 8. 验证

```text
layout-default；
layout-1280x720；
layout-150-percent-scale；
context-inspector-selection；
diagnostics-dock-all-tabs；
layout-save-restore-invalid-state；
13B-08 scene workflow；
13C TIFF native preview；
现有 self-test 和 Quick CI。
```
